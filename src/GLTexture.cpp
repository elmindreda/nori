///////////////////////////////////////////////////////////////////////
// Wendy OpenGL library
// Copyright (c) 2004 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any
// damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any
// purpose, including commercial applications, and to alter it and
// redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you
//     must not claim that you wrote the original software. If you use
//     this software in a product, an acknowledgment in the product
//     documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and
//     must not be misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source
//     distribution.
//
///////////////////////////////////////////////////////////////////////

#include <moira/Moira.h>

#include <wendy/Config.h>
#include <wendy/OpenGL.h>
#include <wendy/GLContext.h>
#include <wendy/GLLight.h>
#include <wendy/GLShader.h>
#include <wendy/GLTexture.h>

///////////////////////////////////////////////////////////////////////

namespace wendy
{
  namespace GL
  {
  
///////////////////////////////////////////////////////////////////////

using namespace moira;

///////////////////////////////////////////////////////////////////////

namespace
{

unsigned int getClosestPower(unsigned int value, unsigned int maximum)
{
  unsigned int result;

  if (value > maximum)
    result = maximum;
  else
    result = value;

  if (result & (result - 1))
  {
    // value is not power of two
    // find largest power lesser than maximum

    unsigned int i;

    for (i = 0;  result & ~1;  i++)
      result >>= 1;

    result = 1 << i;
  }

  if ((result << 1) > maximum)
  {
    // maximum is not power of two, so we give up here
    return result;
  }

  if (value > result)
  {
    // there is room to preserve all detail, so use it
    return result << 1;
  }

  return result;
}

ImageFormat::Type getConversionFormat(const ImageFormat& format)
{
  switch (format)
  {
    case ImageFormat::RGBX8888:
      return ImageFormat::RGBA8888;
    default:
      return format;
  }
}

GLint unmipmapMinFilter(GLint minFilter)
{
  if (minFilter == GL_NEAREST_MIPMAP_NEAREST ||
      minFilter == GL_NEAREST_MIPMAP_LINEAR)
    return GL_NEAREST;
  else if (minFilter == GL_LINEAR_MIPMAP_NEAREST ||
	   minFilter == GL_LINEAR_MIPMAP_LINEAR)
    return GL_LINEAR;

  return minFilter;
}

Mapper<ImageFormat::Type, GLenum> formatMap;

Mapper<ImageFormat::Type, GLenum> genericFormatMap;

Mapper<ShaderUniform::Type, GLenum> samplerTypeMap;

}

///////////////////////////////////////////////////////////////////////

Texture::~Texture(void)
{
  if (textureID)
    glDeleteTextures(1, &textureID);
}

bool Texture::copyFrom(const Image& source,
                       unsigned int x,
		       unsigned int y,
		       unsigned int level)
{
  Image final = source;
  final.convert(format);

  // Moira has y-axis down, OpenGL has y-axis up
  final.flipHorizontal();

  if (textureTarget == GL_TEXTURE_1D)
  {
    if (final.getDimensionCount() > 1)
    {
      Log::writeError("Cannot blt to texture; source image has too many dimensions");
      return false;
    }

    glPushAttrib(GL_TEXTURE_BIT | GL_PIXEL_MODE_BIT);
    glBindTexture(textureTarget, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage1D(textureTarget,
                    level,
		    x,
		    final.getWidth(),
                    genericFormatMap[format],
                    GL_UNSIGNED_BYTE,
		    final.getPixels());

    glPopAttrib();
  }
  else
  {
    if (final.getDimensionCount() > 2)
    {
      Log::writeError("Cannot blt to texture; source image has too many dimensions");
      return false;
    }

    glPushAttrib(GL_TEXTURE_BIT | GL_PIXEL_MODE_BIT);
    glBindTexture(textureTarget, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage2D(textureTarget,
                    level,
		    x, y,
		    final.getWidth(), final.getHeight(),
                    genericFormatMap[final.getFormat()],
                    GL_UNSIGNED_BYTE,
		    final.getPixels());

    glPopAttrib();
  }

#if _DEBUG
  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
  {
    Log::writeError("Error during texture image blt: %s", gluErrorString(error));
    return false;
  }
#endif
  
  return true;
}

GLenum Texture::getTarget(void) const
{
  return textureTarget;
}

unsigned int Texture::getSourceWidth(unsigned int level) const
{
  return sourceWidth >> level;
}

unsigned int Texture::getSourceHeight(unsigned int level) const
{
  return sourceHeight >> level;
}

unsigned int Texture::getSourceDepth(unsigned int level) const
{
  return sourceDepth >> level;
}

unsigned int Texture::getPhysicalWidth(unsigned int level) const
{
  return physicalWidth >> level;
}

unsigned int Texture::getPhysicalHeight(unsigned int level) const
{
  return physicalHeight >> level;
}

unsigned int Texture::getPhysicalDepth(unsigned int level) const
{
  return physicalDepth >> level;
}

unsigned int Texture::getLevelCount(void) const
{
  return levelCount;
}

unsigned int Texture::getFlags(void) const
{
  return flags;
}

const ImageFormat& Texture::getFormat(void) const
{
  return format;
}

Image* Texture::getImage(unsigned int level) const
{
  if (getPhysicalWidth(level) == 0 || getPhysicalHeight(level) == 0)
  {
    Log::writeError("Cannot retrieve image for non-existent level %u", level);
    return NULL;
  }

  Ptr<Image> result = new Image(format,
                                getPhysicalWidth(level),
				getPhysicalHeight(level));

  glPushAttrib(GL_TEXTURE_BIT | GL_PIXEL_MODE_BIT);
  glBindTexture(textureTarget, textureID);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);

  glGetTexImage(textureTarget,
                level,
		genericFormatMap[format],
		GL_UNSIGNED_BYTE,
		result->getPixels());
  
  glPopAttrib();

#if _DEBUG
  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
  {
    Log::writeError("Error during texture image retrieval: %s", gluErrorString(error));
    return NULL;
  }
#endif

  result->flipHorizontal();

  return result.detachObject();
}

Texture* Texture::readInstance(const String& name, unsigned int flags)
{
  if (Texture* texture = findInstance(name))
  {
    if (texture->getFlags() != flags)
      throw Exception("Flags differ for cached texture");

    return texture;
  }

  if (Image* image = Image::findInstance(name))
    return createInstance(*image, flags, name);

  Ptr<Image> image = Image::readInstance(name);
  if (!image)
    return NULL;

  return createInstance(*image, flags, name);
}

Texture* Texture::readInstance(const Path& path,
			       unsigned int flags,
			       const String& name)
{
  Ptr<Image> image = Image::readInstance(path);
  if (!image)
    return NULL;

  return createInstance(*image, flags, name);
}

Texture* Texture::readInstance(Stream& stream,
			       unsigned int flags,
			       const String& name)
{
  Ptr<Image> image = Image::readInstance(stream);
  if (!image)
    return NULL;

  return createInstance(*image, flags, name);
}

Texture* Texture::createInstance(const Image& image,
				 unsigned int flags,
				 const String& name)
{
  Ptr<Texture> texture = new Texture(name);
  if (!texture->init(image, flags))
    return NULL;

  return texture.detachObject();
}

Texture::Texture(const String& name):
  Resource<Texture>(name),
  textureTarget(0),
  textureID(0),
  minFilter(0),
  magFilter(0),
  sourceWidth(0),
  sourceHeight(0),
  sourceDepth(0),
  physicalWidth(0),
  physicalHeight(0),
  physicalDepth(0),
  levelCount(0),
  flags(0)
{
}

bool Texture::init(const Image& image, unsigned int initFlags)
{
  if (!Context::get())
  {
    Log::writeError("Cannot create texture without OpenGL context");
    return false;
  }

  if (formatMap.isEmpty())
  {
    formatMap[ImageFormat::ALPHA8] = GL_ALPHA8;
    formatMap[ImageFormat::GREY8] = GL_LUMINANCE8;
    formatMap[ImageFormat::GREYALPHA88] = GL_LUMINANCE8_ALPHA8;
    formatMap[ImageFormat::RGB888] = GL_RGB8;
    formatMap[ImageFormat::RGBA8888] = GL_RGBA8;

    formatMap.setDefaults(ImageFormat::INVALID, 0);
  }

  if (genericFormatMap.isEmpty())
  {
    genericFormatMap[ImageFormat::ALPHA8] = GL_ALPHA;
    genericFormatMap[ImageFormat::GREY8] = GL_LUMINANCE;
    genericFormatMap[ImageFormat::GREYALPHA88] = GL_LUMINANCE_ALPHA;
    genericFormatMap[ImageFormat::RGB888] = GL_RGB;
    genericFormatMap[ImageFormat::RGBA8888] = GL_RGBA;

    genericFormatMap.setDefaults(ImageFormat::INVALID, 0);
  }

  flags = initFlags;

  // Figure out which texture target to use

  if (image.getDimensionCount() == 1)
    textureTarget = GL_TEXTURE_1D;
  else if (image.getDimensionCount() == 2)
  {
    if (flags & RECTANGULAR)
    {
      if (flags & MIPMAPPED)
      {
	Log::writeError("Rectangular textures cannot be mipmapped");
	return false;
      }

      if (GLEW_ARB_texture_rectangle)
	textureTarget = GL_TEXTURE_RECTANGLE_ARB;
      else
      {
	Log::writeError("Rectangular textures are not supported by the current OpenGL context");
	return false;
      }
    }
    else
      textureTarget = GL_TEXTURE_2D;
  }
  else
  {
    // TODO: Support 3D textures

    Log::writeError("3D textures not supported yet");
    return false;
  }

  // Save source image dimensions
  sourceWidth = image.getWidth();
  sourceHeight = image.getHeight();

  Image source = image;

  // Adapt source image to OpenGL restrictions
  {
    // Ensure that source image is in GL-compatible format
    if (!source.convert(getConversionFormat(source.getFormat())))
      return false;

    format = source.getFormat();

    // Moira has y-axis down, OpenGL has y-axis up
    source.flipHorizontal();

    // Figure out target dimensions

    if (flags & RECTANGULAR)
    {
      physicalWidth = sourceWidth;
      physicalHeight = sourceHeight;

      unsigned int maxSize;
      glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB, (GLint*) &maxSize);

      if (physicalWidth > maxSize)
	physicalWidth = maxSize;

      if (physicalHeight > maxSize)
	physicalHeight = maxSize;
    }
    else
    {
      unsigned int maxSize;

      glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*) &maxSize);

      if (flags & DONT_GROW)
      {
	physicalWidth = getClosestPower(sourceWidth, std::min(maxSize, sourceWidth));
	physicalHeight = getClosestPower(sourceHeight, std::min(maxSize, sourceHeight));
      }
      else
      {
	physicalWidth = getClosestPower(sourceWidth, maxSize);
	physicalHeight = getClosestPower(sourceHeight, maxSize);
      }
    }

