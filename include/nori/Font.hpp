///////////////////////////////////////////////////////////////////////
// Nori - a simple game engine
// Copyright (c) 2006 Camilla Berglund <elmindreda@elmindreda.org>
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

#pragma once

#include <nori/Core.hpp>
#include <nori/Rect.hpp>
#include <nori/Path.hpp>
#include <nori/Pixel.hpp>
#include <nori/Resource.hpp>
#include <nori/Image.hpp>
#include <nori/Face.hpp>

namespace nori
{

/*! @brief %Font layout and rendering object.
 *
 *  This class provides layout and rendering of a single font.
 */
class Font : public Resource, public RefObject
{
public:
  /*! Renders the specified text at the current pen position.
   *  @param text The text to render.
   */
  void drawText(vec2 pen, vec4 color, const char* text);
  /*! @return The ascender for this font.
   */
  float ascender() const { return m_ascender; }
  /*! @return The descender for this font.
   */
  float descender() const { return m_descender; }
  /*! @return The leader for this font.
   */
  float leading() const { return m_leading; }
  /*! @return The width, in pixels, of the character cell for this font.
   */
  float width() const { return m_width; }
  /*! @return The height, in pixels, of the character cell for this font.
   */
  float height() const { return m_height; }
  /*! @param text The text to measure.
   *  @return The bounding rectangle, in pixels, of the specified text as
   *  rendered by this font.
   */
  Rect boundsOf(const char* text);
  /*! @param[in] text The text to measure.
   *  @param[in] start The first codepoint of the string to include.
   *  @param[in] start The number of codepoints to include.
   *  @return The bounding rectangle, in pixels, of the specified section of the
   *  specified text as rendered by this font.
   */
  Rect boundsOf(const char* text, size_t start, size_t count);
  /*! Calculates the layout of glyphs for the specified text.
   */
  std::vector<Rect> layoutOf(const char* text);
  static Ref<Font> create(const ResourceInfo& info,
                          RenderContext& context,
                          Face& face,
                          uint height);
  static Ref<Font> read(RenderContext& context, const std::string& name);
private:
  class Glyph;
  Font(const ResourceInfo& info, RenderContext& context);
  Font(const Font&) = delete;
  bool init(Face& font, uint height);
  const Glyph* addGlyph(uint32 codepoint);
  const Glyph* findGlyph(uint32 codepoint);
  bool addGlyphTextureRow();
  Font& operator = (const Font&) = delete;
  RenderContext& m_context;
  Ref<Face> m_face;
  std::vector<Glyph> m_glyphs;
  float m_scale;
  float m_ascender;
  float m_descender;
  float m_leading;
  float m_width;
  float m_height;
  ivec2 m_position;
  Ref<Texture> m_texture;
  Pass m_pass;
  UniformStateIndex m_colorIndex;
  std::vector<Vertex2ft2fv> m_vertices;
};

/*! @internal
 */
class Font::Glyph
{
public:
  bool operator < (const Glyph& other) const
  {
    return codepoint < other.codepoint;
  }
  bool operator == (uint32 desired) const
  {
    return codepoint == desired;
  }
  vec2 offset;
  vec2 size;
  vec2 bearing;
  float advance;
  uint32 codepoint;
};

} /*namespace nori*/

