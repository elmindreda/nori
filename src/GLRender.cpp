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
#include <wendy/GLTexture.h>
#include <wendy/GLCanvas.h>
#include <wendy/GLLight.h>
#include <wendy/GLVertex.h>
#include <wendy/GLBuffer.h>
#include <wendy/GLShader.h>
#include <wendy/GLRender.h>

#include <algorithm>
#include <cstdlib>

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

class RenderOperationComparator
{
public:
  inline bool operator () (const RenderOperation* x, const RenderOperation* y)
  {
    return *x < *y;
  }
};

}

///////////////////////////////////////////////////////////////////////

RenderPass::RenderPass(const String& initGroupName):
  groupName(initGroupName)
{
  setDefaults();
}

void RenderPass::apply(void) const
{
  // NOTE: Yes, I know this is huge.  You don't need to point it out.

  if (cache.dirty)
  {
    force();
    return;
  }
  
  if (data.cullMode != cache.cullMode)
  {
    if ((data.cullMode == CULL_NONE) != (cache.cullMode == CULL_NONE))
      setBooleanState(GL_CULL_FACE, data.cullMode != CULL_NONE);

    if (data.cullMode != CULL_NONE)
      glCullFace(data.cullMode);
      
    cache.cullMode = data.cullMode;
  }

  if (data.lighting != cache.lighting)
  {
    setBooleanState(GL_LIGHTING, data.lighting);
    cache.lighting = data.lighting;
  }

  if (data.srcFactor != cache.srcFactor || data.dstFactor != cache.dstFactor)
  {
    setBooleanState(GL_BLEND, data.srcFactor != GL_ONE || data.dstFactor != GL_ZERO);

    if (data.srcFactor != GL_ONE || data.dstFactor != GL_ZERO)
      glBlendFunc(data.srcFactor, data.dstFactor);
    
    cache.srcFactor = data.srcFactor;
    cache.dstFactor = data.dstFactor;
  }

  if (data.polygonMode != cache.polygonMode)
  {
    glPolygonMode(GL_FRONT_AND_BACK, data.polygonMode);
    cache.polygonMode = data.polygonMode;
  }

  if (data.lineWidth != cache.lineWidth)
  {
    unsigned int height;

    if (Canvas* canvas = Canvas::getCurrent())
      height = canvas->getPhysicalHeight();
    else
      height = Context::get()->getHeight();

    glLineWidth(data.lineWidth * height / 100.f);
    cache.lineWidth = data.lineWidth;
  }

  if (data.depthTesting || data.depthWriting)
  {
    // Set depth buffer writing.
    if (data.depthWriting != cache.depthWriting)
      glDepthMask(data.depthWriting ? GL_TRUE : GL_FALSE);

    if (data.depthTesting)
    {
      // Set depth buffer function.
      if (data.depthFunction != cache.depthFunction)
      {
        glDepthFunc(data.depthFunction);
        cache.depthFunction = data.depthFunction;
      }
    }
    else if (data.depthWriting)
    {
      // NOTE: Special case; depth buffer filling.
      //       Set specific depth buffer function.
      const GLenum depthFunction = GL_ALWAYS;

      if (cache.depthFunction != depthFunction)
      {
        glDepthFunc(depthFunction);
        cache.depthFunction = depthFunction;
      }
    }

    if (!(cache.depthTesting || cache.depthWriting))
      glEnable(GL_DEPTH_TEST);
  }
  else
  {
    if (cache.depthTesting || cache.depthWriting)
      glDisable(GL_DEPTH_TEST);
  }

  cache.depthTesting = data.depthTesting;
  cache.depthWriting = data.depthWriting;

  if (data.stencilTesting)
  {
    if (!cache.stencilTesting)
    {
      glEnable(GL_STENCIL_TEST);
      cache.stencilTesting = data.stencilTesting;
    }

    if (data.stencilFunction != cache.stencilFunction ||
        data.stencilRef != cache.stencilRef ||
        data.stencilMask != cache.stencilMask)
    {
      glStencilFunc(data.stencilFunction, data.stencilRef, data.stencilMask);
      cache.stencilFunction = data.stencilFunction;
      cache.stencilRef = data.stencilRef;
      cache.stencilMask = data.stencilMask;
    }

    if (data.stencilFailed != cache.stencilFailed ||
        data.depthFailed != cache.depthFailed ||
        data.depthPassed != cache.depthPassed)
    {
      glStencilOp(data.stencilFailed, data.depthFailed, data.depthPassed);
      cache.stencilFailed = data.stencilFailed;
      cache.depthFailed = data.depthFailed;
      cache.depthPassed = data.depthPassed;
    }
  }
  else
  {
    if (cache.stencilTesting)
    {
      glDisable(GL_STENCIL_TEST);
      cache.stencilTesting = data.stencilTesting;
    }
  }

  if (data.colorWriting != cache.colorWriting)
  {
    const GLboolean state = data.colorWriting ? GL_TRUE : GL_FALSE;
    glColorMask(state, state, state, state);
    cache.colorWriting = data.colorWriting;
  }
  
  if (data.lighting)
  {
    // Set ambient material color.
    if (data.ambientColor != cache.ambientColor)
    {
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, data.ambientColor);
      cache.ambientColor = data.ambientColor;
    }

    // Set diffuse material color.
    if (data.diffuseColor != cache.diffuseColor)
    {
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, data.diffuseColor);
      cache.diffuseColor = data.diffuseColor;
    }

    // Set specular material color.
    if (data.specularColor != cache.specularColor)
    {
      glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, data.specularColor);
      cache.specularColor = data.specularColor;
    }

    if (data.shininess != cache.shininess)
    {
      glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, data.shininess);
      cache.shininess = data.shininess;
    }
  }

  if (!data.lighting)
  {
    // For compatibility reasons, we do not trust the cached color.  Since we
    // always overwrite this value, there is no need to check whether the cache
    // is dirty.
    
    glColor4fv(data.defaultColor);
    cache.defaultColor = data.defaultColor;
  }

  if (GLEW_ARB_shader_objects)
  {
    // Since the GLSL program object cannot push the currently active
    // program in any resonable fashion, it must force the use when
    // changing uniforms in a program object.  Hence we cannot trust the
    // state cache's program name to be valid between calls.  Thus we
    // always force the use of the correct program.

    if (data.shaderProgramName.empty())
    {
      ShaderProgram::applyFixedFunction();
      cache.shaderProgramName.clear();
    }
    else
    {
      ShaderProgram* program = ShaderProgram::findInstance(data.shaderProgramName);
      if (program)
      {
	program->apply();
	cache.shaderProgramName = data.shaderProgramName;
      }
      else
	Log::writeError("Render pass uses non-existent GLSL program %s",
	                data.shaderProgramName.c_str());
    }
  }

  /*
  if (GLEW_ARB_vertex_program)
  {
    if (data.vertexProgramName.empty())
    {
      if (!cache.vertexProgramName.empty())
	glDisable(GL_VERTEX_PROGRAM_ARB);

      cache.vertexProgramName.clear();
    }
    else
    {
      VertexProgram* program = VertexProgram::findInstance(data.vertexProgramName);
      if (program)
      {
	if (cache.vertexProgramName.empty())
	  glEnable(GL_VERTEX_PROGRAM_ARB);

	// Since the vertex program object cannot push its binding in any
	// resonable fashion, it must force the binding when creating and
	// changing parameters on a program object.  Hence we cannot trust the
	// state cache's program name to be valid between calls.  Thus we
	// always force the binding to the correct value.
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, program->getGLID());
	cache.vertexProgramName = data.vertexProgramName;
      }
      else
	Log::writeError("Render pass uses non-existent vertex program %s",
	                data.vertexProgramName.c_str());
    }
  }
  else
  {
    if (!data.vertexProgramName.empty())
      Log::writeError("Vertex programs are not supported by the current OpenGL context");
  }

  if (GLEW_ARB_fragment_program)
  {
    if (data.fragmentProgramName.empty())
    {
      if (!cache.fragmentProgramName.empty())
	glDisable(GL_FRAGMENT_PROGRAM_ARB);

      cache.fragmentProgramName.clear();
    }
    else
    {
      FragmentProgram* program = FragmentProgram::findInstance(data.fragmentProgramName);
      if (program)
      {
	if (cache.fragmentProgramName.empty())
	  glEnable(GL_FRAGMENT_PROGRAM_ARB);

	// Since the fragment program object cannot push its binding in any
	// resonable fashion, it must force the binding when creating and
	// changing parameters on a program object.  Hence we cannot trust the
	// state cache's program name to be valid between calls.  Thus we
	// always force the binding to the correct value.
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program->getGLID());
	cache.fragmentProgramName = data.fragmentProgramName;
      }
      else
	Log::writeError("Render pass uses non-existent fragment program %s",
	                data.fragmentProgramName.c_str());
    }
  }
  else
  {
    if (!data.fragmentProgramName.empty())
      Log::writeError("Fragment programs are not supported by the current OpenGL context");
  }
  */

  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
    Log::writeError("Error when applying render pass: %s", gluErrorString(error));

  TextureStack::apply();
  
  data.dirty = false;
}

