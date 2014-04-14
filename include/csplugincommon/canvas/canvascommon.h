/*
    Copyright (C) 1998-2001 by Jorrit Tyberghein
              (C) 2012 by Frank Richter
    Originally written by Andrew Zabolotny <bit@eltech.ru>

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

#ifndef __CS_CSPLUGINCOMMON_CANVAS_CANVASCOMMON_H__
#define __CS_CSPLUGINCOMMON_CANVAS_CANVASCOMMON_H__

/**\file
 * Base class for graphics canvases.
 */

#include "csextern.h"

#include "iutil/event.h"
#include "iutil/pluginconfig.h"
#include "ivideo/canvas.h"
#include "ivideo/natwin.h"

/**
 * \addtogroup plugincommon
 * @{ */

struct iEventOutlet;
struct iObjectRegistry;

namespace CS
{
  namespace PluginCommon
  {

    #include "csutil/deprecated_warn_off.h"

    /**
     * This is the base class for graphics canvases. Plugins should derive their
     * own class from this one and implement required (marked with an
     * asterisk (*)) functions.
     * Functions not marked with an asterisk are optional, but possibly
     * slow since they are too general.
     * \remarks Note that this class only provides implementations for certain
     * interfaces, but not SCF facilities (QueryInterface etc.) - these have to
     * be added by derived classes.
     */
    class CS_CRYSTALSPACE_EXPORT CanvasCommonBase :
      public virtual iGraphicsCanvas,
      public iNativeWindow,
      public iNativeWindowManager,
      public iPluginConfig,
      public iEventPlug
    {
    public:
      /// The object registry.
      iObjectRegistry* objectReg;
      /// Open/Close state.
      bool canvas_open;

      /// Pointer to a title.
      csString win_title;

      /// The width, height and depth of visual.
      int fbWidth, fbHeight, Depth;

      /**
       * Display number.  If 0, use primary display; else if greater than 0, use
       * that display number.  If that display number is not present, use primary
       * display.
       */
      int DisplayNumber;
      /// True if visual is full-screen.
      bool FullScreen;
      /// Whether to allow resizing.
      bool AllowResizing;
      /**
       * Change the depth of the canvas.
       */
      virtual void ChangeDepth (int d);
      /**
       * Get the name of this canvas
       */
      virtual const char *GetName() const;

      /// Hardware mouse cursor setting
      enum HWMouseMode
      {
        /// Never use hardware cursor
        hwmcOff,
        /// Always use hardware cursor, if possible
        hwmcOn,
        /// Only use hardware cursor if true RGBA cursor is available
        hwmcRGBAOnly
      };
      HWMouseMode hwMouse;
    protected:
      /// Screen refresh rate
      int refreshRate;
      /// Activate Vsync
      bool vsync;
      /// Reduce window size to fit into workspace, if necessary
      bool fitToWorkingArea;

      csString name;

      /// Read configuration
      void Initialize (iObjectRegistry* object_reg);

      /**
       * Helper function for FitSizeToWorkingArea(): obtain workspace dimensions.
       */
      virtual bool GetWorkspaceDimensions (int& width, int& height);
      /**
       * Helper function for FitSizeToWorkingArea(): compute window dimensions
       * with the window frame included.
       */
      virtual bool AddWindowFrameDimensions (int& width, int& height);
    public:
      CanvasCommonBase ();
      virtual ~CanvasCommonBase ();

      /// (*) Open graphics system (set videomode, open window etc)
      virtual bool CanvasOpen ();
      /// (*) Close graphics system
      virtual void CanvasClose ();

      /// (*) Flip video pages (or dump backbuffer into framebuffer).
      virtual void Print (csRect const* /*area*/ = 0) { }

      virtual bool SetGamma (float /*gamma*/) { return false; }
      virtual float GetGamma () const { return 1.0; }

    public:
      /// The event plug object
      csRef<iEventOutlet> EventOutlet;

      int GetColorDepth () { return Depth; }

      /// Enable/disable canvas resize (Over-ride in sub classes)
      virtual void AllowResize (bool /*iAllow*/) { };

      /// Resize the canvas
      virtual bool CanvasResize (int w, int h);

      /// Return the Native Window interface for this canvas (if it has one)
      virtual iNativeWindow* GetNativeWindow ();

      /// Returns 'true' if the program is being run full-screen.
      virtual bool GetFullScreen ()
      { return FullScreen; }

      /**
       * Change the fullscreen state of the canvas.
       */
      virtual void SetFullScreen (bool b);

      /// Set mouse cursor position; return success status
      virtual bool SetMousePosition (int x, int y);

      /**
       * Set mouse cursor to one of predefined shape classes
       * (see csmcXXX enum above). If a specific mouse cursor shape
       * is not supported, return 'false'; otherwise return 'true'.
       * If system supports it and iBitmap != 0, shape should be
       * set to the bitmap passed as second argument; otherwise cursor
       * should be set to its nearest system equivalent depending on
       * iShape argument.
       */
      virtual bool SetMouseCursor (csMouseCursorID iShape);

      /**
       * Set mouse cursor using an image.  If the operation is unsupported,
       * return 'false' otherwise return 'true'.
       * On some platforms there is only monochrome pointers available.  In this
       * all black colors in the image will become the value of 'bg' and all
       * non-black colors will become 'fg'
       */
      virtual bool SetMouseCursor (iImage *image, const csRGBcolor* keycolor = 0,
                                   int hotspot_x = 0, int hotspot_y = 0,
                                   csRGBcolor fg = csRGBcolor(255,255,255),
                                   csRGBcolor bg = csRGBcolor(0,0,0));

      void GetFramebufferDimensions (int& width, int& height)
      { width = fbWidth; height = fbHeight; }
    protected:
      /**\name iNativeWindowManager implementation
      * @{ */
      // Virtual Alert function so it can be overridden by subclasses
      // of csGraphics2D.
      virtual void AlertV (int type, const char* title, const char* okMsg,
        const char* msg, va_list args);
      virtual void Alert (int type, const char* title, const char* okMsg,
          const char* msg, ...);
      virtual void AlertV (int type, const wchar_t* title, const wchar_t* okMsg,
        const wchar_t* msg, va_list args);
      virtual void Alert (int type, const wchar_t* title, const wchar_t* okMsg,
          const wchar_t* msg, ...);
      /** @} */

      /**\name iNativeWindow implementation
      * @{ */
      // Virtual SetTitle function so it can be overridden by subclasses
      // of csGraphics2D.
      virtual void SetTitle (const char* title);
      virtual void SetTitle (const wchar_t* title)
      { SetTitle (csString (title)); }

      /** Sets the icon of this window with the provided one.
       *
       *  @note Virtual SetIcon function so it can be overridden by subclasses of csGraphics2D.
       *  @param image the iImage to set as the icon of this window.
       */
      virtual void SetIcon (iImage *image);

      virtual bool IsWindowTransparencyAvailable() { return false; }
      virtual bool SetWindowTransparent (bool transparent) { return false; }
      virtual bool GetWindowTransparent () { return false; }

      virtual bool SetWindowDecoration (WindowDecoration decoration, bool flag)
      { return false; }
      virtual bool GetWindowDecoration (WindowDecoration decoration);

      virtual bool FitSizeToWorkingArea (int& desiredWidth,
                                         int& desiredHeight);
      /** @} */

      /**\name iPluginConfig implementation
      * @{ */
      virtual bool GetOptionDescription (int idx, csOptionDescription*);
      virtual bool SetOption (int id, csVariant* value);
      virtual bool GetOption (int id, csVariant* value);
      /** @} */

      /**\name iEventPlug implementation
      * @{ */
      virtual unsigned GetPotentiallyConflictingEvents ()
      { return CSEVTYPE_Keyboard | CSEVTYPE_Mouse; }
      virtual unsigned QueryEventPriority (unsigned /*iType*/)
      { return 150; }
      /** @} */

    };

    #include "csutil/deprecated_warn_on.h"
  } // namespace PluginCommon
} // namespace CS

/** @} */

#endif // __CS_CSPLUGINCOMMON_CANVAS_CANVASCOMMON_H__
