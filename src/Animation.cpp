///////////////////////////////////////////////////////////////////////
// Wendy core library
// Copyright (c) 2010 Camilla Berglund <elmindreda@elmindreda.org>
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

#include <wendy/Config.h>

#include <wendy/Core.h>
#include <wendy/Block.h>
#include <wendy/Vector.h>
#include <wendy/Bezier.h>
#include <wendy/Quaternion.h>
#include <wendy/Transform.h>
#include <wendy/Managed.h>
#include <wendy/Path.h>
#include <wendy/Stream.h>
#include <wendy/Resource.h>
#include <wendy/Animation.h>

///////////////////////////////////////////////////////////////////////

namespace wendy
{

///////////////////////////////////////////////////////////////////////

KeyFrame3::KeyFrame3(Anim3& initAnimation):
  animation(&initAnimation)
{
}

bool KeyFrame3::operator < (const KeyFrame3& other) const
{
  return moment < other.moment;
}

Time KeyFrame3::getMoment(void) const
{
  return moment;
}

void KeyFrame3::setMoment(Time newMoment)
{
  moment = newMoment;
  animation->sortKeyFrames();
}

const Transform3& KeyFrame3::getTransform(void) const
{
  return transform;
}

const Vec3& KeyFrame3::getDirection(void) const
{
  return direction;
}

void KeyFrame3::setTransform(const Transform3& newTransform)
{
  transform = newTransform;
}

void KeyFrame3::setPosition(const Vec3& newPosition)
{
  transform.position = newPosition;
}

void KeyFrame3::setRotation(const Quat& newRotation)
{
  transform.rotation = newRotation;
}

void KeyFrame3::setDirection(const Vec3& newDirection)
{
  direction = newDirection;
}

///////////////////////////////////////////////////////////////////////

Anim3::Anim3(const String& name):
  Resource<Anim3>(name)
{
}

Anim3::Anim3(const Anim3& source):
  Resource<Anim3>("")
{
  operator = (source);
}

void Anim3::createKeyFrame(Time moment,
                           const Transform3& transform,
                           const Vec3& direction)
{
  moment = std::max(moment, 0.0);

  keyframes.push_back(KeyFrame3(*this));
  keyframes.back().transform = transform;
  keyframes.back().direction = direction;
  keyframes.back().moment = moment;

  sortKeyFrames();
}

void Anim3::destroyKeyFrame(KeyFrame3& frame)
{
  for (KeyFrameList::iterator f = keyframes.begin();  f != keyframes.end();  f++)
  {
    if (&(*f) == &frame)
    {
      keyframes.erase(f);
      break;
    }
  }
}

void Anim3::destroyKeyFrames(void)
{
  keyframes.clear();
}

void Anim3::evaluate(Time moment, Transform3& result) const
{
  if (keyframes.empty())
  {
    result.setIdentity();
    return;
  }

  moment = std::max(moment, 0.0);

  size_t index;

  for (index = 0;  index < keyframes.size();  index++)
  {
    if (keyframes[index].moment > moment)
      break;
  }

  if (index == 0)
  {
    result = keyframes.front().transform;
    return;
  }

  if (index == keyframes.size())
  {
    result = keyframes.back().transform;
    return;
  }

  const KeyFrame3& startFrame = keyframes[index - 1];
  const KeyFrame3& endFrame = keyframes[index];

  const float t = (moment - startFrame.moment) /
                  (endFrame.moment - startFrame.moment);

  const Quat& startRot = startFrame.transform.rotation;
  const Quat& endRot = endFrame.transform.rotation;

  result.rotation = startRot.interpolateTo(t, endRot);

  BezierCurve3 curve;
  curve.P[0] = startFrame.transform.position;
  curve.P[1] = startFrame.direction;
  curve.P[2] = -endFrame.direction;
  curve.P[3] = endFrame.transform.position;

  result.position = curve(t);
}

Anim3& Anim3::operator = (const Anim3& source)
{
  keyframes.reserve(source.keyframes.size());

  for (KeyFrameList::const_iterator f = source.keyframes.begin();  f != source.keyframes.end();  f++)
  {
    keyframes.push_back(KeyFrame3(*this));
    keyframes.back().transform = f->transform;
    keyframes.back().direction = f->direction;
    keyframes.back().moment = f->moment;
  }

  return *this;
}

size_t Anim3::getKeyFrameCount(void) const
{
  return keyframes.size();
}

KeyFrame3& Anim3::getKeyFrame(size_t index)
{
  return keyframes[index];
}

const KeyFrame3& Anim3::getKeyFrame(size_t index) const
{
  return keyframes[index];
}

Time Anim3::getDuration(void) const
{
  if (keyframes.empty())
    return 0.0;

  return keyframes.back().moment;
}

float Anim3::getLength(float tolerance) const
{
  if (keyframes.size() < 2)
    return 0.f;

  float length = 0.f;
  BezierCurve3 curve;

  for (size_t i = 1;  i < keyframes.size();  i++)
  {
    curve.P[0] = keyframes[i - 1].transform.position;
    curve.P[1] = keyframes[i - 1].direction;
    curve.P[2] = -keyframes[i].direction;
    curve.P[3] = keyframes[i].transform.position;

    length += curve.length(tolerance);
  }

  return length;
}

void Anim3::sortKeyFrames(void)
{
  std::stable_sort(keyframes.begin(), keyframes.end());
}

///////////////////////////////////////////////////////////////////////

} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