bool RenderPass::isDirty(void) const
{
  return data.dirty;
}

bool RenderPass::isCulling(void) const
{
  return data.cullMode != CULL_NONE;
}

bool RenderPass::isBlending(void) const
{
  return data.srcFactor != GL_ONE || data.dstFactor != GL_ZERO;
}

bool RenderPass::isDepthTesting(void) const
{
  return data.depthTesting;
}

bool RenderPass::isDepthWriting(void) const
{
  return data.depthWriting;
}

bool RenderPass::isStencilTesting(void) const
{
  return data.stencilTesting;
}

bool RenderPass::isColorWriting(void) const
{
  return data.colorWriting;
}

bool RenderPass::isLit(void) const
{
  return data.lighting;
}

float RenderPass::getLineWidth(void) const
{
  return data.lineWidth;
}

CullMode RenderPass::getCullMode(void) const
{
  return data.cullMode;
}

GLenum RenderPass::getPolygonMode(void) const
{
  return data.polygonMode;
}

GLenum RenderPass::getSrcFactor(void) const
{
  return data.srcFactor;
}

GLenum RenderPass::getDstFactor(void) const
{
  return data.dstFactor;
}

GLenum RenderPass::getDepthFunction(void) const
{
  return data.depthFunction;
}

GLenum RenderPass::getAlphaFunction(void) const
{
  return data.alphaFunction;
}

