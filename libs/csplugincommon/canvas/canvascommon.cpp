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

#include "cssysdef.h"

#include "csplugincommon/canvas/canvascommon.h"

#include "iutil/cfgfile.h"
#include "iutil/cmdline.h"
#include "iutil/eventq.h"
#include "iutil/objreg.h"
#include "csutil/cfgacc.h"
#include "csutil/eventnames.h"
#include "csutil/sysfunc.h"

namespace CS
{
  namespace PluginCommon
  {

    CanvasCommonBase::CanvasCommonBase () :
      objectReg (nullptr), hwMouse (hwmcOff), fitToWorkingArea (false)
    {
      static uint g2d_count = 0;

      fbWidth = 640;
      fbHeight = 480;
      Depth = 16;
      DisplayNumber = 0;
      FullScreen = false;
      canvas_open = false;
      win_title = "Crystal Space Application";
      AllowResizing = false;
      refreshRate = 0;
      vsync = false;

      name.Format ("graph2d.%x", g2d_count++);
    }

    CanvasCommonBase::~CanvasCommonBase ()
    {
      CanvasClose ();
    }

    void CanvasCommonBase::Initialize (iObjectRegistry* object_reg)
    {
      this->objectReg = object_reg;

      // Get the system parameters
      csConfigAccess config (objectReg);
      fbWidth = config->GetInt ("Video.ScreenWidth", fbWidth);
      fbHeight = config->GetInt ("Video.ScreenHeight", fbHeight);
      Depth = config->GetInt ("Video.ScreenDepth", Depth);
      FullScreen = config->GetBool ("Video.FullScreen", FullScreen);
      fitToWorkingArea = config->GetBool ("Video.FitToWorkingArea", fitToWorkingArea);
      DisplayNumber = config->GetInt ("Video.DisplayNumber", DisplayNumber);
      refreshRate = config->GetInt ("Video.DisplayFrequency", 0);
      vsync = config->GetBool ("Video.VSync", false);

      const char* hwMouseFlag = config->GetStr ("Video.SystemMouseCursor", "yes");
      if ((strcasecmp (hwMouseFlag, "yes") == 0)
          || (strcasecmp (hwMouseFlag, "true") == 0)
          || (strcasecmp (hwMouseFlag, "on") == 0)
          || (strcmp (hwMouseFlag, "1") == 0))
      {
        hwMouse = hwmcOn;
      }
      else if (strcasecmp (hwMouseFlag, "rgbaonly") == 0)
      {
        hwMouse = hwmcRGBAOnly;
      }
      else
      {
        hwMouse = hwmcOff;
      }
      csRef<iCommandLineParser> cmdline (
        csQueryRegistry<iCommandLineParser> (object_reg));
      if (cmdline->GetOption ("sysmouse") || cmdline->GetOption ("nosysmouse"))
      {
        hwMouse = cmdline->GetBoolOption ("sysmouse") ? hwmcOn : hwmcOff;
      }

      csRef<iEventQueue> q (csQueryRegistry<iEventQueue> (object_reg));
      if (q != 0)
        EventOutlet = q->CreateEventOutlet (this);
    }

    void CanvasCommonBase::ChangeDepth (int d)
    {
      if (Depth == d) return;
      Depth = d;
    }

    const char* CanvasCommonBase::GetName() const
    {
      return name;
    }

    bool CanvasCommonBase::CanvasOpen ()
    {
      if (canvas_open) return true;

      if (!FullScreen && fitToWorkingArea)
      {
        int newWidth, newHeight;
        GetFramebufferDimensions (newWidth, newHeight);
        if (FitSizeToWorkingArea (newWidth, newHeight))
        {
          bool oldResize (AllowResizing);
          AllowResizing = true;
          CanvasResize (newWidth, newHeight);
          AllowResizing = oldResize;
        }
      }

      canvas_open = true;
      return true;
    }

    void CanvasCommonBase::CanvasClose ()
    {
      if (!canvas_open) return;
      canvas_open = false;
    }

    void CanvasCommonBase::AlertV (int type, const char* title, const char* okMsg,
        const char* msg, va_list arg)
    {
      (void)type; (void)title; (void)okMsg;
      csPrintf ("ALERT: ");
      csPrintfV (msg, arg);
      csPrintf ("\n");
      fflush (stdout);
    }

    void CanvasCommonBase::Alert (int type, const char* title, const char* okMsg,
                  const char* msg, ...)
    {
      va_list arg;
      va_start (arg, msg);
      AlertV (type, title, okMsg, msg, arg);
      va_end (arg);
    }

    void CanvasCommonBase::Alert (int type, const wchar_t* title, const wchar_t* okMsg,
                  const wchar_t* msg, ...)
    {
      va_list arg;
      va_start (arg, msg);
      AlertV (type, csString (title), csString (okMsg), csString (msg), arg);
      va_end (arg);
    }