    // Rescale source image (don't worry, it's a no-op if the sizes are equal)
    if (!source.resize(physicalWidth, physicalHeight))
      return false;
  }

  // Clear any errors
  glGetError();

  // Contact space station
  glGenTextures(1, &textureID);

  glPushAttrib(GL_TEXTURE_BIT | GL_PIXEL_MODE_BIT);
  glBindTexture(textureTarget, textureID);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (flags & MIPMAPPED)
  {
    if (textureTarget == GL_TEXTURE_1D)
    {
      gluBuild1DMipmaps(textureTarget,
                        format.getChannelCount(),
                        source.getWidth(),
                        genericFormatMap[source.getFormat()],
                        GL_UNSIGNED_BYTE,
                        source.getPixels());
    }
    else
    {
      gluBuild2DMipmaps(textureTarget,
                        format.getChannelCount(),
                        source.getWidth(),
                        source.getHeight(),
                        genericFormatMap[source.getFormat()],
                        GL_UNSIGNED_BYTE,
                        source.getPixels());
    }

    levelCount = (unsigned int) log2f(fmaxf(sourceWidth, sourceHeight));
    /*
    if (flags & RECTANGULAR)
      levelCount = (unsigned int) (1.f + floorf(log2f(fmaxf(width, height))));
    else
      levelCount = (unsigned int) log2f(fmaxf(width, height));
    */
  }
  else
  {
    if (textureTarget == GL_TEXTURE_1D)
    {
      glTexImage1D(textureTarget,
                   0,
                   formatMap[source.getFormat()],
                   source.getWidth(),
                   0,
                   genericFormatMap[source.getFormat()],
                   GL_UNSIGNED_BYTE,
                   source.getPixels());
    }
    else
    {
      glTexImage2D(textureTarget,
                   0,
                   formatMap[source.getFormat()],
                   source.getWidth(),
                   source.getHeight(),
                   0,
                   genericFormatMap[source.getFormat()],
                   GL_UNSIGNED_BYTE,
                   source.getPixels());
    }

    levelCount = 1;
  }