GLenum RenderPass::getStencilFunction(void) const
{
  return data.stencilFunction;
}

GLenum RenderPass::getStencilFailOperation(void) const
{
  return data.stencilFailed;
}

GLenum RenderPass::getDepthFailOperation(void) const
{
  return data.depthFailed;
}

GLenum RenderPass::getDepthPassOperation(void) const
{
  return data.depthPassed;
}

unsigned int RenderPass::getStencilReference(void) const
{
  return data.stencilRef;
}

unsigned int RenderPass::getStencilMask(void) const
{
  return data.stencilMask;
}

float RenderPass::getShininess(void) const
{
  return data.shininess;
}

const ColorRGBA& RenderPass::getDefaultColor(void) const
{
  return data.defaultColor;
}

const ColorRGBA& RenderPass::getAmbientColor(void) const
{
  return data.ambientColor;
}

const ColorRGBA& RenderPass::getDiffuseColor(void) const
{
  return data.diffuseColor;
}

const ColorRGBA& RenderPass::getSpecularColor(void) const
{
  return data.specularColor;
}

const String& RenderPass::getShaderProgramName(void) const
{
  return data.shaderProgramName;
}

const String& RenderPass::getGroupName(void) const
{
  return groupName;
}

void RenderPass::setLit(bool enable)
{
  data.lighting = enable;
  data.dirty = true;
}

