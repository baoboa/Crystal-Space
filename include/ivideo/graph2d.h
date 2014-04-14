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

#ifndef __CS_IVIDEO_GRAPH2D_H__
#define __CS_IVIDEO_GRAPH2D_H__

/**\file
 * 2D graphics interface
 */

/**
 * \addtogroup gfx2d
 * @{ */
 
#include "csutil/scf.h"
#include "csgfx/rgbpixel.h"
#include "ivideo/canvas.h"
#include "ivideo/cursor.h"
#include <stdarg.h>

struct iImage;
struct iFontServer;
struct iFont;
struct iNativeWindow;
struct iGraphics2D;

class csBox3;
class csRect;
class csTransform;
class csVector3;

namespace CS {
namespace Math {
class Matrix4;
} // namespace Math
} // namespace CS

/// iGraphics2D::Write() flags.
enum
{
  /**
   * Write by baseline, \p x and \p y are treated as the pen position on
   * a baseline. 
   */
  CS_WRITE_BASELINE    = (1 << 0),
  /**
   * Don't use anti-aliased glyphs.
   */
  CS_WRITE_NOANTIALIAS = (1 << 1)
};

/// Simple 2D pixel coordinate
struct csPixelCoord
{
  /// X component
  int x;
  /// Y component
  int y;
};

/**
 * This is the interface for 2D renderer. The 2D renderer is responsible
 * for all 2D operations such as creating the window, switching pages,
 * returning pixel format and so on.
 *
 * Main creators of instances implementing this interface:
 * - OpenGL/Windows canvas plugin (crystalspace.graphics2d.glwin32)
 * - OpenGL/X11 canvas plugin (crystalspace.graphics2d.glx)
 * - Null 2D canvas plugin (crystalspace.graphics2d.null)
 * - Some others.
 * - Note that it is the 3D renderer that will automatically create
 *       the right instance of the canvas that it requires.
 *
 * Main ways to get pointers to this interface:
 * - csQueryRegistry<iGraphics2D>()
 * - iGraphics3D::GetDriver2D()
 *
 * Main users of this interface:
 * - 3D renderers (iGraphics3D implementations)
 */
struct iGraphics2D : public virtual iGraphicsCanvas
{
  SCF_INTERFACE (iGraphics2D, 5, 0, 0);
  
  /// Open the device.
  virtual bool Open () = 0;

  /// Close the device.
  virtual void Close () = 0;

  /// Return the width of the viewport.
  virtual int GetWidth () = 0;

  /// Return the height of the viewport.
  virtual int GetHeight () = 0;

  /**
   * Find an RGB (0..255) color. The actual color bytes are returned.
   *
   * Use returned value for color arguments in iGraphics2D.
   */
  virtual int FindRGB (int r, int g, int b, int a = 255) = 0;

  /**
   * Retrieve the R,G,B tuple for a given color number.
   */
  virtual void GetRGB (int color, int& r, int& g, int& b) = 0;
  /**
   * Retrieve the R,G,B,A tuple for a given color number.
   */
  virtual void GetRGB (int color, int& r, int& g, int& b, int& a) = 0;
  
  /**
   * Set clipping rectangle.
   * The clipping rectangle is inclusive the top and left edges and exclusive
   * for the right and bottom borders.
   */
  virtual void SetClipRect (int nMinX, int nMinY, int nMaxX, int nMaxY) = 0;

  /// Retrieve clipping rectangle
  virtual void GetClipRect(int& nMinX, int& nMinY, int& nMaxX, int& nMaxY) = 0;

  /**
   * This routine should be called before any draw operations.
   * It should return true if graphics context is ready.
   */
  virtual bool BeginDraw () = 0;

  /// This routine should be called when you finished drawing.
  virtual void FinishDraw () = 0;

  /// Clear backbuffer.
  virtual void Clear (int color) = 0;

  /// Clear all video pages.
  virtual void ClearAll (int color) = 0;

  /// Draw a line.
  virtual void DrawLine(float x1, float y1, float x2, float y2, int color) = 0;

  /// Draw a box
  virtual void DrawBox (int x, int y, int w, int h, int color) = 0;

  /**
   * Clip a line against given rectangle.
   * Function returns true if line is not visible.
   */
  virtual bool ClipLine (float& x1, float& y1, float& x2, float& y2,
    int xmin, int ymin, int xmax, int ymax) = 0;

  /// Draw a pixel.
  virtual void DrawPixel (int x, int y, int color) = 0;

  /// Draw an array of pixel coordinates with the given color.
  virtual void DrawPixels(csPixelCoord const* pixels, int num_pixels,
     int color) = 0;

