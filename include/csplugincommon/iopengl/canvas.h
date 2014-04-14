/*
    Copyright (C) 2012 by Frank Richter

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

#ifndef __CS_CSPLUGINCOMMON_IOPENGL_CANVAS_H__
#define __CS_CSPLUGINCOMMON_IOPENGL_CANVAS_H__

#include "iutil/cfgmgr.h"

/**\file
 * Interface to read custom GL driver databases
 */

struct iDocumentNode;

/**
 * Interface for OpenGL-specific canvas functionality.
 */
struct iOpenGLCanvas : public virtual iBase
{
  SCF_INTERFACE(iOpenGLCanvas, 0, 0, 1);

  /// Get number of current multisampling samples
  virtual int GetMultiSamples() = 0;
};

#endif // __CS_CSPLUGINCOMMON_IOPENGL_CANVAS_H__