void RenderPass::setDepthTesting(bool enable)
{
  data.depthTesting = enable;
  data.dirty = true;
}

void RenderPass::setDepthWriting(bool enable)
{
  data.depthWriting = enable;
  data.dirty = true;
}

void RenderPass::setStencilTesting(bool enable)
{
  data.stencilTesting = enable;
  data.dirty = true;
}

void RenderPass::setLineWidth(float width)
{
  data.lineWidth = width;
  data.dirty = true;
}

void RenderPass::setCullMode(CullMode mode)
{
  data.cullMode = mode;
  data.dirty = true;
}

void RenderPass::setPolygonMode(GLenum mode)
{
  data.polygonMode = mode;
  data.dirty = true;
}

void RenderPass::setBlendFactors(GLenum src, GLenum dst)
{
  data.srcFactor = src;
  data.dstFactor = dst;
  data.dirty = true;
}

void RenderPass::setDepthFunction(GLenum function)
{
  data.depthFunction = function;
  data.dirty = true;
}

void RenderPass::setAlphaFunction(GLenum function)
{
  data.alphaFunction = function;
  data.dirty = true;
}

void RenderPass::setStencilFunction(GLenum function)
{
  data.stencilFunction = function;
  data.dirty = true;
}

void RenderPass::setStencilValues(unsigned int reference, unsigned int mask)
{
  data.stencilRef = reference;
  data.stencilMask = mask;
  data.dirty = true;
}

void RenderPass::setStencilOperations(GLenum stencilFailed,
                                      GLenum depthFailed,
                                      GLenum depthPassed)
{
  data.stencilFailed = stencilFailed;
  data.depthFailed = depthFailed;
  data.depthPassed = depthPassed;
  data.dirty = true;
}

void RenderPass::setColorWriting(bool enabled)
{
  data.colorWriting = enabled;
  data.dirty = true;
}

void RenderPass::setShininess(float newValue)
{
  data.shininess = newValue;
  data.dirty = true;
}

void RenderPass::setDefaultColor(const ColorRGBA& color)
{
  data.defaultColor = color;
  data.dirty = true;
}

void RenderPass::setAmbientColor(const ColorRGBA& color)
{
  data.ambientColor = color;
  data.dirty = true;
}

void RenderPass::setDiffuseColor(const ColorRGBA& color)
{
  data.diffuseColor = color;
  data.dirty = true;
}

void RenderPass::setSpecularColor(const ColorRGBA& color)
{
  data.specularColor = color;
  data.dirty = true;
}

void RenderPass::setShaderProgramName(const String& newName)
{
  data.shaderProgramName = newName;
  data.dirty = true;
}

void RenderPass::setDefaults(void)
{
  data.setDefaults();
}

void RenderPass::invalidateCache(void)
{
  cache.dirty = true;
}

