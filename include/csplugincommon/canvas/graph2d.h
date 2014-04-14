/*
    Copyright (C) 1998-2001 by Jorrit Tyberghein
    Written by Andrew Zabolotny <bit@eltech.ru>

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

#ifndef __CS_CSPLUGINCOMMON_CANVAS_GRAPH2D_H__
#define __CS_CSPLUGINCOMMON_CANVAS_GRAPH2D_H__

/**\file
 * Base class for 2D canvases.
 */

#include "csextern.h"

#include "canvascommon.h"

#include "csutil/cfgacc.h"
#include "csutil/scf.h"
#include "csutil/scf_implementation.h"
#include "csutil/weakref.h"

#include "iutil/comp.h"
#include "iutil/dbghelp.h"
#include "iutil/eventh.h"
#include "iutil/plugin.h"
#include "iutil/pluginconfig.h"
#include "iutil/string.h"
#include "ivideo/fontserv.h"
#include "ivideo/graph2d.h"
#include "ivideo/natwin.h"

/**
 * \addtogroup plugincommon
 * @{ */

struct iObjectRegistry;
struct iPluginManager;

class csFontCache;

#include "csutil/deprecated_warn_off.h"

namespace CS
{
  namespace PluginCommon
  {
    /**
     * This is the base class for iGraphics2D implementations.
     */
    class CS_CRYSTALSPACE_EXPORT Graphics2DCommon :
      public virtual iGraphics2D,
      public virtual iComponent,
      public virtual iEventHandler
    {
    public:
      /// The clipping rectangle.
      int ClipX1, ClipX2, ClipY1, ClipY2;

      /// Open/Close state.
      bool is_open;
      /// Resize event for the canvas we're associated with
      csEventID evCanvasResize;

      /// The object registry.
      iObjectRegistry* object_reg;
      /// The plugin manager.
      csWeakRef<iPluginManager> plugin_mgr;

      /// The font server
      csWeakRef<iFontServer> FontServer;
      /// The font cache
      csFontCache* fontCache;

      int vpLeft, vpTop, vpWidth, vpHeight;
      bool viewportIsFullScreen;

      /**
      * The counter that is incremented inside BeginDraw and decremented in
      * FinishDraw().
      */
      int FrameBufferLocked;
    protected:
      csRef<iEventHandler> weakEventHandler;

      virtual iGraphicsCanvas* GetCanvas() = 0;

      /// Handle a resize event from the canvas.
      void HandleResize ();
    public:
      /// Create csGraphics2D object
      Graphics2DCommon ();

      /// Destroy csGraphics2D object
      virtual ~Graphics2DCommon ();

      /// Initialize the plugin
      virtual bool Initialize (iObjectRegistry*);
      /// Event handler for plugin.
      virtual bool HandleEvent (iEvent&);

      /// (*) Open graphics system (set videomode, open window etc)
      virtual bool Open ();
      /// (*) Close graphics system
      virtual void Close ();

      virtual int GetWidth () { return vpWidth; }
      virtual int GetHeight () { return vpHeight; }

      /// Set clipping rectangle
      virtual void SetClipRect (int xmin, int ymin, int xmax, int ymax);
      /// Query clipping rectangle
      virtual void GetClipRect (int &xmin, int &ymin, int &xmax, int &ymax);

      /**
      * This routine should be called before any draw operations.
      * It should return true if graphics context is ready.
      */
      virtual bool BeginDraw ();
      /// This routine should be called when you finished drawing
      virtual void FinishDraw ();

      /// Clear backbuffer
      virtual void Clear (int color);
      /// Clear all video pages
      virtual void ClearAll (int color);

      virtual int FindRGB (int r, int g, int b, int a = 255)
      {
        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;
        if (a < 0) a = 0; else if (a > 255) a = 255;
        return ((255 - a) << 24) | (r << 16) | (g << 8) | b;
        /* Alpha is "inverted" so '-1' can be decomposed to a
          transparent color. (But alpha not be inverted, '-1'
          would be "opaque white". However, -1 is the color
          index for "transparent text background". */
      }
      virtual void GetRGB (int color, int& r, int& g, int& b)
      {
        r = (color >> 16) & 0xff;
        g = (color >> 8) & 0xff;
        b = color & 0xff;
      }
      virtual void GetRGB (int color, int& r, int& g, int& b, int& a)
      {
        a = 255 - (color >> 24);
        GetRGB (color, r, g, b);
      }

      //@{
      /// Write a text string into the back buffer
      virtual void Write (iFont *font , int x, int y, int fg, int bg,
        const char *text, uint flags = 0);
      virtual void Write (iFont *font , int x, int y, int fg, int bg,
        const wchar_t* text, uint flags = 0);
      //@}

    private:
        /// helper function for ClipLine()
      bool CLIPt (float denom, float num, float& tE, float& tL);
    public:

      /**
      * Clip a line against given rectangle
      * Function returns true if line is not visible
      */
      virtual bool ClipLine (float &x1, float &y1, float &x2, float &y2,
        int xmin, int ymin, int xmax, int ymax);

      /// Gets the font server
      virtual iFontServer *GetFontServer ()
      { return FontServer; }

      /**
      * Perform a system specific extension. Return false if extension
      * not supported.
      */
      virtual bool PerformExtensionV (char const* command, va_list);

      bool Resize (int w, int h);

      void SetViewport (int left, int top, int width, int height);
      void GetViewport (int& left, int& top, int& width, int& height)
      { left = vpLeft; top = vpTop; width = vpWidth; height = vpHeight; }

      const char* GetHWRenderer ()
      { return 0; }
      const char* GetHWGLVersion ()
      { return 0; }
      const char* GetHWVendor ()
      { return 0; }

      CS_EVENTHANDLER_NAMES("crystalspace.graphics2d.common")
      CS_EVENTHANDLER_NIL_CONSTRAINTS
    };
  } // namespace PluginCommon
} // namespace CS