  glGetTexParameteriv(textureTarget, GL_TEXTURE_MIN_FILTER, &minFilter);
  glGetTexParameteriv(textureTarget, GL_TEXTURE_MAG_FILTER, &magFilter);
  glGetTexParameteriv(textureTarget, GL_TEXTURE_WRAP_S, &addressMode);

  glPopAttrib();

  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
  {
    Log::writeError("Error during creation of texture %s: %s",
                    getName().c_str(),
		    gluErrorString(error));
    return false;
  }
  
  return true;
}
  
///////////////////////////////////////////////////////////////////////

TextureLayer::TextureLayer(unsigned int initUnit):
  unit(initUnit)
{
  static bool initialized = false;

  if (!initialized)
  {
    Context::getCreateSignal().connect(onCreateContext);
    Context::getDestroySignal().connect(onDestroyContext);

    if (Context::get() && caches.empty())
      onCreateContext();

    initialized = true;
  }

  if (samplerTypeMap.isEmpty())
  {
    samplerTypeMap[ShaderUniform::SAMPLER_1D] = GL_TEXTURE_1D;
    samplerTypeMap[ShaderUniform::SAMPLER_2D] = GL_TEXTURE_2D;
    samplerTypeMap[ShaderUniform::SAMPLER_3D] = GL_TEXTURE_3D;
    samplerTypeMap.setDefaults((ShaderUniform::Type) 0, 0);
  }
}

