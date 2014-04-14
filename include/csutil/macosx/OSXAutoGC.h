/*
    Copyright (C) 2012 by Eric Sunshine <sunshine@sunshineco.com>

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
#ifndef __CSUTIL_MACOSX_OSXAUTOGC_H__
#define __CSUTIL_MACOSX_OSXAUTOGC_H__

#include "cssysdef.h"
#import <Foundation/NSAutoreleasePool.h>

/**
 * Helper class to allocate an \c NSAutoreleasePool at the point of delcaration
 * and release it automatically at the end of the block in which it is
 * declared.  This is more convenient (and involves less typing) than manually
 * allocating an \c NSAutoreleasePool and then remembering to release it later.
 * Can only be used in conjunction with the Objective-C++ compiler.
 */
class csOSXAutoGC
{
private:
  NSAutoreleasePool* x;
public:
  csOSXAutoGC() { x = [[NSAutoreleasePool alloc] init]; }
  ~csOSXAutoGC() { [x release]; }
};

#endif // __CSUTIL_MACOSX_OSXAUTOGC_H__