  /// Blit a memory block.  Format of the image is RGBA in bytes. Row by row.
  virtual void Blit (int x, int y, int width, int height,
    unsigned char const* data) = 0;

  /// Query pixel R,G,B at given screen location
  virtual void GetPixel (int x, int y, uint8 &oR, uint8 &oG, uint8 &oB) = 0;
  /// As GetPixel() above, but with alpha
  virtual void GetPixel (int x, int y, uint8 &oR, uint8 &oG, uint8 &oB,
  	uint8 &oA) = 0;

  /**
   * Write a text string into the back buffer. A value of -1 for \p bg
   * color will not draw the background.
   * \remarks \p str is expected to be UTF-8 encoded.
   * \remarks For transparent backgrounds, it is recommended to obtain a color
   *  value from FindRGB() that has the same R, G, B components as the 
   *  foreground color, but an alpha component of 0.
   */
  virtual void Write (iFont *font, int x, int y, int fg, int bg,
    const char *str, uint flags = 0) = 0;


  /// Resize the canvas
  virtual bool Resize (int w, int h) = 0;

  /// Get the active font server (does not do IncRef())
  virtual iFontServer *GetFontServer () = 0;

  /**
   * Perform a system specific exension.<p>
   * The command is a string; any arguments may follow.
   * There is no way to guarantee the uniquiness of
   * commands, so please try to use descriptive command names rather
   * than "a", "b" and so on...
   */
  bool PerformExtension (char const* command, ...)
  {
    va_list args;
    va_start(args, command);
    bool x = PerformExtensionV(command, args);
    va_end(args);
    return x;
  }

  /**
   * Perform a system specific exension.<p>
   * Just like PerformExtension() except that the command arguments are passed
   * as a `va_list'.
   */
  virtual bool PerformExtensionV (char const* command, va_list) = 0;

  /// Do a screenshot: return a new iImage object
  virtual csPtr<iImage> ScreenShot () = 0;

  /**
   * Write a text string into the back buffer. A value of -1 for \p bg
   * color will not draw the background.
   * \remarks For transparent backgrounds, it is recommended to obtain a color
   *  value from FindRGB() that has the same R, G, B components as the 
   *  foreground color, but an alpha component of 0.
   */
  virtual void Write (iFont *font, int x, int y, int fg, int bg,
    const wchar_t* str, uint flags = 0) = 0;

  /**
   * Set the viewport (the rectangle of the framebuffer to draw to).
   * \param left Left of the viewport. X=0 will map to this.
   * \param top Right of the viewport. Y=0 will map to this.
   * \param width Width of the viewport.
   * \param height Height of the viewport.
   */
  virtual void SetViewport (int left, int top, int width, int height) = 0;
  /// Get the currently set viewport.
  virtual void GetViewport (int& left, int& top, int& width, int& height) = 0;
  
  /// Get a string containing the hardware renderer.
  virtual const char* GetHWRenderer () = 0;
  /// Get a string containing the OpenGL version.
  virtual const char* GetHWGLVersion () = 0;
  /// Get a string containing the vendor info.
  virtual const char* GetHWVendor () = 0;

  /**
   * Draw a line between two points expressed in camera space.
   *
   * \warning This version of the method will only work for iPerspectiveCamera's.
   * For other camera's, you should use the other DrawLineProjected() method.
   *
   * \param v1 Start point of the line.
   * \param v2 End point of the line.
   * \param fov Field of View to use for the projection. Typically, you would
   * want to use the one returned by iPerspectiveCamera::GetFOV().
   * \param color Color of the line.
   */
  virtual void DrawLineProjected (const csVector3& v1, const csVector3& v2,
    float fov, int color) = 0;

  /**
   * Draw a line between two points expressed in camera space.
   * \param v1 Start point of the line.
   * \param v2 End point of the line.
   * \param projection Projection matrix to use for the camera. Typically, you
   * would want to use the one returned by iCamera::GetProjectionMatrix().
   * \param color Color of the line.
   */
  virtual void DrawLineProjected (const csVector3& v1, const csVector3& v2,
    const CS::Math::Matrix4& projection, int color) = 0;

  /**
   * Draw a box expressed in camera space.
   * \param box The box to be drawn.
   * \param v2 object2camera The transform from the box space to the camera space.
   * \param projection Projection matrix to use for the camera. Typically, you
   * would want to use the one returned by iCamera::GetProjectionMatrix().
   * \param color Color of the line.
   */
  virtual void DrawBoxProjected (const csBox3& box, const csTransform& object2camera,
    const CS::Math::Matrix4& projection, int color) = 0;
};

/** @} */

#endif // __CS_IVIDEO_GRAPH2D_H__