void TextureLayer::apply(void) const
{
  if (unit > unitCount)
  {
    Log::writeError("Cannot apply texture layer to non-existent texture unit");
    return;
  }

  Data& cache = caches[unit];

  if (cache.dirty)
  {
    force();
    return;
  }

  if (GLEW_ARB_multitexture)
  {
    if (unit != activeUnit)
    {
      glActiveTextureARB(GL_TEXTURE0_ARB + unit);
      activeUnit = unit;
    }
  }

  if (data.texture)
  {
    const GLenum textureTarget = data.texture->getTarget();

    if (textureTarget != textureTargets[unit])
    {
      if (textureTargets[unit])
	glDisable(textureTargets[unit]);

      glEnable(textureTarget);
      textureTargets[unit] = textureTarget;
    }

    if (data.texture != cache.texture)
    {
      glBindTexture(textureTarget, data.texture->textureID);
      cache.texture = data.texture;
    }

    if (data.combineMode != cache.combineMode)
    {
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, data.combineMode);
      cache.combineMode = data.combineMode;
    }

    // Set texture environment color
    if (data.combineColor != cache.combineColor)
    {
      glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, data.combineColor);
      cache.combineColor = data.combineColor;
    }

    if (data.addressMode != data.texture->addressMode)
    {
      // Yes, this is correct.  This is per-object state, not per-unit state,
      // so we can't use the regular state cache

      glTexParameteri(textureTarget, GL_TEXTURE_WRAP_S, data.addressMode);
      glTexParameteri(textureTarget, GL_TEXTURE_WRAP_T, data.addressMode);
      data.texture->addressMode = data.addressMode;
    }

    GLint minFilter = data.minFilter;

    if (!(data.texture->getFlags() & Texture::MIPMAPPED))
      minFilter = unmipmapMinFilter(minFilter);

    if (minFilter != data.texture->minFilter)
    {
      // Yes, this is correct.  This is per-object state, not per-unit state,
      // so we can't use the regular state cache

      glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, minFilter);
      data.texture->minFilter = minFilter;
    }

    if (data.magFilter != data.texture->magFilter)
    {
      // Yes, this is correct.  This is per-object state, not per-unit state,
      // so we can't use the regular state cache

      glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, data.magFilter);
      data.texture->magFilter = data.magFilter;
    }

    if (data.sphereMapped != cache.sphereMapped)
    {
      setBooleanState(GL_TEXTURE_GEN_S, data.sphereMapped);
      setBooleanState(GL_TEXTURE_GEN_T, data.sphereMapped);

      if (data.sphereMapped)
      {
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
      }

      cache.sphereMapped = data.sphereMapped;
    }

    if (!data.samplerName.empty())
      applySampler(*data.texture);
  }
  else
  {
    if (cache.texture)
    {
      glDisable(textureTargets[unit]);
      textureTargets[unit] = 0;
      cache.texture = NULL;
    }

    if (!data.samplerName.empty())
      Log::writeError("Texture layer %u with no texture bound to GLSL sampler uniform %s", unit, data.samplerName.c_str());
  }

