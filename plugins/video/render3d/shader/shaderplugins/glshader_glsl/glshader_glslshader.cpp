/*
  Copyright (C) 2011 by Antony Martin

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"

#include "csplugincommon/opengl/glextmanager.h"

#include "glshader_glsl.h"
#include "glshader_glslshader.h"

CS_PLUGIN_NAMESPACE_BEGIN(GLShaderGLSL)
{
  CS_LEAKGUARD_IMPLEMENT (csShaderGLSLShader);

  bool csShaderGLSLShader::Compile (const char *source, bool doVerbose)
  {
    GLint status;                   // compile status
    const csGLExtensionManager* ext = shaderPlug->ext;

    shader_id = ext->glCreateShaderObjectARB (type);
    ext->glShaderSourceARB (shader_id, 1, &source, NULL);
    ext->glCompileShaderARB (shader_id);

    ext->glGetObjectParameterivARB (shader_id, GL_OBJECT_COMPILE_STATUS_ARB,
      &status);
    if (doVerbose)
    {
      if (!status)
      {
        shaderPlug->Report (CS_REPORTER_SEVERITY_WARNING,
          "Couldn't compile %s", CS::Quote::Single (typeName));
      }
      GLint logSize (0);

      ext->glGetObjectParameterivARB (shader_id, GL_OBJECT_INFO_LOG_LENGTH_ARB,
        &logSize);
      CS_ALLOC_STACK_ARRAY(char, infoLog, logSize+1);
      ext->glGetInfoLogARB (shader_id, logSize, NULL, (GLcharARB*)infoLog);

      if ((logSize > 0) && (strlen (infoLog) > 0))
      {
        int severity = !status ? CS_REPORTER_SEVERITY_WARNING
                               : CS_REPORTER_SEVERITY_NOTIFY;
        shaderPlug->Report (severity, "Info log for %s: %s", 
          CS::Quote::Single (typeName),
          infoLog);
      }
    }

    return status != GL_FALSE;
  }
}
CS_PLUGIN_NAMESPACE_END(GLShaderGLSL)
