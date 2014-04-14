/*
  Copyright (C) 2004-2006 by Frank Richter
	    (C) 2004-2006 by Jorrit Tyberghein

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

#ifndef __CS_DOCWRAP_H__
#define __CS_DOCWRAP_H__

#include "csutil/csstring.h"
#include "csutil/documentcommon.h"
#include "csutil/leakguard.h"
#include "csutil/parray.h"
#include "csutil/pooledscfclass.h"
#include "csutil/ref.h"
#include "csutil/refarr.h"
#include "csutil/scf.h"
#include "csutil/scf_implementation.h"
#include "csutil/set.h"
#include "csutil/strhash.h"
#include "csutil/weakref.h"

#include "iutil/document.h"
#include "iutil/objreg.h"
#include "iutil/vfs.h"

#include "docwrap_replacer.h"
#include "docpreproc.h"
#include "tempheap.h"

#include "csutil/custom_new_disable.h"

CS_PLUGIN_NAMESPACE_BEGIN(DocPreproc)
{

class csXMLShaderCompiler;

class csWrappedDocumentNodeIterator;
struct WrapperStackEntry;

struct csConditionNode;
class csWrappedDocumentNodeFactory;
class ConditionDumper;

struct iWrappedDocumentNode : public virtual iBase
{
  SCF_INTERFACE (iWrappedDocumentNode, 0, 0, 1);
};

/**
 * Wrapper around a document node, supporting conditionals.
 */