void RenderPass::force(void) const
{
  cache = data;

  setBooleanState(GL_CULL_FACE, data.cullMode != CULL_NONE);
  if (data.cullMode != CULL_NONE)
    glCullFace(data.cullMode);

  setBooleanState(GL_LIGHTING, data.lighting);

  setBooleanState(GL_BLEND, data.srcFactor != GL_ONE || data.dstFactor != GL_ZERO);
  glBlendFunc(data.srcFactor, data.dstFactor);
  
  glPolygonMode(GL_FRONT_AND_BACK, data.polygonMode);

  unsigned int height;

  if (Canvas* canvas = Canvas::getCurrent())
    height = canvas->getPhysicalHeight();
  else
    height = Context::get()->getHeight();

  glLineWidth(data.lineWidth * height / 100.f);

  glDepthMask(data.depthWriting ? GL_TRUE : GL_FALSE);
  setBooleanState(GL_DEPTH_TEST, data.depthTesting || data.depthWriting);

  if (data.depthWriting && !data.depthTesting)
  {
    const GLenum depthFunction = GL_ALWAYS;
    glDepthFunc(depthFunction);
    cache.depthFunction = depthFunction;
  }
  else
    glDepthFunc(data.depthFunction);

  const GLboolean state = data.colorWriting ? GL_TRUE : GL_FALSE;
  glColorMask(state, state, state, state);

  setBooleanState(GL_STENCIL_TEST, data.stencilTesting);
  glStencilFunc(data.stencilFunction, data.stencilRef, data.stencilMask);
  glStencilOp(data.stencilFailed, data.depthFailed, data.depthPassed);

  glColor4fv(data.defaultColor);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, data.ambientColor);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, data.diffuseColor);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, data.specularColor);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, data.shininess);

  if (GLEW_ARB_shader_objects)
  {
    if (data.shaderProgramName.empty())
      ShaderProgram::applyFixedFunction();
    else
    {
      ShaderProgram* program = ShaderProgram::findInstance(data.shaderProgramName);
      if (program)
	program->apply();
      else
	Log::writeError("Render pass uses non-existent shader program %s",
			data.shaderProgramName.c_str());
    }
  }

  /*
  if (GLEW_ARB_vertex_program)
  {
    if (data.vertexProgramName.empty())
      glDisable(GL_VERTEX_PROGRAM_ARB);
    else
    {
      VertexProgram* program = VertexProgram::findInstance(data.vertexProgramName);
      if (program)
      {
	glEnable(GL_VERTEX_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, program->getGLID());
      }
      else
	Log::writeError("Render pass uses non-existent vertex program %s",
			data.vertexProgramName.c_str());
    }
  }
  else
  {
    if (!data.vertexProgramName.empty())
      Log::writeError("Vertex programs are not supported by the current OpenGL context");
  }

  if (GLEW_ARB_fragment_program)
  {
    if (data.fragmentProgramName.empty())
      glDisable(GL_FRAGMENT_PROGRAM_ARB);
    else
    {
      FragmentProgram* program = FragmentProgram::findInstance(data.fragmentProgramName);
      if (program)
      {
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program->getGLID());
      }
      else
	Log::writeError("Render pass uses non-existent fragment program %s",
			data.fragmentProgramName.c_str());
    }
  }
  else
  {
    if (!data.fragmentProgramName.empty())
      Log::writeError("Fragment programs are not supported by the current OpenGL context");
  }
  */

  GLenum error = glGetError();
  if (error != GL_NO_ERROR)
    Log::writeWarning("Error when forcing render pass: %s", gluErrorString(error));

  TextureStack::apply();
  
  cache.dirty = data.dirty = false;
}

void RenderPass::setBooleanState(GLenum state, bool value) const
{
  if (value)
    glEnable(state);
  else
    glDisable(state);
}

RenderPass::Data RenderPass::cache;

///////////////////////////////////////////////////////////////////////

RenderPass::Data::Data(void)
{
  setDefaults();
}

void RenderPass::Data::setDefaults(void)
{
  dirty = true;
  lighting = false;
  depthTesting = true;
  depthWriting = true;
  stencilTesting = false;
  colorWriting = true;
  lineWidth = 1.f;
  cullMode = CULL_BACK;
  polygonMode = GL_FILL;
  srcFactor = GL_ONE;
  dstFactor = GL_ZERO;
  depthFunction = GL_LESS;
  alphaFunction = GL_ALWAYS;
  stencilFunction = GL_ALWAYS;
  stencilRef = 0;
  stencilMask = ~0;
  stencilFailed = GL_KEEP;
  depthFailed = GL_KEEP;
  depthPassed = GL_KEEP;
  shininess = 0.f;
  defaultColor.set(1.f, 1.f, 1.f, 1.f);
  ambientColor.set(0.f, 0.f, 0.f, 1.f);
  diffuseColor.set(1.f, 1.f, 1.f, 1.f);
  specularColor.set(1.f, 1.f, 1.f, 1.f);
  shaderProgramName.clear();
}

///////////////////////////////////////////////////////////////////////