    void CanvasCommonBase::AlertV (int type, const wchar_t* title, const wchar_t* okMsg,
        const wchar_t* msg, va_list arg)
    {
      AlertV (type, csString (title), csString (okMsg), csString (msg), arg);
    }

    iNativeWindow* CanvasCommonBase::GetNativeWindow ()
    {
      return static_cast<iNativeWindow*> (this);
    }

    void CanvasCommonBase::SetTitle (const char* title)
    {
      win_title = title;
    }

    void CanvasCommonBase::SetIcon (iImage *image)
    {

    }

    bool CanvasCommonBase::CanvasResize (int w, int h)
    {
      if (!canvas_open)
      {
        // Still in Initialization phase, configuring size of canvas
        fbWidth = w;
        fbHeight = h;
        return true;
      }

      if (!AllowResizing)
        return false;

      fbWidth = w;
      fbHeight = h;

      if (EventOutlet)
        /* CanvasCommonBase::Open() causes a resize due to fitting to the working area;
        * don't emit a resize event in that case */
        EventOutlet->Broadcast (csevCanvasResize(objectReg, this), (intptr_t)this);
      return true;
    }

    void CanvasCommonBase::SetFullScreen (bool b)
    {
      if (FullScreen == b) return;
      FullScreen = b;
    }

    bool CanvasCommonBase::SetMousePosition (int x, int y)
    {
      (void)x; (void)y;
      return false;
    }

    bool CanvasCommonBase::SetMouseCursor (csMouseCursorID iShape)
    {
      return (iShape == csmcArrow);
    }

    bool CanvasCommonBase::SetMouseCursor (iImage *, const csRGBcolor*, int, int,
                                      csRGBcolor, csRGBcolor)
    {
      return false;
    }

    bool CanvasCommonBase::GetWindowDecoration (WindowDecoration decoration)
    {
      // Decorations that are commonly on
      switch (decoration)
      {
      case decoCaption:
        return !FullScreen;
      default:
        break;
      }

      // Everything else: assume off
      return false;
    }

    bool CanvasCommonBase::GetWorkspaceDimensions (int& width, int& height)
    {
      return false;
    }

    bool CanvasCommonBase::AddWindowFrameDimensions (int& width, int& height)
    {
      return false;
    }

    bool CanvasCommonBase::FitSizeToWorkingArea (int& desiredWidth,
                                                 int& desiredHeight)
    {
      int wswidth, wsheight;
      if (!GetWorkspaceDimensions (wswidth, wsheight))
        return false;
      int framedWidth (desiredWidth), framedHeight (desiredHeight);
      if (!AddWindowFrameDimensions (framedWidth, framedHeight))
        return false;
      if (framedWidth > wswidth)
      {
        desiredWidth -= (framedWidth - wswidth);
      }
      if (framedHeight > wsheight)
      {
        desiredHeight -= (framedHeight - wsheight);
      }
      return true;
    }

    //---------------------------------------------------------------------------

    #define NUM_OPTIONS 3

    static const csOptionDescription config_options [NUM_OPTIONS] =
    {
      csOptionDescription( 0, "depth", "Display depth", CSVAR_LONG ),
      csOptionDescription( 1, "fs", "Fullscreen if available", CSVAR_BOOL ),
      csOptionDescription( 2, "mode", "Window size or resolution", CSVAR_STRING )
    };

    bool CanvasCommonBase::SetOption (int id, csVariant* value)
    {
      if (value->GetType () != config_options[id].type)
        return false;
      switch (id)
      {
        case 0: ChangeDepth (value->GetLong ()); break;
        case 1: SetFullScreen (value->GetBool ()); break;
        case 2:
        {
          const char* buf = value->GetString ();
          int wres, hres;
          if (sscanf (buf, "%dx%d", &wres, &hres) == 2)
            CanvasResize (wres, hres);
          break;
        }
        default: return false;
      }
      return true;
    }

    bool CanvasCommonBase::GetOption (int id, csVariant* value)
    {
      switch (id)
      {
        case 0: value->SetLong (Depth); break;
        case 1: value->SetBool (FullScreen); break;
        case 2:
        {
          csString buf;
          int w, h;
          GetFramebufferDimensions (w, h);
          buf.Format ("%dx%d", w, h);
          value->SetString (buf);
          break;
        }
        default: return false;
      }
      return true;
    }

    bool CanvasCommonBase::GetOptionDescription (int idx, csOptionDescription* option)
    {
      if (idx < 0 || idx >= NUM_OPTIONS)
        return false;
      *option = config_options[idx];
      return true;
    }

  } // namespace PluginCommon
} // namespace CS
