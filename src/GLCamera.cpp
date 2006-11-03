///////////////////////////////////////////////////////////////////////
// Wendy OpenGL library
// Copyright (c) 2005 Camilla Berglund <elmindreda@elmindreda.org>
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
#include <wendy/GLTexture.h>
#include <wendy/GLVertex.h>
#include <wendy/GLBuffer.h>
#include <wendy/GLCanvas.h>
#include <wendy/GLRender.h>
#include <wendy/GLCamera.h>

///////////////////////////////////////////////////////////////////////

namespace wendy
{
  namespace GL
  {
  
///////////////////////////////////////////////////////////////////////

using namespace moira;

///////////////////////////////////////////////////////////////////////

Camera::Camera(const std::string& name):
  Managed<Camera>(name),
  FOV(90.f),
  aspectRatio(0.f),
  minDepth(0.01f),
  maxDepth(1000.f),
  dirtyFrustum(true),
  dirtyInverse(true)
{
}

void Camera::begin(void) const
{
  if (current)
    throw Exception("Cannot nest cameras");

  Renderer* renderer = Renderer::get();
  if (!renderer)
  {
    Log::writeError("Cannot make camera current without a renderer");
    return;
  }

  renderer->begin3D(FOV, aspectRatio);

  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();

  Matrix4 matrix = getInverseTransform();
  glLoadMatrixf(matrix);

  glPopAttrib();

  current = const_cast<Camera*>(this);
}

void Camera::end(void) const
{
  if (current != this)
    throw Exception("No current camera or camera invalidated during rendering");

  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glPopAttrib();

  Renderer::get()->end();

  current = NULL;
}

float Camera::getFOV(void) const
{
  return FOV;
}

float Camera::getAspectRatio(void) const
{
  return aspectRatio;
}

float Camera::getMinDepth(void) const
{
  return minDepth;
}

float Camera::getMaxDepth(void) const
{
  return maxDepth;
}

void Camera::setFOV(float newFOV)
{
  if (current == this)
    throw Exception("Cannot change properties on an active camera");

  FOV = newFOV;
  dirtyFrustum = true;
}

void Camera::setAspectRatio(float newAspectRatio)
{
  if (current == this)
    throw Exception("Cannot change properties on an active camera");

  aspectRatio = newAspectRatio;
  dirtyFrustum = true;
}

void Camera::setDepthRange(float newMinDepth, float newMaxDepth)
{
  if (current == this)
    throw Exception("Cannot change properties on an active camera");

  minDepth = newMinDepth;
  maxDepth = newMaxDepth;
  dirtyFrustum = true;
}

const Transform3& Camera::getTransform(void) const
{
  return transform;
}

const Transform3& Camera::getInverseTransform(void) const
{
  if (dirtyInverse)
  {
    inverse = transform;
    inverse.invert();
    dirtyInverse = false;
  }

  return inverse;
}

void Camera::setTransform(const Transform3& newTransform)
{
  transform = newTransform;
  dirtyFrustum = true;
  dirtyInverse = true;
}

const Frustum& Camera::getFrustum(void) const
{
  if (dirtyFrustum)
  {
    frustum.set(FOV, aspectRatio, maxDepth);
    frustum.transformBy(transform);
    dirtyFrustum = false;
  }

  return frustum;
}

Camera* Camera::getCurrent(void)
{
  return current;
}

Camera* Camera::current = NULL;

///////////////////////////////////////////////////////////////////////

  } /*namespace GL*/
} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
