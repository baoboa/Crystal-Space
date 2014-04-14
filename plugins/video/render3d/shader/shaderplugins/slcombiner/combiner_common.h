/*
  Copyright (C) 2003-2011 by Marten Svanfeldt
            (C) 2004-2011 by Frank Richter

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

#ifndef __COMBINER_COMMON_H__
#define __COMBINER_COMMON_H__

#include "csplugincommon/shader/weavercombiner.h"
#include "csutil/documenthelper.h"
#include "csutil/refarr.h"
#include "csutil/scf_implementation.h"

#include "iutil/comp.h"

#include "beautify.h"

CS_PLUGIN_NAMESPACE_BEGIN(SLCombiner)
{
  namespace WeaverCommon = CS::PluginCommon::ShaderWeaver;

  class ShaderCombinerLoaderCommon :
    public scfImplementation2<ShaderCombinerLoaderCommon,
                              WeaverCommon::iCombinerLoader,
                              iComponent>
  {
  public:
    /**
     * Whether to add extra (debugging) annotations to the generated shader
     * code.
     */
    bool annotateCombined;

    ShaderCombinerLoaderCommon (iBase* parent);

    /**\name CS::PluginCommon::ShaderWeaver::iCombinerLoader implementation
     * @{ */
    const char* GetCodeString() { return codeString; }
    /** @} */

    /**\name iComponent implementation
    * @{ */
    bool Initialize (iObjectRegistry* reg);
    /** @} */

    /// Report a message
    void Report (int severity, const char* msg, ...) CS_GNUC_PRINTF(3, 4);
    /// Report a parsing issue
    void Report (int severity, iDocumentNode* node, const char* msg, ...)
      CS_GNUC_PRINTF(4, 5);

    csPtr<WeaverCommon::iCoerceChainIterator> QueryCoerceChain (
      const char* fromType, const char* toType);
    uint CoerceCost (const char* fromType, const char* toType);
  public:
  #define CS_TOKEN_ITEM_FILE \
    "plugins/video/render3d/shader/shaderplugins/slcombiner/combiner_common.tok"
  #include "cstool/tokenlist.h"
  #undef CS_TOKEN_ITEM_FILE
    csStringHash xmltokens_common;
  protected:
    iObjectRegistry* object_reg;

    void GenerateConstantInputBlocks (iDocumentNode* node,
      const char* locationPrefix, /*const csVector4& value,
      int usedComponents, */const char* outputName);

    virtual const char* GetMessageID() const = 0;

    /// Format a string so it's suitable as an identifier
    static csString MakeIdentifier (const char* s);

    /**\name Coercion data
     * @{ */
    /// Coercion data code string
    csString codeString;

    struct CoerceItem
    {
      uint cost;
      const char* fromType;
      const char* toType;
      csRef<iDocumentNode> node;
    };
    typedef csArray<CoerceItem> CoerceItems;
    csHash<CoerceItems, const char*> coercions;

    bool LoadCoercionLibrary (const char* path);
    bool ParseCoercion (iDocumentNode* node);
    virtual const char* CoercionInputName () = 0;
    virtual const char* CoercionResultName () = 0;
    typedef csHash<csRef<iDocumentNode>, csString> CoercionTemplates;
    bool ParseCoercionTemplates (iDocumentNode* node,
      CoercionTemplates& templates);
    bool SynthesizeDefaultCoercions (const CoercionTemplates& templates);

    static int CoerceItemCompare (CoerceItem const& i1, CoerceItem const& i2);

    void FindCoerceChain (const char* from, const char* to,
      csArray<const CoerceItem*>& chain);

    class CoerceChainIterator :
      public scfImplementation1<CoerceChainIterator,
                                WeaverCommon::iCoerceChainIterator>
    {
      size_t pos;
    public:
      csArray<const CoerceItem*> nodes;

      CoerceChainIterator() : scfImplementationType (this), pos (0) {}

      bool HasNext() { return pos < nodes.GetSize(); }
      csRef<iDocumentNode> Next () { return nodes[pos++]->node; }
      csRef<iDocumentNode> Next (const char*& fromType, const char*& toType)
      {
        fromType = nodes[pos]->fromType;
        toType = nodes[pos]->toType;
        return nodes[pos++]->node;
      }
      size_t GetNextPosition () { return pos; }
      size_t GetEndPosition () { return nodes.GetSize(); }
    };

    csStringHash typesSet;
    const char* StoredTypeName (const char* type)
    { return typesSet.Register (type); }
    /** @} */
  };

  class ShaderCombinerCommon :
    public scfImplementation1<ShaderCombinerCommon,
                              CS::PluginCommon::ShaderWeaver::iCombiner>
  {
  protected:
    csString description;

    virtual void AppendComment (csString& s, const char* contents) = 0;
      
    void SplitOffArrayCount (csString& name, int& count);
  public:
    ShaderCombinerCommon () : scfImplementationType (this) {}

    /**\name CS::PluginCommon::ShaderWeaver::iCombiner implementation
     * @{ */
    virtual void SetDescription (const char* descr) { description = descr; }
    /** @} */

    struct CommonSnippet
    {
        typedef csRefArray<iDocumentNode> NodesArray;

        csString annotation;
    };

    class DocNodeAppender
    {
      csRef<iDocumentNode> node;
      csString stringAppend;
      Beautifier beautifier;

      void FlushAppendString ();
    public:
      DocNodeAppender (iDocumentNode* node);
      ~DocNodeAppender ();

      void Append (const char* str);
      void Append (iDocumentNode* appendNode);
      void Append (const csRefArray<iDocumentNode>& nodes);
      void AppendFmt (const char* str, ...)
      {
        va_list args;
        va_start (args, str);
        csString s;
        s.FormatV (str, args);
        va_end (args);
        Append (s);
      }
    };
  };

}
CS_PLUGIN_NAMESPACE_END(SLCombiner)

#endif // __COMBINER_COMMON_H__