RenderStyle::RenderStyle(const String& name):
  Resource<RenderStyle>(name)
{
}

RenderPass& RenderStyle::createPass(const String& groupName)
{
  passes.push_back(RenderPass(groupName));
  return passes.back();
}

void RenderStyle::destroyPasses(void)
{
  passes.clear();
}

void RenderStyle::applyPass(unsigned int index) const
{
  getPass(index).apply();
}

bool RenderStyle::operator < (const RenderStyle& other) const
{
  // Styles with blending always go last

  if (!isBlending() && other.isBlending())
    return true;
 
  // TODO: Better sorting.

  return false;
}

bool RenderStyle::isBlending(void) const
{
  for (PassList::const_iterator i = passes.begin();  i != passes.end();  i++)
    if ((*i).isBlending())
      return true;

  return false;
}

RenderPass& RenderStyle::getPass(unsigned int index)
{
  PassList::iterator pass = passes.begin();
  std::advance(pass, index);
  return *pass;
}

const RenderPass& RenderStyle::getPass(unsigned int index) const
{
  PassList::const_iterator pass = passes.begin();
  std::advance(pass, index);
  return *pass;
}

unsigned int RenderStyle::getPassCount(void) const
{
  return (unsigned int) passes.size();
}

///////////////////////////////////////////////////////////////////////

RenderOperation::RenderOperation(void):
  vertexBuffer(NULL),
  indexBuffer(NULL),
  style(NULL),
  start(0),
  count(0)
{
}

bool RenderOperation::operator < (const RenderOperation& other) const
{
  return (*style) < (*other.style);
}

///////////////////////////////////////////////////////////////////////

RenderQueue::RenderQueue(const Camera& initCamera):
  camera(initCamera),
  sorted(true)
{
}

void RenderQueue::addLight(Light& light)
{
  lights.push_back(&light);
}

RenderOperation& RenderQueue::createOperation(void)
{
  sorted = false;

  operations.push_back(RenderOperation());
  return operations.back();
}

void RenderQueue::destroyOperations(void)
{
  operations.clear();
}

void RenderQueue::renderOperations(void) const
{
  sortOperations();

  for (LightList::const_iterator i = lights.begin();  i != lights.end();  i++)
    (*i)->setEnabled(true);

  for (unsigned int i = 0;  i < sortedOperations.size();  i++)
  {
    const RenderOperation& operation = *sortedOperations[i];

    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixf(operation.transform);
    glPopAttrib();

    for (unsigned int j = 0;  j < operation.style->getPassCount();  j++)
    {
      const RenderPass& pass = operation.style->getPass(j);
      if (!pass.getGroupName().empty())
	continue;

      pass.apply();

      if (operation.indexBuffer)
        operation.indexBuffer->render(*(operation.vertexBuffer),
	                              operation.renderMode,
	                              operation.start,
				      operation.count);
      else
        operation.vertexBuffer->render(operation.renderMode,
	                               operation.start,
				       operation.count);
    }

    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
  }

  for (LightList::const_iterator i = lights.begin();  i != lights.end();  i++)
    (*i)->setEnabled(false);
}

const Camera& RenderQueue::getCamera(void) const
{
  return camera;
}

const RenderQueue::LightList& RenderQueue::getLights(void) const
{
  return lights;
}

const RenderQueue::OperationList& RenderQueue::getOperations(void) const
{
  return operations;
}

void RenderQueue::sortOperations(void) const
{
  if (!sorted)
  {
    sortedOperations.clear();
    sortedOperations.reserve(operations.size());
    for (unsigned int i = 0;  i < operations.size();  i++)
      sortedOperations.push_back(&operations[i]);

    RenderOperationComparator comparator;
    std::sort(sortedOperations.begin(), sortedOperations.end(), comparator);
    sorted = true;
  }
}

///////////////////////////////////////////////////////////////////////

RenderStage::~RenderStage(void)
{
}

void RenderStage::prepare(const RenderQueue& queue)
{
}

void RenderStage::render(const RenderQueue& queue)
{
}