#if _DEBUG
  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
    Log::writeError("Error when applying texture layer %u: %s", unit, gluErrorString(error));
#endif

  data.dirty = cache.dirty = false;
}

bool TextureLayer::isCompatible(void) const
{
  if (unit > getUnitCount())
    return false;

  return true;
}

bool TextureLayer::isSphereMapped(void) const
{
  return data.sphereMapped;
}

GLenum TextureLayer::getCombineMode(void) const
{
  return data.combineMode;
}

const ColorRGBA& TextureLayer::getCombineColor(void) const
{
  return data.combineColor;
}

GLint TextureLayer::getMinFilter(void) const
{
  return data.minFilter;
}

GLint TextureLayer::getMagFilter(void) const
{
  return data.magFilter;
}

GLint TextureLayer::getAddressMode(void) const
{
  return data.addressMode;
}

Texture* TextureLayer::getTexture(void) const
{
  return data.texture;
}

const String& TextureLayer::getSamplerName(void) const
{
  return data.samplerName;
}

unsigned int TextureLayer::getUnit(void) const
{
  return unit;
}

void TextureLayer::setSphereMapped(bool newState)
{
  data.sphereMapped = newState;
  data.dirty = true;
}

void TextureLayer::setCombineMode(GLenum newMode)
{
  data.combineMode = newMode;
  data.dirty = true;
}

void TextureLayer::setCombineColor(const ColorRGBA& newColor)
{
  data.combineColor = newColor;
  data.dirty = true;
}

void TextureLayer::setFilters(GLint newMinFilter, GLint newMagFilter)
{
  data.minFilter = newMinFilter;
  data.magFilter = newMagFilter;
  data.dirty = true;
}

void TextureLayer::setAddressMode(GLint newMode)
{
  data.addressMode = newMode;
  data.dirty = true;
}

void TextureLayer::setTexture(Texture* newTexture)
{
  data.texture = newTexture;
  data.dirty = true;
}

void TextureLayer::setSamplerName(const String& newName)
{
  data.samplerName = newName;
  data.dirty = true;
}

void TextureLayer::setDefaults(void)
{
  data.setDefaults();
}

unsigned int TextureLayer::getUnitCount(void)
{
  if (!Context::get())
  {
    Log::writeError("Cannot query texture unit count before OpenGL context creation");
    return 0;
  }

  if (!unitCount)
  {
    if (GLEW_ARB_multitexture)
      glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*) &unitCount);
    else
      unitCount = 1;
  }

  return unitCount;
}

void TextureLayer::force(void) const
{
  if (GLEW_ARB_multitexture)
  {
    glActiveTextureARB(GL_TEXTURE0_ARB + unit);
    activeUnit = unit;
  }

  Data& cache = caches[unit];

  cache = data;

  glDisable(GL_TEXTURE_1D);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_TEXTURE_3D);

  textureTargets[unit] = 0;

  setBooleanState(GL_TEXTURE_GEN_S, data.sphereMapped);
  setBooleanState(GL_TEXTURE_GEN_T, data.sphereMapped);
  glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
  glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, data.combineMode);
  glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, data.combineColor);

  Texture* texture = NULL;

  if (data.texture)
  {
    const GLenum textureTarget = data.texture->getTarget();

    glEnable(textureTarget);
    glBindTexture(textureTarget, data.texture->textureID);

    textureTargets[unit] = textureTarget;

    GLint minFilter = data.minFilter;

    if (!(data.texture->getFlags() & Texture::MIPMAPPED))
      minFilter = unmipmapMinFilter(minFilter);

    glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, minFilter);
    data.texture->minFilter = minFilter;

    glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, data.magFilter);
    data.texture->magFilter = data.magFilter;

    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_S, data.addressMode);
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_T, data.addressMode);

    data.texture->addressMode = data.addressMode;

    if (!data.samplerName.empty())
      forceSampler(*data.texture);
  }
  else
  {
    if (!data.samplerName.empty())
      Log::writeError("Texture layer %u with no texture bound to GLSL sampler uniform %s", unit, data.samplerName.c_str());
  }

