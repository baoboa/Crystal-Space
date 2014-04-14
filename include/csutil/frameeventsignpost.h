/*
   Crystal Space 3D engine: Event and module naming interface
   (C) 2005 by Adam D. Bradley <artdodge@cs.bu.edu>

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

#ifndef __CS_CSUTIL_FRAMEEVENTSIGNPOST_H__
#define __CS_CSUTIL_FRAMEEVENTSIGNPOST_H__

#include "cssysdef.h"
#include "iutil/eventh.h"

struct iFrameEventSignpost : public iEventHandler
{
 public:
  iFrameEventSignpost () { }
  virtual ~iFrameEventSignpost () { }
  CS_EVENTHANDLER_DEFAULT_INSTANCE_CONSTRAINTS
  virtual bool HandleEvent (iEvent&)
  {
    return false;
  }
};

#endif // __CS_CSUTIL_FRAMEEVENTSIGNPOST_H__