class csWrappedDocumentNode : 
  public scfImplementationExt1<csWrappedDocumentNode,
                               csDocumentNodeReadOnly,
                               iWrappedDocumentNode>
{
  friend class csWrappedDocumentNodeIterator;
  friend struct WrapperStackEntry;
  friend class csTextNodeWrapper;
  friend class csWrappedDocumentNodeFactory;

  csRef<iDocumentNode> wrappedNode;
  csWeakRef<csWrappedDocumentNode> parent;
  csString contents;
  csRef<csWrappedDocumentNodeFactory> shared;

protected:
  csRefArray<iDocumentNode> wrappedChildren;

  /**
   * Helper class to go over the wrapped children in a linear fashion,
   * without having to care that a hierarchy of WrappedChild structures 
   * are actually used.
   */
  class WrapperWalker
  {
    size_t currentIndex;
    const csRefArray<iDocumentNode>* currentWrappers;
  public:
    WrapperWalker (const csRefArray<iDocumentNode>& wrappedChildren);
    WrapperWalker ();
    void SetData (const csRefArray<iDocumentNode>& wrappedChildren);

    bool HasNext ();
    iDocumentNode* Peek ();
    iDocumentNode* Next ();

    size_t GetNextPosition () { return currentIndex; }
    size_t GetEndPosition () { return currentWrappers->GetSize(); }
  };
  friend class WrapperWalker;

  struct Template
  {
    typedef csRefArray<iDocumentNode, TempHeapAlloc> Nodes;
    typedef csArray<TempString<>, csArrayElementHandler<TempString<> >, 
      TempHeapAlloc> Params;
    Nodes nodes;
    Params paramMap;
  };
  struct GlobalProcessingState : public csRefCount, protected TempHeap
  {
    static GlobalProcessingState* Create()
    {
      GlobalProcessingState* gps;
      gps = (GlobalProcessingState*)TempHeap::Alloc (sizeof (GlobalProcessingState));
      new (gps) GlobalProcessingState;
      return gps;
    }
    virtual void Delete ()
    {
      this->~GlobalProcessingState();
      TempHeap::Free (this);
    }
    
    ConditionDumper* condDumper;
    csHash<Template, TempString<>, TempHeapAlloc> templates;
    csArray<int, csArrayElementHandler<int>, TempHeapAlloc> ascendStack;
    csSet<TempString<>, TempHeapAlloc> defines;
    
    csRef<iVFS> vfs;
    csHash<csRef<iDocumentNode>, TempString<>, TempHeapAlloc> includesCache;
  };
  csRef<GlobalProcessingState> globalState;

  struct NodeProcessingState;
  void ProcessInclude (const TempString<>& filename,
    NodeProcessingState* state, iDocumentNode* node);
  /**
   * Process a node when a Template or Generate is active.
   * Returns 'true' if the node was handled.
   */
  bool ProcessTemplate (iDocumentNode* templNode,
    NodeProcessingState* state);
  bool InvokeTemplate (Template* templ, const Template::Params& params,
    Template::Nodes& templatedNodes);
  bool InvokeTemplate (const char* name,
    iDocumentNode* node, NodeProcessingState* state, 
    const Template::Params& params);
  /// Validate that a 'Template' was properly matched by an 'Endtemplate'
  void ValidateTemplateEnd (iDocumentNode* node, 
    NodeProcessingState* state);
  /// Validate that a 'Generate' was properly matched by an 'Endgenerate'
  void ValidateGenerateEnd (iDocumentNode* node, 
    NodeProcessingState* state);
  /// Validate that an 'SIfDef' was properly matched by an 'SEndIf'
  void ValidateStaticIfEnd (iDocumentNode* node, 
    NodeProcessingState* state);
  void ParseTemplateArguments (const char* str, 
    Template::Params& strings, bool omitEmpty);
  /**
   * Process a node when a static conditition is active.
   * Returns 'true' if the node was handled.
   */
  bool ProcessStaticIf (NodeProcessingState* state, iDocumentNode* node);

  /// Process a "Template" or "TemplateWeak" instruction
  bool ProcessInstrTemplate (NodeProcessingState* state, iDocumentNode* node, 
    const TempString<>& args, bool weak);
  /// Process a "Define" instruction
  bool ProcessDefine (NodeProcessingState* state, iDocumentNode* node, 
    const TempString<>& args);
  /// Process an "Undef" instruction
  bool ProcessUndef (NodeProcessingState* state, iDocumentNode* node, 
    const TempString<>& args);
  /// Process a static "IfDef"/"IfNDef" instruction
  bool ProcessStaticIfDef (NodeProcessingState* state, iDocumentNode* node, 
    const TempString<>& args, bool invert);

  void ProcessSingleWrappedNode (NodeProcessingState* state, iDocumentNode* wrappedNode);
  void ProcessWrappedNode (NodeProcessingState* state,
    iDocumentNode* wrappedNode);
  void Report (int severity, iDocumentNode* node, const char* msg, ...);
  
  static void AppendNodeText (WrapperWalker& walker, csString& text);

  csWrappedDocumentNode (csWrappedDocumentNode* parent,
    iDocumentNode* wrappedNode,
    csWrappedDocumentNodeFactory* shared, 
    GlobalProcessingState* globalState);
  csWrappedDocumentNode (csWrappedDocumentNode* parent,
    iDocumentNode* wrappedNode,
    csWrappedDocumentNodeFactory* shared);
  csWrappedDocumentNode (csWrappedDocumentNode* parent,
    csWrappedDocumentNodeFactory* shared);
public:
  CS_LEAKGUARD_DECLARE(csWrappedDocumentNode);

  virtual ~csWrappedDocumentNode ();

  virtual csDocumentNodeType GetType ()
  { return wrappedNode->GetType(); }
  virtual bool Equals (iDocumentNode* other);
  virtual const char* GetValue ();

  virtual csRef<iDocumentNode> GetParent ()
  { return static_cast<iDocumentNode*> (parent); }
  virtual csRef<iDocumentNodeIterator> GetNodes ();
  virtual csRef<iDocumentNodeIterator> GetNodes (const char* value);
  virtual csRef<iDocumentNode> GetNode (const char* value);

  virtual const char* GetContentsValue ();

  virtual csRef<iDocumentAttributeIterator> GetAttributes ();
  virtual csRef<iDocumentAttribute> GetAttribute (const char* name);
  virtual const char* GetAttributeValue (const char* name);
  virtual int GetAttributeValueAsInt (const char* name);
  virtual float GetAttributeValueAsFloat (const char* name);
  virtual bool GetAttributeValueAsBool (const char* name, 
    bool defaultvalue = false);

  inline iDocumentNode* GetWrappedNode() const { return wrappedNode; }
};

