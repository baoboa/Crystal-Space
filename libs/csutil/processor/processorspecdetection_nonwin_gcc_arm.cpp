/*
Copyright (C) 2007 by Michael Gist and Marten Svanfeldt

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free
Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"

#if defined(CS_COMPILER_GCC) && defined(CS_PROCESSOR_ARM) && !defined(CS_PLATFORM_WIN32)
#include "csutil/processorspecdetection.h"

namespace CS
{
  namespace Platform
  {
    namespace Implementation
    {
      uint DetectInstructionsNonWinGCCARM::CheckSupportedInstruction()
      {
        uint instructionBitMask = 0U;
        // TODO : check for NEON support and stuff
        return instructionBitMask;
      }
    } // namespace Implementation
  } // namespace Platform
} // namespace CS

#endif // defined(CS_COMPILER_GCC) && defined(CS_PROCESSOR_ARM) && !defined(CS_PLATFORM_WIN32)