/**
 * This is the base class for 2D canvases. Plugins should derive their 
 * own class from this one and implement required (marked with an 
 * asterisk (*)) functions.
 * Functions not marked with an asterisk are optional, but possibly
 * slow since they are too general.
 */
class CS_CRYSTALSPACE_EXPORT csGraphics2D : 
  public scfImplementation7<csGraphics2D, 
    scfFakeInterface<iGraphics2D>,
    scfFakeInterface<iComponent>,
    scfFakeInterface<iNativeWindow>,
    scfFakeInterface<iNativeWindowManager>,
    scfFakeInterface<iPluginConfig>,
    iDebugHelper,
    scfFakeInterface<iEventHandler> >,
  public virtual CS::PluginCommon::Graphics2DCommon,
  public virtual CS::PluginCommon::CanvasCommonBase
{
protected:
  iGraphicsCanvas* GetCanvas() { return this; }
public:
  /// The configuration file.
  csConfigAccess config;

  /// Create csGraphics2D object
  csGraphics2D (iBase*);

  /// Destroy csGraphics2D object
  virtual ~csGraphics2D ();

  /// Initialize the plugin
  virtual bool Initialize (iObjectRegistry*);
protected:
  /**\name iDebugHelper implementation
   * @{ */
  virtual bool DebugCommand (const char* cmd);
  virtual int GetSupportedTests () const { return 0; }
  virtual csPtr<iString> UnitTest () { return 0; }
  virtual csPtr<iString> StateTest ()  { return 0; }
  virtual csTicks Benchmark (int /*num_iterations*/) { return 0; }
  virtual csPtr<iString> Dump ()  { return 0; }
  virtual void Dump (iGraphics3D* /*g3d*/)  { }
  /** @} */
};

#include "csutil/deprecated_warn_on.h"

/** @} */

#endif // __CS_CSPLUGINCOMMON_CANVAS_GRAPH2D_H__
