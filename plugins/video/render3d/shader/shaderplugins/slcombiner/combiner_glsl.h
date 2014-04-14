/*
  Copyright(C) 2004-2011 by Frank Richter

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

#ifndef __COMBINER_GLSL_H__
#define __COMBINER_GLSL_H__

#include "combiner_common.h"

#include "csplugincommon/shader/weavertypes.h"
#include "csutil/set.h"

CS_PLUGIN_NAMESPACE_BEGIN(SLCombiner)
{
  namespace WeaverCommon = CS::PluginCommon::ShaderWeaver;

  struct FindCoerceChainHelper;

  class ShaderCombinerLoaderGLSL :
    public scfImplementationExt0<ShaderCombinerLoaderGLSL,
                                 ShaderCombinerLoaderCommon>
  {
  public:
    ShaderCombinerLoaderGLSL (iBase *parent);
    
    /**\name CS::PluginCommon::ShaderWeaver::iCombinerLoader implementation
     * @{ */
    csPtr<WeaverCommon::iCombiner> GetCombiner (iDocumentNode* params);

    void GenerateConstantInputBlocks (iDocumentNode* node,
      const char* locationPrefix, const csVector4& value,
      int usedComponents, const char* outputName);
    void GenerateSVInputBlocks (iDocumentNode* node,
      const char* locationPrefix, const char* svName,
      const char* outputType, const char* outputName,
      const char* uniqueTag);
    void GenerateBufferInputBlocks (iDocumentNode* node,
      const char* locationPrefix, const char* bufName,
      const char* outputType, const char* outputName,
      const char* uniqueTag);
    /** @} */

    /**\name iComponent implementation
     * @{ */
    bool Initialize (iObjectRegistry* reg);
    /** @} */
  protected:
    const char* GetMessageID() const;
    const char* CoercionInputName () { return "value"; }
    const char* CoercionResultName () { return "result"; }
  };
  
  class ShaderCombinerGLSL :
    public scfImplementationExt0<ShaderCombinerGLSL, ShaderCombinerCommon>
  {
    csRef<ShaderCombinerLoaderGLSL> loader;
  public:
    ShaderCombinerGLSL (ShaderCombinerLoaderGLSL* loader);

    /**\name CS::PluginCommon::ShaderWeaver::iCombiner implementation
     * @{ */
    void BeginSnippet (const char* annotation = 0);
    void AddInput (const char* name, const char* type);
    void AddInputValue (const char* name, const char* type,
      const char* value);
    void AddOutput (const char* name, const char* type);
    void InputRename (const char* fromName, const char* toName);
    void OutputRename (const char* fromName, const char* toName);
    void PropagateAttributes (const char* fromInput, const char* toOutput);
    void AddOutputAttribute (const char* outputName,  const char* name,
      const char* type);
    void AddInputAttribute (const char* inputName, const char* name,
      const char* type, const char* defVal);
    void Link (const char* fromName, const char* toName);
    bool WriteBlock (const char* location, iDocumentNode* blockNodes);
    bool EndSnippet ();

    void AddGlobal (const char* name, const char* type,
          const char* annotation = 0);
    void SetOutput (csRenderTargetAttachment target,
      const char* name, const char* annotation = 0);

    csPtr<WeaverCommon::iCoerceChainIterator> QueryCoerceChain (
      const char* fromType, const char* toType);

    uint CoerceCost (const char* fromType, const char* toType);

    void WriteToPass (iDocumentNode* pass);

    bool CompatibleParams (iDocumentNode* params);

    csRef<iString> QueryInputTag (const char* location,
      iDocumentNode* blockNodes);
    /** @} */
  protected:
    void AppendComment (csString& s, const char* contents);

    struct Snippet : public CommonSnippet
    {
      csRefArray<iDocumentNode> vert2frag;
      csRefArray<iDocumentNode> inputs;

      csSet<csString> localIDs;
      csHash<csString, csString> v2fMaps;
      csString locals;
      csHash<csString, csString> inputMaps;
      csHash<const WeaverCommon::TypeInfo*, csString> inputMapsTypes;
      csRefArray<iDocumentNode> vertexBody;
      csRefArray<iDocumentNode> fragmentBody;
      csHash<csString, csString> outputMaps;
      csHash<const WeaverCommon::TypeInfo*, csString> outputMapsTypes;
      csString links;

      csHash<csString, csString> attrInputMaps;
      csHash<const WeaverCommon::TypeInfo*, csString> attrInputMapsTypes;
      csHash<csString, csString> attrOutputMaps;
      csHash<const WeaverCommon::TypeInfo*, csString> attrOutputMapsTypes;
    };
    size_t uniqueCounter;
    csArray<Snippet> snippets;
    Snippet currentSnippet;
    csSet<csString> globalIDs;
    csString globals;
    csHash<const WeaverCommon::TypeInfo*, csString> globalsTypes;
    csRefArray<iDocumentNode> variableMaps;
    csString outputAssign[rtaNumAttachments];
    csRefArray<iDocumentNode> definitions;
    /// Required GLSL version
    int requiredVersion;

    struct Attribute
    {
      csString name;
      csString type;
    };
    typedef csArray<Attribute> AttributeArray;
    Attribute* FindAttr (AttributeArray& arr, const char* name,
      const char* type);
    csHash<AttributeArray, csString> attributes;

    /// Shader program type
    enum ProgramType
    {
      progVP, progFP
    };
    void AppendProgramInput (const csRefArray<iDocumentNode>& nodes,
      DocNodeAppender& appender, ProgramType progType = progVP);
    void AppendProgramInput_V2FDecl (const Snippet& snippet, ProgramType progType,
      DocNodeAppender& appender);
    void AppendProgramInput_V2FLocals (const Snippet& snippet,
      DocNodeAppender& appender);
    void AppendProgramInput_V2FVP (const Snippet& snippet,
      DocNodeAppender& appender);
    void AppendProgramInput_V2FFP (const Snippet& snippet,
      DocNodeAppender& appender);
    void AppendProgramInput (iDocumentNode* node, DocNodeAppender& appender,
                             ProgramType progType = progVP);
    void AppendSnippetMap (const csHash<csString, csString>& map,
                           const csHash<const WeaverCommon::TypeInfo*, csString>& fromTypeMap,
                           DocNodeAppender& appender);
    void AppendSnippetMapCleanup (const csHash<csString, csString>& map,
                                  const csHash<const WeaverCommon::TypeInfo*, csString>& fromTypeMap,
                                  DocNodeAppender& appender);
    
    /**
     * Append program body nodes. Deals with \c \#sampler_assign
     * pseudo-directives in text nodes.
     */
    void AppendProgramBody (const csRefArray<iDocumentNode>& nodes,
      Snippet& snippet, DocNodeAppender& appender);

    /// Process \c \#sampler_assign pseudo-directives
    csString ProcessSamplerAssigns (const csString& originalText,
                                    Snippet& snippet);

    csString GLSLType (const WeaverCommon::TypeInfo* typeInfo);
    csString GLSLType (const char* weaverType);
    csString GetAttrIdentifier (const char* var, const char* attr);
  };
}
CS_PLUGIN_NAMESPACE_END(SLCombiner)

#endif //__COMBINER_GLSL_H__
