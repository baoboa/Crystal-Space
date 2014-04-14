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

#include "cssysdef.h"

#include "combiner_common.h"

#include "imap/services.h"
#include "iutil/objreg.h"
#include "iutil/vfs.h"
#include "ivaria/reporter.h"

#include "csplugincommon/shader/shadercachehelper.h"
#include "csplugincommon/shader/weavertypes.h"
#include "csutil/base64.h"
#include "csutil/checksum.h"
#include "csutil/cfgacc.h"
#include "csutil/csendian.h"
#include "csutil/stringquote.h"
#include "csutil/xmltiny.h"

CS_PLUGIN_NAMESPACE_BEGIN(SLCombiner)
{
  using namespace CS::PluginCommon;

  ShaderCombinerLoaderCommon::ShaderCombinerLoaderCommon (iBase* parent)
    : scfImplementationType (this, parent), annotateCombined (false), object_reg (nullptr)
  {
    InitTokenTable (xmltokens_common);
  }

  bool ShaderCombinerLoaderCommon::Initialize (iObjectRegistry* reg)
  {
    object_reg = reg;

    csConfigAccess config (object_reg);
    annotateCombined = config->GetBool ("Video.ShaderWeaver.AnnotateOutput");

    return true;
  }

  void ShaderCombinerLoaderCommon::Report (int severity, const char* msg, ...)
  {
    va_list args;
    va_start (args, msg);
    csReportV (object_reg, severity, GetMessageID(), msg, args);
    va_end (args);
  }

  void ShaderCombinerLoaderCommon::Report (int severity, iDocumentNode* node,
                                           const char* msg, ...)
  {
    va_list args;
    va_start (args, msg);

    csRef<iSyntaxService> synsrv = csQueryRegistry<iSyntaxService> (
      object_reg);
    if (synsrv.IsValid())
    {
      synsrv->ReportV (GetMessageID(), severity, node, msg, args);
    }
    else
    {
      csReportV (object_reg, severity, GetMessageID(), msg, args);
    }
    va_end (args);
  }

  void ShaderCombinerLoaderCommon::GenerateConstantInputBlocks (
    iDocumentNode* node, const char* locationPrefix, const char* code)
  {
    csRef<iDocumentNode> blockNode;
    csRef<iDocumentNode> contents;

    blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
    blockNode->SetValue ("block");
    blockNode->SetAttribute ("location",
      csString().Format ("%s:fragmentMain", locationPrefix));
    contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
    contents->SetValue (code);

    blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
    blockNode->SetValue ("block");
    blockNode->SetAttribute ("location",
      csString().Format ("%s:vertexMain", locationPrefix));
    contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
    contents->SetValue (code);
  }

  static inline bool IsAlpha (char c)
  { return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') || (c <= 'z')); }

  static inline bool IsAlNum (char c)
  { return ((c >= '0') && (c <= '9')) || IsAlpha (c); }

  csString ShaderCombinerLoaderCommon::MakeIdentifier (const char* s)
  {
    csString res;
    if (!IsAlpha (*s)) res << '_';
    while (*s != 0)
    {
      char ch = *s++;
      if (IsAlNum (ch))
        res << ch;
      else
        res.AppendFmt ("_%02x", ch);
    }
    return res;
  }

  bool ShaderCombinerLoaderCommon::LoadCoercionLibrary (const char* path)
  {
    csRef<iVFS> vfs (csQueryRegistry<iVFS> (object_reg));
    if (!vfs.IsValid()) return false;

    csRef<iFile> libfile = vfs->Open (path, VFS_FILE_READ);
    if (!libfile.IsValid())
    {
      Report (CS_REPORTER_SEVERITY_ERROR, "Can't open %s", path);
      return false;
    }

    csRef<iDocumentSystem> docsys;
    docsys.AttachNew (new csTinyDocumentSystem);
    csRef<iDocument> doc = docsys->CreateDocument();
    {
      const char* err = doc->Parse (libfile);
      if (err)
      {
        Report (CS_REPORTER_SEVERITY_ERROR, "Error parsing %s: %s",
          path, err);
        return false;
      }
    }
    csRef<iDocumentNode> startNode =
      doc->GetRoot ()->GetNode ("combinerlibrary");
    if (!startNode.IsValid())
    {
      Report (CS_REPORTER_SEVERITY_ERROR,
        "Expected %s node in file %s",
        CS::Quote::Single ("combinerlibrary"),
        CS::Quote::Single (path));
      return false;
    }

    CoercionTemplates templates;
    csRef<iDocumentNodeIterator> nodes = startNode->GetNodes();
    while (nodes->HasNext())
    {
      csRef<iDocumentNode> child = nodes->Next();
      if (child->GetType() != CS_NODE_ELEMENT) continue;

      csStringID id = xmltokens_common.Request (child->GetValue());
      switch (id)
      {
        case XMLTOKEN_COERCION:
          if (!ParseCoercion (child))
            return false;
          break;
        case XMLTOKEN_COERCIONTEMPLATE:
          if (!ParseCoercionTemplates (child, templates))
            return false;
          break;
        default:
          {
            csRef<iSyntaxService> synsrv = csQueryRegistry<iSyntaxService> (
              object_reg);
            if (synsrv.IsValid())
              synsrv->ReportBadToken (child);
            return false;
          }
      }
    }

    /* Compute a checksum over the coercion library, to be able to detect
       changes (and allow weaver to invalidate cached shaders) */
    uint32 libCheckSum;
    {
      csRef<iDataBuffer> libData = libfile->GetAllData();
      libCheckSum = CS::Utility::Checksum::Adler32 (libData);
      CS::PluginCommon::ShaderCacheHelper::ShaderDocHasher hasher (object_reg,
        startNode);
      csRef<iDataBuffer> hashStream = hasher.GetHashStream();
      libCheckSum = CS::Utility::Checksum::Adler32 (libCheckSum, hashStream);
    }
    uint32 checkSumLE = csLittleEndian::UInt32 (libCheckSum);
    codeString = CS::Utility::EncodeBase64 (&checkSumLE, sizeof (checkSumLE));

    return SynthesizeDefaultCoercions (templates);
  }

  bool ShaderCombinerLoaderCommon::ParseCoercion (iDocumentNode* node)
  {
    const char* from = node->GetAttributeValue ("from");
    if (!from || !*from)
    {
      Report (CS_REPORTER_SEVERITY_ERROR, node,
        "Non-empty %s attribute expected",
        CS::Quote::Single ("from"));
      return false;
    }
    const char* to = node->GetAttributeValue ("to");
    if (!to || !*to)
    {
      Report (CS_REPORTER_SEVERITY_ERROR, node,
        "Non-empty %s attribute expected",
        CS::Quote::Single ("to"));
      return false;
    }
    int cost;
    csRef<iDocumentAttribute> costAttr = node->GetAttribute ("cost");
    if (!costAttr.IsValid())
    {
      Report (CS_REPORTER_SEVERITY_WARNING, node,
        "No %s attribute, assuming cost 0",
        CS::Quote::Single ("cost"));
      cost = 0;
    }
    else
    {
      cost = costAttr->GetValueAsInt ();
    }

    csRef<iDocumentNode> inputNode =
      node->CreateNodeBefore (CS_NODE_ELEMENT);
    inputNode->SetValue ("input");
    inputNode->SetAttribute ("name", CoercionInputName());
    inputNode->SetAttribute ("type", from);

    csRef<iDocumentNode> outputNode =
      node->CreateNodeBefore (CS_NODE_ELEMENT);
    outputNode->SetValue ("output");
    outputNode->SetAttribute ("name", CoercionResultName ());
    outputNode->SetAttribute ("type", to);
    outputNode->SetAttribute ("inheritattr", CoercionInputName());

    CoerceItem item;
    item.fromType = StoredTypeName (from);
    item.toType = StoredTypeName (to);
    item.cost = cost;
    item.node = node;
    CoerceItems* items = coercions.GetElementPointer (from);
    if (items == 0)
    {
      coercions.Put (StoredTypeName (from), CoerceItems());
      items = coercions.GetElementPointer (from);
    }
    items->InsertSorted (item, &CoerceItemCompare);

    return true;
  }

  bool ShaderCombinerLoaderCommon::ParseCoercionTemplates (iDocumentNode* node,
    CoercionTemplates& templates)
  {
    const char* name = node->GetAttributeValue ("name");
    if (!name || !*name)
    {
      Report (CS_REPORTER_SEVERITY_ERROR, node,
        "Non-empty %s attribute expected",
        CS::Quote::Single ("name"));
      return false;
    }
    templates.PutUnique (name, node);
    return true;
  }

  bool ShaderCombinerLoaderCommon::SynthesizeDefaultCoercions (
    const CoercionTemplates& templates)
  {
    iDocumentNode* templNormalize = templates.Get ("normalize",
      (iDocumentNode*)0);
    if (!templNormalize)
    {
      Report (CS_REPORTER_SEVERITY_ERROR,
        "No %s coercion template",
        CS::Quote::Single ("normalize"));
      return false;
    }
    iDocumentNode* templPassthrough = templates.Get ("passthrough",
      (iDocumentNode*)0);
    if (!templPassthrough)
    {
      Report (CS_REPORTER_SEVERITY_ERROR,
        "No %s coercion template",
        CS::Quote::Single ("passthrough"));
      return false;
    }

    csRef<iDocumentSystem> docsys;
    docsys.AttachNew (new csTinyDocumentSystem);

    ShaderWeaver::TypeInfoIterator typeIt;
    while (typeIt.HasNext())
    {
      csString type;
      const ShaderWeaver::TypeInfo& typeInfo = *typeIt.Next (type);
      iDocumentNode* templ = 0;
      ShaderWeaver::TypeInfo newTypeInfo (typeInfo);
      if (typeInfo.semantics == ShaderWeaver::TypeInfo::Direction)
      {
        if (typeInfo.unit)
        {
          templ = templPassthrough;
          newTypeInfo.unit = false;
        }
        else
        {
          templ = templNormalize;
          newTypeInfo.unit = true;
        }
      }
      else if (typeInfo.space != ShaderWeaver::TypeInfo::NoSpace)
      {
        templ = templPassthrough;
        newTypeInfo.space = ShaderWeaver::TypeInfo::NoSpace;
      }
      const char* newTypeName = ShaderWeaver::QueryType (newTypeInfo);
      if( (templ != 0) && (newTypeName != 0))
      {
        csRef<iDocument> doc = docsys->CreateDocument();
        csRef<iDocumentNode> root = doc->CreateRoot ();
        csRef<iDocumentNode> main = root->CreateNodeBefore (CS_NODE_ELEMENT);
        CS::DocSystem::CloneNode (templ, main);
        main->SetAttribute ("from", type);
        main->SetAttribute ("to", newTypeName);
        ParseCoercion (main);
      }
    }
    return true;
  }

  int ShaderCombinerLoaderCommon::CoerceItemCompare (CoerceItem const& i1,
                                                     CoerceItem const& i2)
  {
    if (i1.cost < i2.cost)
      return -1;
    else if (i1.cost < i2.cost)
      return 1;

    // If two types have same cost, put the more specialized one first.
    const ShaderWeaver::TypeInfo* t1 = ShaderWeaver::QueryTypeInfo (i1.toType);
    const ShaderWeaver::TypeInfo* t2 = ShaderWeaver::QueryTypeInfo (i2.toType);
    if ((t1 == 0) || (t2 == 0)) return 0;

    if ((t1->semantics != ShaderWeaver::TypeInfo::NoSemantics)
      && (t2->semantics == ShaderWeaver::TypeInfo::NoSemantics))
      return -1;
    if ((t1->semantics == ShaderWeaver::TypeInfo::NoSemantics)
      && (t2->semantics != ShaderWeaver::TypeInfo::NoSemantics))
      return 1;

    if ((t1->space != ShaderWeaver::TypeInfo::NoSpace)
      && (t2->space == ShaderWeaver::TypeInfo::NoSpace))
      return -1;
    if ((t1->space == ShaderWeaver::TypeInfo::NoSpace)
      && (t2->space != ShaderWeaver::TypeInfo::NoSpace))
      return 1;

    /*
    if (t1->unit && !t2->unit)
      return -1;
    if (!t1->unit && t2->unit)
      return 1;
    */

    return 0;
  }

  /* Bit of a kludge... helpers uses by FindCoerceChain, however, can't be made
   * local b/c used as template parameters...
   */
  namespace
  {
    template<typename T>
    struct Hierarchy
    {
      const T* item;
      size_t parent;

      Hierarchy (const T* item, size_t parent) : item (item),
        parent (parent) {}
    };
    template<typename T>
    struct TestSource
    {
      csString type;
      const T* item;
      size_t depth;
      size_t hierarchy;

      TestSource (const T& item, size_t depth, size_t hierarchy) :
        type (item.toType), item (&item), depth (depth), hierarchy (hierarchy) {}
    };
  }

  void ShaderCombinerLoaderCommon::FindCoerceChain (const char* from,
                                                    const char* to,
                                                    csArray<const CoerceItem*>& chain)
  {
    csArray<Hierarchy<CoerceItem> > hierarchy;
    csFIFO<TestSource<CoerceItem> > sourcesToTest;

    const CoerceItems* items = coercions.GetElementPointer (from);
    if (items != 0)
    {
      // Search for direct match
      for (size_t i = 0; i < items->GetSize(); i++)
      {
    if (strcmp (items->Get (i).toType, to) == 0)
    {
      chain.Push (&items->Get (i));
      return;
    }
    else
    {
      // Otherwise, search if no match is found.
          const CoerceItem& item = items->Get (i);
      sourcesToTest.Push (TestSource<CoerceItem> (item,
        0,
        hierarchy.Push (Hierarchy<CoerceItem> (&item,
          csArrayItemNotFound))));
    }
      }
    }

    // To avoid unnecessary complicated each type can only appear once.
    csSet<csString> seenTypes;
    seenTypes.Add (from);
    // Keep track of used coercions to prevent loops.
    csSet<csConstPtrKey<CoerceItem> > checkedItems;
    while (sourcesToTest.GetSize() > 0)
    {
      TestSource<CoerceItem> testFrom = sourcesToTest.PopTop ();
      if (checkedItems.Contains (testFrom.item)) continue;
      const CoerceItems* items = coercions.GetElementPointer (testFrom.type);
      if (items != 0)
      {
        // Search for direct match
    for (size_t i = 0; i < items->GetSize(); i++)
    {
          const CoerceItem& item = items->Get (i);
      if (strcmp (item.toType, to) == 0)
      {
        // Generate chain
        size_t d = testFrom.depth+1;
        chain.SetSize (d+1);
            chain[d] = &item;
            chain[--d] = testFrom.item;
        size_t h = testFrom.hierarchy;
        while (d-- > 0)
        {
          h = hierarchy[h].parent;
              const CoerceItem* hItem = hierarchy[h].item;
          chain[d] = hItem;
        }
            return;
      }
      else
      {
        // Otherwise, search if no match is found.
            if (!seenTypes.Contains (item.toType))
            {
          sourcesToTest.Push (TestSource<CoerceItem> (item,
            testFrom.depth+1,
            hierarchy.Push (Hierarchy<CoerceItem> (&item,
              testFrom.hierarchy))));
            }
      }
    }
      }
      checkedItems.AddNoTest (testFrom.item);
      seenTypes.Add (testFrom.type);
    }
  }

  csPtr<WeaverCommon::iCoerceChainIterator>
  ShaderCombinerLoaderCommon::QueryCoerceChain (const char* fromType,
                                                const char* toType)
  {
    csRef<CoerceChainIterator> iterator;
    iterator.AttachNew (new CoerceChainIterator);

    FindCoerceChain (fromType, toType, iterator->nodes);

    return csPtr<WeaverCommon::iCoerceChainIterator> (iterator);
  }

  uint ShaderCombinerLoaderCommon::CoerceCost (const char* fromType,
                                               const char* toType)
  {
    csArray<const CoerceItem*> chain;

    FindCoerceChain (fromType, toType, chain);
    if (chain.GetSize() == 0)
      return ShaderWeaver::NoCoercion;

    uint cost = 0;
    for (size_t i = 0; i < chain.GetSize(); i++)
      cost += chain[i]->cost;
    return cost;
  }

  //-------------------------------------------------------------------------

  void ShaderCombinerCommon::SplitOffArrayCount (csString& name, int& count)
  {
    size_t bracketPos = name.FindFirst ('[');
    if (bracketPos == (size_t)-1)
    {
      count = -1;
    }
    else
    {
      // @@@ Not very strict
      sscanf (name.GetData() + bracketPos + 1, "%d", &count);
      name.Truncate (bracketPos);
    }
  }

  //-------------------------------------------------------------------------

  void ShaderCombinerCommon::DocNodeAppender::FlushAppendString ()
  {
    if (!stringAppend.IsEmpty ())
    {
      csRef<iDocumentNode> newNode = node->CreateNodeBefore (CS_NODE_TEXT);
      newNode->SetValue (stringAppend);
      stringAppend.Empty();
    }
  }

  ShaderCombinerCommon::DocNodeAppender::DocNodeAppender (iDocumentNode* node) :
    node (node), beautifier (stringAppend)
  {
    stringAppend.SetGrowsBy (0);
  }

  ShaderCombinerCommon::DocNodeAppender::~DocNodeAppender ()
  {
    FlushAppendString();
  }

  void ShaderCombinerCommon::DocNodeAppender::Append (const char* str)
  {
    if (str == 0) return;
    beautifier.Append (str);
  }

  void ShaderCombinerCommon::DocNodeAppender::Append (iDocumentNode* appendNode)
  {
    csDocumentNodeType nodeType = appendNode->GetType();
    if (nodeType == CS_NODE_TEXT)
    {
      Append (appendNode->GetValue());
    }
    else if (nodeType == CS_NODE_COMMENT)
    {
      // Skip
    }
    else
    {
      FlushAppendString();
      csRef<iDocumentNode> newNode =
        node->CreateNodeBefore (nodeType);
      CS::DocSystem::CloneNode (appendNode, newNode);
    }
  }

  void ShaderCombinerCommon::DocNodeAppender::Append (
    const csRefArray<iDocumentNode>& nodes)
  {
    for (size_t n = 0; n < nodes.GetSize(); n++)
    {
      Append (nodes[n]);
    }
  }

}
CS_PLUGIN_NAMESPACE_END(SLCombiner)