#if _DEBUG
  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
    Log::writeError("Error when forcing texture layer %u: %s", unit, gluErrorString(error));
#endif

  data.dirty = cache.dirty = false;
}

void TextureLayer::applySampler(Texture& texture) const
{
  forceSampler(texture);
}

void TextureLayer::forceSampler(Texture& texture) const
{
  ShaderPermutation* permutation = ShaderPermutation::getCurrent();
  if (!permutation)
  {
    Log::writeError("Cannot bind texture layer %u to GLSL sampler uniform %s without a current permutation",
                    unit,
		    data.samplerName.c_str());
    return;
  }

  ShaderUniform* sampler = permutation->getUniform(data.samplerName);
  if (!sampler)
  {
    Log::writeError("Texture layer %u bound to non-existent GLSL sampler uniform %s",
                    unit,
		    data.samplerName.c_str());
    return;
  }

  if (samplerTypeMap[sampler->getType()] != texture.getTarget())
  {
    Log::writeWarning("Type mismatch between texture %s and GLSL sampler uniform %s for texture layer %u",
                      texture.getName().c_str(),
		      data.samplerName.c_str(),
		      unit);
    return;
  }

  sampler->setValue((int) unit);
}

void TextureLayer::setBooleanState(GLenum state, bool value) const
{
  if (value)
    glEnable(state);
  else
    glDisable(state);
}

void TextureLayer::onCreateContext(void)
{
  caches.resize(getUnitCount());
  textureTargets.insert(textureTargets.end(), getUnitCount(), 0);
}

void TextureLayer::onDestroyContext(void)
{
  caches.clear();
  textureTargets.clear();
  activeUnit = 0;
  unitCount = 0;
}

TextureLayer::DataList TextureLayer::caches;

TextureLayer::TargetList TextureLayer::textureTargets;

unsigned int TextureLayer::activeUnit = 0;

unsigned int TextureLayer::unitCount = 0;

///////////////////////////////////////////////////////////////////////

TextureLayer::Data::Data(void)
{
  setDefaults();
}

void TextureLayer::Data::setDefaults(void)
{
  dirty = true;
  texture = NULL;
  sphereMapped = false;
  combineMode = GL_MODULATE;
  combineColor.set(1.f, 1.f, 1.f, 1.f);
  minFilter = GL_LINEAR_MIPMAP_LINEAR;
  magFilter = GL_LINEAR;
  addressMode = GL_REPEAT;
}

///////////////////////////////////////////////////////////////////////

TextureStack::TextureStack(void)
{
  if (!defaults.size())
  {
    const unsigned int textureUnitCount = TextureLayer::getUnitCount();

    for (unsigned int i = 0;  i < textureUnitCount;  i++)
      defaults.push_back(TextureLayer(i));
  }
}

void TextureStack::apply(void) const
{
  const unsigned int count = std::min(layers.size(), defaults.size());

  for (unsigned int i = 0;  i < count;  i++)
    layers[i].apply();

  for (unsigned int i = count;  i < defaults.size();  i++)
    defaults[i].apply();
}

TextureLayer& TextureStack::createTextureLayer(void)
{
  layers.push_back(TextureLayer(layers.size()));
  return layers.back();
}

void TextureStack::destroyTextureLayers(void)
{
  layers.clear();
}

bool TextureStack::isCompatible(void) const
{
  for (unsigned int i = 0;  i < layers.size();  i++)
  {
    if (!layers[i].isCompatible())
      return false;
  }

  return true;
}

unsigned int TextureStack::getTextureLayerCount(void) const
{
  return layers.size();
}

TextureLayer& TextureStack::getTextureLayer(unsigned int index)
{
  return layers[index];
}

const TextureLayer& TextureStack::getTextureLayer(unsigned int index) const
{
  return layers[index];
}

TextureStack::LayerList TextureStack::defaults;

///////////////////////////////////////////////////////////////////////

  } /*namespace GL*/
} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
