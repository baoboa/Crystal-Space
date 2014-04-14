/*
    Copyright (C) 2001 by Jorrit Tyberghein
    Copyright (C) 1998-2000 by Andrew Zabolotny <bit@eltech.ru>

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

#ifndef __CS_IVIDEO_CANVAS_H__
#define __CS_IVIDEO_CANVAS_H__

/**\file
 * Graphics canvas interface
 */

/**
 * \addtogroup gfx2d
 * @{ */

#include "csutil/scf.h"
#include "ivideo/cursor.h"
#include "csgfx/rgbpixel.h"

struct iImage;
struct iNativeWindow;

class csRect;

/**
 * This is the interface for the graphics canvas.
 * graphics canvas is responsible for basic operations such as creating the
 * window, returning pixel format and so on.
 *
 * Main creators of instances implementing this interface:
 * - OpenGL/Windows canvas plugin (crystalspace.graphics2d.glwin32)
 * - OpenGL/X11 canvas plugin (crystalspace.graphics2d.glx)
 * - Null 2D canvas plugin (crystalspace.graphics2d.null)
 * - Some others.
 * - Note that it is the 3D renderer that will automatically create
 *       the right instance of the canvas that it requires.
 */
struct iGraphicsCanvas : public virtual iBase
{
  SCF_INTERFACE (iGraphicsCanvas, 1, 0, 1);
  
  /// Open the canvas.
  virtual bool CanvasOpen () = 0;

  /// Close the canvas.
  virtual void CanvasClose () = 0;

  /// Return color depth of the framebuffer.
  virtual int GetColorDepth () = 0;

  /**
   * Flip video pages (or dump backbuffer into framebuffer). The area
   * parameter is only a hint to the canvas driver. Changes outside the
   * rectangle may or may not be printed as well.
   */
  virtual void Print (csRect const* pArea) = 0;

  /// Enable/disable canvas resizing
  virtual void AllowResize (bool iAllow) = 0;

  /// Resize the canvas
  virtual bool CanvasResize (int w, int h) = 0;

  /**
   * Get the native window corresponding with this canvas.
   * If this is an off-screen canvas then this will return 0.
   */
  virtual iNativeWindow* GetNativeWindow () = 0;

  /// Returns 'true' if the program is being run full-screen.
  virtual bool GetFullScreen () = 0;

  /**
   * Change the fullscreen state of the canvas.
   */
  virtual void SetFullScreen (bool b) = 0;

  /// Set mouse position (relative to top-left of CS window).
  virtual bool SetMousePosition (int x, int y) = 0;

  /**
   * Set mouse cursor to one of predefined shape classes
   * (see csmcXXX enum above). If a specific mouse cursor shape
   * is not supported, return 'false'; otherwise return 'true'.
   * If system supports it the cursor should be set to its nearest
   * system equivalent depending on iShape argument and the routine
   * should return "true".
   */
  virtual bool SetMouseCursor (csMouseCursorID iShape) = 0;

  /**
   * Set mouse cursor using an image.  If the operation is unsupported, 
   * \c false is returned, otherwise \c true.
   *
   * \remarks
   * If setting a custom mouse is not supported no mouse cursor "emulation"
   * is done in the canvas.  You can use the custom cursor plugin (see
   * iCursor) for automatic mouse cursor emulation in case the canvas
   * doesn't support it, or do it yourself (after everything was drawn,
   * draw the desired mouse cursor image at the current mouse cursor
   * position).
   *
   * On some platforms there are only monochrome pointers available.  In this
   * all black colors in the image will become the value of \a bg and all 
   * non-black colors will become \a fg. This behaviour can be disabled
   * by setting the <tt>Video.SystemMouseCursor</tt> configuration key to
   * \c rgbaonly.
   */
  virtual bool SetMouseCursor (iImage *image, const csRGBcolor* keycolor = 0, 
                               int hotspot_x = 0, int hotspot_y = 0,
                               csRGBcolor fg = csRGBcolor(255,255,255),
                               csRGBcolor bg = csRGBcolor(0,0,0)) = 0;

  /**
   * Set gamma value (if supported by canvas). By default this is 1.
   * Smaller values are darker. If the canvas doesn't support gamma
   * then this function will return false.
   */
  virtual bool SetGamma (float gamma) = 0;

  /**
   * Get gamma value.
   */
  virtual float GetGamma () const = 0;

  /**
   * Get the name of the canvas
   */
  virtual const char* GetName () const = 0;

  /// Get the dimensions of the framebuffer.
  virtual void GetFramebufferDimensions (int& width, int& height) = 0;
};

/** @} */

#endif // __CS_IVIDEO_CANVAS_H__

