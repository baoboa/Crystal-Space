/*
  Copyright (C) 2003-2007 by Marten Svanfeldt
            (C) 2004-2007 by Frank Richter

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

#ifndef __COMBINER_CG_H__
#define __COMBINER_CG_H__

#include "csplugincommon/shader/weavercombiner.h"
#include "csutil/csstring.h"
#include "csutil/leakguard.h"
#include "csutil/refarr.h"
#include "csutil/scf_implementation.h"
#include "csutil/strhash.h"

#include "iutil/comp.h"

#include "combiner_common.h"

CS_PLUGIN_NAMESPACE_BEGIN(SLCombiner)
{
  namespace WeaverCommon = CS::PluginCommon::ShaderWeaver;

  struct FindCoerceChainHelper;

  class ShaderCombinerLoaderCg : 
    public scfImplementationExt0<ShaderCombinerLoaderCg,
                                 ShaderCombinerLoaderCommon>
  {
  public:
    CS_LEAKGUARD_DECLARE (ShaderCombinerLoaderCg);
  
    ShaderCombinerLoaderCg (iBase *parent);
    
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
    const char* CoercionInputName () { return "input"; }
    const char* CoercionResultName () { return "output"; }
  };
  
  class ShaderCombinerCg : 
    public scfImplementationExt0<ShaderCombinerCg, ShaderCombinerCommon>
  {
    csRef<ShaderCombinerLoaderCg> loader;
    bool writeVP, writeFP;
    
    struct Attribute
    {
      csString name;
      csString type;
    };
    typedef csArray<Attribute> AttributeArray;
    Attribute* FindAttr (AttributeArray& arr, const char* name, 
      const char* type);
    struct Snippet : public CommonSnippet
    {
      csRefArray<iDocumentNode> vert2frag;
      csRefArray<iDocumentNode> vertexIn;
      csRefArray<iDocumentNode> fragmentIn; 
      
      csSet<csString> localIDs;
      csHash<csString, csString> v2fMaps;
      csString locals;
      csHash<csString, csString> inputMaps;
      csRefArray<iDocumentNode> vertexBody;
      csRefArray<iDocumentNode> fragmentBody;
      csHash<csString, csString> outputMaps;
      csString links;

      csHash<csString, csString> attrInputMaps;
      csHash<csString, csString> attrOutputMaps;
    };
    size_t uniqueCounter;
    csArray<Snippet> snippets;
    Snippet currentSnippet;
    csRefArray<iDocumentNode> vertexCompilerArgs;
    csRefArray<iDocumentNode> fragmentCompilerArgs; 
    csRefArray<iDocumentNode> variableMaps;
    csString outputAssign[rtaNumAttachments];
    csRefArray<iDocumentNode> definitions;
    csSet<csString> globalIDs;
    csString globals;

    csHash<AttributeArray, csString> attributes;
  public:
    ShaderCombinerCg (ShaderCombinerLoaderCg* loader, bool vp, bool fp);

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
  private:
    class V2FAutoSematicsHelper;
  
    void AppendProgramInput (const csRefArray<iDocumentNode>& nodes, 
      DocNodeAppender& appender);
    void AppendProgramInput_V2FHead (const Snippet& snippet, 
      DocNodeAppender& appender);
    void AppendProgramInput_V2FDecl (const Snippet& snippet, 
      const V2FAutoSematicsHelper& semanticsHelper,
      DocNodeAppender& appender);
    void AppendProgramInput_V2FLocals (const Snippet& snippet, 
      DocNodeAppender& appender);
    void AppendProgramInput_V2FVP (const Snippet& snippet, 
      DocNodeAppender& appender);
    void AppendProgramInput_V2FFP (const Snippet& snippet, 
      DocNodeAppender& appender);
    void AppendProgramInput (iDocumentNode* node, DocNodeAppender& appender);
    csString CgType (const WeaverCommon::TypeInfo* typeInfo);
    csString CgType (const char* weaverType);
    csString GetAttrIdentifier (const char* var, const char* attr);

    csString annotateStr;
    const char* MakeComment (const char* s);
    void AppendSnippetMap (const csHash<csString, csString>& map, 
      DocNodeAppender& appender);
  protected:
    void AppendComment (csString& s, const char* contents);
  };
}
CS_PLUGIN_NAMESPACE_END(SLCombiner)

#endif //__COMBINER_CG_H__