class csTextNodeWrapper : 
  public scfImplementationPooled<
  scfImplementationExt0<csTextNodeWrapper, csDocumentNodeReadOnly>,
  CS::Memory::AllocatorMalloc,
  true>
{
  char* nodeText;
  csRef<iDocumentNode> realMe;
public:
  csTextNodeWrapper (iDocumentNode* realMe, const char* text);
  virtual ~csTextNodeWrapper ();

  virtual csDocumentNodeType GetType ()
  { return CS_NODE_TEXT; }
  virtual bool Equals (iDocumentNode* other)
  { return realMe->Equals (other); }

  virtual const char* GetValue ()
  { return nodeText; }

  virtual csRef<iDocumentNode> GetParent ()
  { return realMe->GetParent (); }

  virtual const char* GetContentsValue ()
  { return 0; }
};

class csWrappedDocumentNodeIterator : 
  public scfImplementationPooled<
  scfImplementation1<csWrappedDocumentNodeIterator, iDocumentNodeIterator>,
  CS::Memory::AllocatorMalloc,
  true>
{
  csString filter;

  csWrappedDocumentNode* parentNode;
  csWrappedDocumentNode::WrapperWalker walker;
  csRef<iDocumentNode> next;
  void SeekNext();
public:
  CS_LEAKGUARD_DECLARE(csWrappedDocumentNodeIterator);

  csWrappedDocumentNodeIterator (csWrappedDocumentNode* node, const char* filter);
  virtual ~csWrappedDocumentNodeIterator ();

  virtual bool HasNext ();
  virtual csRef<iDocumentNode> Next ();
  size_t GetNextPosition () { return walker.GetNextPosition (); }
  size_t GetEndPosition () { return walker.GetEndPosition (); }
};

class csWrappedDocumentNodeFactory : public scfImplementation0<csWrappedDocumentNodeFactory>
{
  friend class csWrappedDocumentNode;
  friend class csWrappedDocumentNodeIterator;

  csRef<DocumentPreprocessor> plugin;
  csTextNodeWrapper::Pool textWrapperPool;
  csWrappedDocumentNodeIterator::Pool iterPool;
  csReplacerDocumentNodeFactory replacerFactory;
  iObjectRegistry* objreg;

  csStringHash pitokens;
#define CS_TOKEN_ITEM_FILE \
  "plugins/documentsystem/preprocessor/docwrap.tok"
#define CS_TOKEN_LIST_TOKEN_PREFIX PITOKEN_
#include "cstool/tokenlist.h"
#undef CS_TOKEN_ITEM_FILE
#undef CS_TOKEN_LIST_TOKEN_PREFIX
  enum
  {
    PITOKEN_TEMPLATE_NEW = 0xfeeb1e,
    PITOKEN_TEMPLATEWEAK,
    PITOKEN_ENDTEMPLATE_NEW,
    PITOKEN_INCLUDE_NEW,
    PITOKEN_GENERATE,
    PITOKEN_ENDGENERATE,
    PITOKEN_DEFINE,
    PITOKEN_UNDEF,
    PITOKEN_STATIC_IFDEF,
    PITOKEN_STATIC_IFNDEF,
    PITOKEN_STATIC_ELSIFDEF,
    PITOKEN_STATIC_ELSIFNDEF,
    PITOKEN_STATIC_ELSE,
    PITOKEN_STATIC_ENDIF
  };

  void DebugProcessing (const char* msg, ...) CS_GNUC_PRINTF (2, 3);
public:
  csWrappedDocumentNodeFactory (DocumentPreprocessor* plugin);

  csWrappedDocumentNode* CreateWrapperStatic (iDocumentNode* wrappedNode);
};

}
CS_PLUGIN_NAMESPACE_END(DocPreproc)

#include "csutil/custom_new_enable.h"

#endif // __CS_DOCWRAP_H__
