/*
    Copyright (C) 1998-2001 by Jorrit Tyberghein

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

#include "csplugincommon/opengl/glcanvascommon.h"

#include "iutil/cfgfile.h"
#include "iutil/eventq.h"
#include "iutil/objreg.h"
#include "ivaria/reporter.h"

#include "csutil/cfgacc.h"

namespace CS
{
  namespace PluginCommon
  {
    namespace GL
    {
      CanvasCommonBase::CanvasCommonBase ()
      {
        memset (currentFormat, 0, sizeof (currentFormat));
      }

      CanvasCommonBase::~CanvasCommonBase ()
      {
        CanvasClose ();
      }

      bool CanvasCommonBase::CanvasOpen ()
      {
        if (canvas_open) return true;
        if (!PluginCommon::CanvasCommonBase::CanvasOpen()) return false;

        Report (CS_REPORTER_SEVERITY_NOTIFY,
          "Using %s mode at resolution %dx%d.",
          FullScreen ? "full screen" : "windowed", fbWidth, fbHeight);

        {
          csString pfStr;
          GetPixelFormatString (currentFormat, pfStr);

          Report (CS_REPORTER_SEVERITY_NOTIFY,
            "Pixel format: %s", pfStr.GetData());
        }
        if (currentFormat[glpfvColorBits] < 24)
        {
          Report (CS_REPORTER_SEVERITY_WARNING,
            "WARNING! Crystal Space performs better in 24 or 32 bit display mode!");
        }

        return true;
      }

      void CanvasCommonBase::CanvasClose ()
      {
        if (!canvas_open) return;
        PluginCommon::CanvasCommonBase::CanvasClose ();
      }

      void CanvasCommonBase::GetPixelFormatString (const GLPixelFormat& format,
                              csString& str)
      {
        const char* valueNames[glpfvValueCount] = {"Color", "Alpha", "Depth",
          "Stencil", "AccumColor", "AccumAlpha", "MultiSamples"};

        str.Clear();
        for (int v = 0; v < glpfvValueCount; v++)
        {
          str.AppendFmt ("%s: %d ", valueNames[v], format[v]);
        }
      }

      void CanvasCommonBase::Report (int severity, const char* msg, ...)
      {
        va_list args;
        va_start (args, msg);
        csReportV (objectReg, severity,
          "crystalspace.canvas.openglcommon",
          msg, args);
        va_end (args);
      }

      bool CanvasCommonBase::CanvasResize (int width, int height)
      {
        if (!PluginCommon::CanvasCommonBase::CanvasResize (width, height))
          return false;

        return true;
      }

      //-----------------------------------------------------------------------

      CanvasCommonBase::csGLPixelFormatPicker::csGLPixelFormatPicker(
        CanvasCommonBase* parent) : parent (parent)
      {
        Reset();
      }

      CanvasCommonBase::csGLPixelFormatPicker::~csGLPixelFormatPicker()
      {
      }

      void CanvasCommonBase::csGLPixelFormatPicker::ReadStartValues (iConfigFile* config)
      {
        currentValues[glpfvColorBits] = parent->Depth;

        currentValues[glpfvAlphaBits] =
          config->GetInt ("Video.OpenGL.AlphaBits", 8);
        currentValues[glpfvDepthBits] =
          config->GetInt ("Video.OpenGL.DepthBits", 32);
        currentValues[glpfvStencilBits] =
          config->GetInt ("Video.OpenGL.StencilBits", 8);
        currentValues[glpfvAccumColorBits] =
          config->GetInt ("Video.OpenGL.AccumColorBits", 0);
        currentValues[glpfvAccumAlphaBits] =
          config->GetInt ("Video.OpenGL.AccumAlphaBits", 0);
        currentValues[glpfvMultiSamples] =
          config->GetInt ("Video.OpenGL.MultiSamples", 0);
        currentValid = true;
      }

      void CanvasCommonBase::csGLPixelFormatPicker::ReadPickerValues (iConfigFile* config)
      {
        const char* order = config->GetStr (
          "Video.OpenGL.FormatPicker.ReductionOrder", "ACmasdc");
        SetupIndexTable (order);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.ColorBits"),
          pixelFormats[pixelFormatIndexTable[glpfvColorBits]].possibleValues);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.AlphaBits"),
          pixelFormats[pixelFormatIndexTable[glpfvAlphaBits]].possibleValues);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.DepthBits"),
          pixelFormats[pixelFormatIndexTable[glpfvDepthBits]].possibleValues);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.StencilBits"),
          pixelFormats[pixelFormatIndexTable[glpfvStencilBits]].possibleValues);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.AccumColorBits"),
          pixelFormats[pixelFormatIndexTable[glpfvAccumColorBits]].possibleValues);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.AccumAlphaBits"),
          pixelFormats[pixelFormatIndexTable[glpfvAccumAlphaBits]].possibleValues);

        ReadPickerValue (config->GetStr (
          "Video.OpenGL.FormatPicker.MultiSamples"),
          pixelFormats[pixelFormatIndexTable[glpfvMultiSamples]].possibleValues);
      }

      template<class T>
      static int ReverseCompare(T const& r1, T const& r2)
      {
        return csComparator<T,T>::Compare(r2,r1);
      }

      void CanvasCommonBase::csGLPixelFormatPicker::ReadPickerValue (
        const char* valuesStr, csArray<int>& values)
      {
        if ((valuesStr != 0) && (*valuesStr != 0))
        {
          CS_ALLOC_STACK_ARRAY(char, myValues, strlen (valuesStr) + 1);
          strcpy (myValues, valuesStr);

          char* currentVal = myValues;
          while ((currentVal != 0) && (*currentVal != 0))
          {
            char* comma = strchr (currentVal, ',');
            if (comma != 0) *comma = 0;

            char dummy;
            int val;
            if (sscanf (currentVal, "%d%c", &val, &dummy) == 1)
            {
                values.Push (val);
            }
            currentVal = comma ? comma + 1 : 0;
          }
        }

        if (values.GetSize () == 0)
          values.Push (0);

        values.Sort (ReverseCompare<int>);
      }

      void CanvasCommonBase::csGLPixelFormatPicker::SetInitialIndices ()
      {
        for (size_t format = 0; format < glpfvValueCount; ++format)
        {
          size_t formatIdx = pixelFormatIndexTable[format];

          const csArray<int>& values = pixelFormats[formatIdx].possibleValues;

          size_t closestIndex = values.GetSize () - 1;

          for (size_t i = 0; i < values.GetSize (); ++i)
          {
            // find first which is less than value
            if (values[i] <= currentValues[format])
            {
              closestIndex = i;
              break;
            }
          }

          //pixelFormats[formatIdx].firstIndex = csMin (firstIndex, values.GetSize () - 1);
          pixelFormats[formatIdx].firstIndex = closestIndex;
          pixelFormats[formatIdx].nextIndex = pixelFormats[formatIdx].firstIndex;
        }
      }

      void CanvasCommonBase::csGLPixelFormatPicker::SetupIndexTable (
        const char* orderStr)
      {
        size_t orderIdx = 0;

        while (*orderStr != 0 && orderIdx < glpfvValueCount)
        {
          char orderVal = *orderStr;

          // Map character to value type
          GLPixelFormatValue val = glpfvColorBits;
          switch (orderVal)
          {
          case 'c':
            val = glpfvColorBits;
            break;
          case 'a':
            val = glpfvAlphaBits;
            break;
          case 'd':
            val = glpfvDepthBits;
            break;
          case 's':
            val = glpfvStencilBits;
            break;
          case 'C':
            val = glpfvAccumColorBits;
            break;
          case 'A':
            val = glpfvAccumAlphaBits;
            break;
          case 'm':
            val = glpfvMultiSamples;
            break;
          }

          //Now map orderIdx to val
          pixelFormatIndexTable[val] = orderIdx;

          //Set it to be this value too
          pixelFormats[orderIdx].valueType = val;

          orderStr++;
          orderIdx++;
        }
      }

      bool CanvasCommonBase::csGLPixelFormatPicker::PickNextFormat ()
      {
        for (size_t i = 0; i < glpfvValueCount; ++i)
        {
          currentValues[pixelFormats[i].valueType] =
            pixelFormats[i].possibleValues[pixelFormats[i].nextIndex];
        }

        // Increment
        bool incComplete = true;
        size_t indexToInc = 0;

        do
        {
          pixelFormats[indexToInc].nextIndex++;
          incComplete = true;

          if (pixelFormats[indexToInc].nextIndex >=
            pixelFormats[indexToInc].possibleValues.GetSize ())
          {
            //roll around
            pixelFormats[indexToInc].nextIndex = pixelFormats[indexToInc].firstIndex;
            incComplete = false;
            indexToInc++;
          }

        } while(!incComplete && indexToInc < glpfvValueCount);

        return incComplete;
      }

      void CanvasCommonBase::csGLPixelFormatPicker::Reset()
      {
        for (size_t v = 0; v < glpfvValueCount; v++)
        {
          pixelFormats[v].possibleValues.DeleteAll();
        }

        csConfigAccess config (parent->objectReg);
        ReadStartValues (config);
        ReadPickerValues (config);
        SetInitialIndices();
        PickNextFormat ();
      }

      bool CanvasCommonBase::csGLPixelFormatPicker::GetNextFormat (
        GLPixelFormat& format)
      {
        memcpy (format, currentValues, sizeof (GLPixelFormat));

        bool oldCurrentValid = currentValid;
        currentValid = PickNextFormat ();
        return oldCurrentValid;
      }
    } // namespace GL
  } // namespace PluginCommon
} // namespace CS
