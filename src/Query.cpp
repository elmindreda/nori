///////////////////////////////////////////////////////////////////////
// Nori - a simple game engine
// Copyright (c) 2011 Camilla Berglund <elmindreda@elmindreda.org>
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

#include <nori/Config.hpp>

#include <nori/Texture.hpp>
#include <nori/RenderBuffer.hpp>
#include <nori/Query.hpp>

#include <GREG/greg.h>

#include <internal/OpenGL.hpp>

#include <memory>

namespace nori
{

OcclusionQuery::~OcclusionQuery()
{
  if (m_active)
    logError("Occlusion query destroyed while active");

  if (m_queryID)
    glDeleteQueries(1, &m_queryID);

#if NORI_DEBUG
  checkGL("OpenGL error during occlusion query deletion");
#endif
}

void OcclusionQuery::begin()
{
  if (m_active)
  {
    logError("Cannot begin already active occlusion query");
    return;
  }

  glBeginQuery(GL_SAMPLES_PASSED, m_queryID);

  m_active = true;

#if NORI_DEBUG
  checkGL("OpenGL error during occlusion query begin");
#endif
}

void OcclusionQuery::end()
{
  if (!m_active)
  {
    logError("Cannot end non-active occlusion query");
    return;
  }

  glEndQuery(GL_SAMPLES_PASSED);

  m_active = false;

#if NORI_DEBUG
  checkGL("OpenGL error during occlusion query end");
#endif
}

bool OcclusionQuery::hasResultAvailable() const
{
  if (m_active)
    return false;

  int available;
  glGetQueryObjectiv(m_queryID, GL_QUERY_RESULT_AVAILABLE, &available);

#if NORI_DEBUG
  if (!checkGL("OpenGL error during occlusion query result availability check"))
    return false;
#endif

  return available ? true : false;
}

uint OcclusionQuery::result() const
{
  if (m_active)
  {
    logError("Cannot retrieve result of active occlusion query");
    return 0;
  }

  uint result;
  glGetQueryObjectuiv(m_queryID, GL_QUERY_RESULT, &result);

#if NORI_DEBUG
  if (!checkGL("OpenGL error during occlusion query result retrieval"))
    return 0;
#endif

  return result;
}

std::unique_ptr<OcclusionQuery> OcclusionQuery::create(RenderContext& context)
{
  std::unique_ptr<OcclusionQuery> query(new OcclusionQuery(context));
  if (!query->init())
    return nullptr;

  return query;
}

OcclusionQuery::OcclusionQuery(RenderContext& context):
  m_context(context),
  m_queryID(0),
  m_active(false)
{
}

bool OcclusionQuery::init()
{
  glGenQueries(1, &m_queryID);

  if (!checkGL("OpenGL error during creation of occlusion query object"))
    return false;

  return true;
}

} /*namespace nori*/

