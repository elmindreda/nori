///////////////////////////////////////////////////////////////////////
// Wendy OpenAL library
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

#include <wendy/Config.hpp>
#include <wendy/Core.hpp>

#include <internal/ALHelper.hpp>

#include <al.h>
#include <alc.h>

#include <cstdio>

///////////////////////////////////////////////////////////////////////

namespace wendy
{

///////////////////////////////////////////////////////////////////////

namespace
{

const char* getErrorStringAL(ALenum error)
{
  switch (error)
  {
    case AL_INVALID_NAME:
      return "Invalid name parameter";
    case AL_INVALID_ENUM:
      return "Invalid enum parameter";
    case AL_INVALID_VALUE:
      return "Invalid enum parameter value";
    case AL_INVALID_OPERATION:
      return "Invalid operation";
    case AL_OUT_OF_MEMORY:
      return "Out of memory";
  }

  return "Unknown OpenAL error";
}

const char* getErrorStringALC(ALCenum error)
{
  switch (error)
  {
    case ALC_INVALID_DEVICE:
      return "Invalid device";
    case ALC_INVALID_CONTEXT:
      return "Invalid context";
    case ALC_INVALID_ENUM:
      return "Invalid enum parameter";
    case ALC_INVALID_VALUE:
      return "Invalid enum parameter value";
    case ALC_OUT_OF_MEMORY:
      return "Out of memory";
  }

  return "Unknown OpenAL error";
}

} /*namespace*/

///////////////////////////////////////////////////////////////////////

bool checkAL(const char* format, ...)
{
  ALenum error = alGetError();
  if (error == AL_NO_ERROR)
    return true;

  va_list vl;

  va_start(vl, format);
  String message = vlformat(format, vl);
  va_end(vl);

  logError("%s: %s", message.c_str(), getErrorStringAL(error));
  return false;
}

bool checkALC(const char* format, ...)
{
  ALCenum error = alcGetError(alcGetContextsDevice(alcGetCurrentContext()));
  if (error == ALC_NO_ERROR)
    return true;

  va_list vl;

  va_start(vl, format);
  String message = vlformat(format, vl);
  va_end(vl);

  logError("%s: %s", message.c_str(), getErrorStringALC(error));
  return false;
}

///////////////////////////////////////////////////////////////////////

} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