///////////////////////////////////////////////////////////////////////

void RenderStageStack::addStage(RenderStage& stage)
{
  stages.push_back(&stage);
}

void RenderStageStack::destroyStages(void)
{
  while (!stages.empty())
  {
    delete stages.back();
    stages.pop_back();
  }
}

void RenderStageStack::prepare(const RenderQueue& queue)
{
  for (StageList::iterator i = stages.begin();  i != stages.end();  i++)
    (*i)->prepare(queue);
}

void RenderStageStack::render(const RenderQueue& queue)
{
  for (StageList::iterator i = stages.begin();  i != stages.end();  i++)
    (*i)->render(queue);
}

///////////////////////////////////////////////////////////////////////

Renderable::~Renderable(void)
{
}

///////////////////////////////////////////////////////////////////////

void Renderer::begin2D(const Vector2& resolution) const
{
  Canvas* canvas = Canvas::getCurrent();
  if (!canvas)
  {
    Log::writeError("Cannot begin without a current canvas");
    return;
  }

  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0.f, resolution.x, 0.f, resolution.y);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glPopAttrib();
}

void Renderer::begin3D(float FOV, float aspect, float nearZ, float farZ) const
{
  Canvas* canvas = Canvas::getCurrent();
  if (!canvas)
  {
    Log::writeError("Cannot begin without a current canvas");
    return;
  }

  if (aspect == 0.f)
    aspect = (float) canvas->getPhysicalWidth() / (float) canvas->getPhysicalHeight();

  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluPerspective(FOV, aspect, nearZ, farZ);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glPopAttrib();
}
  
void Renderer::end(void) const
{
  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glPopAttrib();
}

void Renderer::drawPoint(const Vector2& point)
{
  pass.apply();

  glBegin(GL_POINTS);
  glVertex2fv(point);
  glEnd();
}

void Renderer::drawLine(const Segment2& segment)
{
  pass.apply();

  glBegin(GL_LINES);
  glVertex2fv(segment.start);
  glVertex2fv(segment.end);
  glEnd();
}

void Renderer::drawBezier(const BezierCurve2& spline)
{
  BezierCurve2::PointList points;
  spline.tessellate(points);
  
  pass.apply();

  glBegin(GL_LINE_STRIP);
  for (unsigned int i = 0;  i < points.size();  i++)
    glVertex2fv(points[i]);
  glEnd();
}

void Renderer::drawRectangle(const Rectangle& rectangle)
{
  float minX, minY, maxX, maxY;
  rectangle.getBounds(minX, minY, maxX, maxY);

  if (maxX - minX < 1.f || maxY - minY < 1.f)
    return;

  pass.setPolygonMode(GL_LINE);
  pass.apply();

  glRectf(minX, minY, maxX - 1.f, maxY - 1.f);
}

void Renderer::fillRectangle(const Rectangle& rectangle)
{
  float minX, minY, maxX, maxY;
  rectangle.getBounds(minX, minY, maxX, maxY);

  if (maxX - minX < 1.f || maxY - minY < 1.f)
    return;

  pass.setPolygonMode(GL_FILL);
  pass.apply();

  glRectf(minX, minY, maxX - 1.f, maxY - 1.f);
}

bool Renderer::allocateIndices(IndexBufferRange& range,
		               unsigned int count,
                               IndexBuffer::Type type)
{
  IndexBufferSlot* slot = NULL;

  for (IndexBufferList::iterator i = indexBuffers.begin();  i != indexBuffers.end();  i++)
  {
    if ((*i).indexBuffer->getType() == type && (*i).available >= count)
    {
      slot = &(*i);
      break;
    }
  }

  if (!slot)
  {
    indexBuffers.push_back(IndexBufferSlot());
    slot = &(indexBuffers.back());

    const unsigned int standardCount = 1024;

    slot->indexBuffer = IndexBuffer::createInstance(std::max(standardCount, count),
                                                    type,
						    IndexBuffer::STREAM);
    if (!slot->indexBuffer)
    {
      indexBuffers.pop_back();
      return false;
    }

    slot->available = slot->indexBuffer->getCount();
  }

  range = IndexBufferRange(*(slot->indexBuffer), 
			   slot->indexBuffer->getCount() - slot->available,
                           count);

  slot->available -= count;
  return true;
}

