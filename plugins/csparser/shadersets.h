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

#ifndef __CS_SHADERSETS_H__
#define __CS_SHADERSETS_H__

#include "csutil/csstring.h"
#include "csutil/strset.h"

struct iDocument;
struct iDocumentNode;
struct iLoaderContext;
struct iShader;

CS_PLUGIN_NAMESPACE_BEGIN(csparser)
{
  class csThreadedLoader;

  struct MaterialShaderMapping
  {
    csStringID shaderType;
    iShader* shader;
  };
  typedef csArray<MaterialShaderMapping> MaterialShaderMappingsArray;

  /**
   * Container class to manage shader sets.
   * A shader set is a collection of shader to shader type assignments,
   * applied to a material together.
   */
  class ShaderSets
  {
    /// Parse a shadersets XML &lt;shadersets&gt; node
    bool ParseShaderSets (iDocumentNode* shadersetsNode);
    /// Parse a shadersets XML &lt;shadersets&gt; -&gt; &lt;shaderset&gt; node
    bool ParseShaderSet (iDocumentNode* shadersetNode);
    /// Parse a shadersets XML &lt;shadersets&gt; -&gt; &lt;shaders&gt; node
    bool ParseShaders (iDocumentNode* shadersNode);
    /**
     * Parse a shadersets XML &lt;shadersets&gt; -&gt; &lt;shaders&gt;
     * -&gt; &lt;shader&gt; node
     */
    bool ParseShader (iDocumentNode* shaderNode);

    /**
     * Map between shader names and there source nodes, used in case a shader
     * has to be loaded
     */
    struct ShaderInfo
    {
      csRef<iDocumentNode> node;
      csString filename;
    };
    csHash<ShaderInfo, csString> set_shaders;
    /// Load a shader from a ShaderInfo
    csPtr<iShader> ShaderFromInfo (iLoaderContext* ldr_context,
                                   const ShaderInfo& info);

    /// Shader set entry parsed from XML
    struct ParsedShaderSetEntry
    {
      csStringID shaderType;
      csString shaderName;
    };
    /// Shader set parsed from XML
    typedef csArray<ParsedShaderSetEntry> ParsedShaderSet;
    /// All parsed shader sets
    csHash<ParsedShaderSet, csString> shaderSets;
  public:
    ShaderSets (csThreadedLoader* parent);

    /// Parse shader sets from an XML file
    bool ParseShaderSets (const char* vfsPath);
    /// Parse shader sets from a document. \a filename is used for error reporting
    bool ParseShaderSets (iDocument* doc, const char* filename);

    /// Append the shaders of the set \a setName to the mappings in \a array.
    bool ApplyShaderSet (iLoaderContext* ldr_context, const char* setName,
                         MaterialShaderMappingsArray& mappings);
  protected:
    csThreadedLoader* parent;
  };
}
CS_PLUGIN_NAMESPACE_END(csparser)

#endif // __CS_SHADERSETS_H__
