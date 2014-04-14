/*
    Copyright (C) 1998 by Jorrit Tyberghein
              (C) 2012 by Frank Richter

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

#ifndef __CS_GLCANVASCOMMON_H__
#define __CS_GLCANVASCOMMON_H__

/**\file
 * Common OpenGL canvas superclass.
 */

#if defined(CS_OPENGL_PATH)
#include CS_HEADER_GLOBAL(CS_OPENGL_PATH,gl.h)
#else
#include <GL/gl.h>
#endif

#include "iutil/event.h"
#include "csplugincommon/iopengl/canvas.h"
#include "csutil/scf.h"
#include "csplugincommon/canvas/canvascommon.h"
#include "driverdb.h"

#include "csextern_gl.h"

/**\addtogroup plugincommon
 * @{ */

namespace CS
{
  namespace PluginCommon
  {
    namespace GL
    {
      /**
       * Basic OpenGL version of the graphics canvas class.
       */
      class CS_CSPLUGINCOMMON_GL_EXPORT CanvasCommonBase :
        public virtual PluginCommon::CanvasCommonBase,
        public virtual iOpenGLCanvas
      {
      public:
        enum GLPixelFormatValue
        {
          glpfvColorBits = 0,
          glpfvAlphaBits,
          glpfvDepthBits,
          glpfvStencilBits,
          glpfvAccumColorBits,
          glpfvAccumAlphaBits,
          glpfvMultiSamples,

          glpfvValueCount
        };
        typedef int GLPixelFormat[glpfvValueCount];
      protected:
        class CS_CSPLUGINCOMMON_GL_EXPORT csGLPixelFormatPicker
        {
          CanvasCommonBase* parent;

          /*
          size_t nextValueIndices[glpfvValueCount];
          csArray<int> values[glpfvValueCount];


          char* order;
          size_t orderPos;
          size_t orderNum;*/

          // Hold properties for a single pixelformat property
          struct PixelFormatPropertySet
          {
            GLPixelFormatValue valueType;
            size_t nextIndex;
            size_t firstIndex;
            csArray<int> possibleValues;
          };

          /* Pixel format properties, however this is _not_ indexed by
          GLPixelFormatValue but sorted by order */
          PixelFormatPropertySet pixelFormats[glpfvValueCount];

          // Remapping table from real GLPixelFormatValue to index in table above
          size_t pixelFormatIndexTable[glpfvValueCount];

          GLPixelFormat currentValues;
          bool currentValid;

          void ReadStartValues (iConfigFile* config);
          void ReadPickerValues (iConfigFile* config);
          void ReadPickerValue (const char* valuesStr, csArray<int>& values);
          void SetInitialIndices ();
          void SetupIndexTable (const char* orderStr);
          bool PickNextFormat ();
        public:
          csGLPixelFormatPicker (CanvasCommonBase* parent);
          ~csGLPixelFormatPicker ();

          void Reset();
          bool GetNextFormat (GLPixelFormat& format);
        };
        friend class csGLPixelFormatPicker;

        GLPixelFormat currentFormat;

        void GetPixelFormatString (const GLPixelFormat& format, csString& str);

        void Report (int severity, const char* msg, ...);
      public:
        /**
         * Constructor does little, most initialization stuff happens in
         * Initialize().
         */
        CanvasCommonBase ();

        /// Clear font cache etc.
        virtual ~CanvasCommonBase ();

        /*
        * You must supply all the functions not supplied here, such as
        * SetMouseCursor etc. Note also that even though Initialize, Open,
        * and Close are supplied here, you must still override these functions
        * for your own subclass to make system-specific calls for creating and
        * showing windows, etc.
        */

        virtual bool CanvasOpen ();

        virtual void CanvasClose ();

        bool CanvasResize (int width, int height);

        /**\name iOpenGLCanvas implementation
        * @{ */
        int GetMultiSamples() { return currentFormat[glpfvMultiSamples]; }
        /** @} */
      };
    } // namespace GL
  } // namespace PluginCommon
} // namespace CS

/** @} */

#endif // __CS_GLCANVASCOMMON_H__