bool Renderer::allocateVertices(VertexBufferRange& range,
				unsigned int count,
				const VertexFormat& format)
{
  VertexBufferSlot* slot = NULL;

  for (VertexBufferList::iterator i = vertexBuffers.begin();  i != vertexBuffers.end();  i++)
  {
    if ((*i).vertexBuffer->getFormat() == format && (*i).available >= count)
    {
      slot = &(*i);
      break;
    }
  }

  if (!slot)
  {
    vertexBuffers.push_back(VertexBufferSlot());
    slot = &(vertexBuffers.back());

    const unsigned int standardCount = 1024;

    slot->vertexBuffer = VertexBuffer::createInstance(std::max(standardCount, count),
                                                      format,
						      VertexBuffer::STREAM);
    if (!slot->vertexBuffer)
    {
      vertexBuffers.pop_back();
      return false;
    }

    slot->available = slot->vertexBuffer->getCount();
  }

  range = VertexBufferRange(*(slot->vertexBuffer), 
			    slot->vertexBuffer->getCount() - slot->available,
                            count);

  slot->available -= count;
  return true;
}

const ColorRGBA& Renderer::getColor(void) const
{
  return pass.getDefaultColor();
}

void Renderer::setColor(const ColorRGBA& newColor)
{
  pass.setDefaultColor(newColor);
}

const RenderStyle& Renderer::getDefaultStyle(void) const
{
  return *defaultStyle;
}

bool Renderer::create(void)
{
  Ptr<Renderer> renderer = new Renderer();
  if (!renderer->init())
    return false;

  set(renderer.detachObject());
  return true;
}

Renderer::Renderer(void)
{
  static bool initialized = false;

  if (!initialized)
  {
    Context::getDestroySignal().connect(onContextDestroy);
    initialized = true;
  }
}

bool Renderer::init(void)
{
  if (!Context::get())
  {
    Log::writeError("Cannot create renderer without OpenGL context");
    return false;
  }

  try
  {
    CheckerImageGenerator generator;
    generator.setDefaultColor(ColorRGBA(1.f, 0.f, 1.f, 1.f));
    generator.setCheckerColor(ColorRGBA(0.f, 1.f, 0.f, 1.f));
    generator.setCheckerSize(1);

    Ptr<Image> image = generator.generate(ImageFormat::RGB888, 2, 2);
    if (!image)
      return false;

    defaultTexture = Texture::createInstance(*image, Texture::DEFAULT, "default");
    if (!defaultTexture)
      return false;

    defaultStyle = new RenderStyle("default");
    
    RenderPass& pass = defaultStyle->createPass();
    pass.setCullMode(CULL_NONE);

    TextureLayer& layer = pass.createTextureLayer();
    layer.setTextureName(defaultTexture->getName());
    layer.setSphereMapped(true);
  }
  catch (Exception& exception)
  {
    Log::writeError("Failed to create default render style");
    return false;
  }

  Context::get()->getFinishSignal().connect(*this, &Renderer::onContextFinish);
  return true;
}

void Renderer::onContextFinish(void)
{
  for (IndexBufferList::iterator i = indexBuffers.begin();  i != indexBuffers.end();  i++)
    (*i).available = (*i).indexBuffer->getCount();

  for (VertexBufferList::iterator i = vertexBuffers.begin();  i != vertexBuffers.end();  i++)
    (*i).available = (*i).vertexBuffer->getCount();
}

void Renderer::onContextDestroy(void)
{
  if (Renderer::get())
  {
    Log::writeWarning("Renderer not explicitly destroyed before context destruction");
    Renderer::destroy();
  }
}

///////////////////////////////////////////////////////////////////////

  } /*namespace GL*/
} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
