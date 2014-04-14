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

#include "cssysdef.h"
#include <ctype.h>

#include "csgeom/math.h"
#include "csutil/bitarray.h"
#include "csutil/csendian.h"
#include "csutil/documenthelper.h"
#include "csutil/fixedsizeallocator.h"
#include "csutil/set.h"
#include "csutil/stringquote.h"
#include "csutil/sysfunc.h"
#include "csutil/util.h"
#include "csutil/xmltiny.h"
#include "cstool/vfsdirchange.h"
#include "imap/services.h"
#include "ivaria/reporter.h"
#include "iutil/vfs.h"
#include "iutil/document.h"

#include "docwrap.h"

CS_PLUGIN_NAMESPACE_BEGIN(DocPreproc)
{

struct csWrappedDocumentNode::NodeProcessingState
{
  csRefArray<iDocumentNode> currentWrapper;
  csRef<iDocumentNodeIterator> iter;

  bool templActive;
  Template templ;
  uint templNestLevel;
  TempString<> templateName;
  bool templWeak;

  bool generateActive;
  bool generateValid;
  uint generateNestLevel;
  TempString<> generateVar;
  int generateStart;
  int generateEnd;
  int generateStep;

  struct StaticIfState
  {
    bool processNodes;
    bool elseBranch;

    StaticIfState(bool processNodes) : processNodes (processNodes),
      elseBranch (false) {}
  };
  csArray<StaticIfState, csArrayElementHandler<StaticIfState>,
    TempHeapAlloc> staticIfStateStack;
  csArray<uint, csArrayElementHandler<uint>,
    TempHeapAlloc> staticIfNest;

  NodeProcessingState() : templActive (false), templWeak (false),
    generateActive (false), generateValid (false) {}
};

//---------------------------------------------------------------------------

CS_LEAKGUARD_IMPLEMENT(csWrappedDocumentNode);

csWrappedDocumentNode::csWrappedDocumentNode (csWrappedDocumentNode* parent,
                                              iDocumentNode* wrapped_node,
					      csWrappedDocumentNodeFactory* shared_fact, 
					      GlobalProcessingState* global_state)
  : scfImplementationType (this), wrappedNode (wrapped_node), parent (parent),
    shared (shared_fact), globalState (global_state)
{
  {
    NodeProcessingState state;
    //state.currentWrapper.AttachNew (new WrappedChild);
    ProcessWrappedNode (&state, wrappedNode);
    //wrappedChildren.Push (state.currentWrapper);
    wrappedChildren = state.currentWrapper;
  }
  globalState = 0;
}
  
csWrappedDocumentNode::csWrappedDocumentNode (csWrappedDocumentNode* parent,
                                              iDocumentNode* wrappedNode,
                                              csWrappedDocumentNodeFactory* shared)
  : scfImplementationType (this), wrappedNode (wrappedNode), parent (parent),
    shared (shared)
{
}
  
csWrappedDocumentNode::csWrappedDocumentNode (csWrappedDocumentNode* parent,
                                              csWrappedDocumentNodeFactory* shared)
  : scfImplementationType (this), parent (parent),
    shared (shared)
{
}

csWrappedDocumentNode::~csWrappedDocumentNode ()
{
}

struct ReplacedEntity
{
  const char* entity;
  char replacement;
};

static const ReplacedEntity entities[] = {
  {"&lt;", '<'},
  {"&gt;", '>'},
  {0, 0}
};

static const char* ReplaceEntities (const char* str, TempString<>& scratch)
{
  const ReplacedEntity* entity = entities;
  while (entity->entity != 0)
  {
    const char* entPos;
    if ((entPos = strstr (str, entity->entity)) != 0)
    {
      size_t offset = entPos - str;
      if (scratch.GetData() == 0)
      {
	scratch.Replace (str);
	str = scratch.GetData ();
      }
      scratch.DeleteAt (offset, strlen (entity->entity));
      scratch.Insert (offset, entity->replacement);
    }
    else
      entity++;
  }
  return str;
}

static bool SplitNodeValue (const char* nodeStr, TempString<>& command, 
                            TempString<>& args)
{
  TempString<> replaceScratch;
  const char* nodeValue = ReplaceEntities (nodeStr, replaceScratch);
  if ((nodeValue != 0) && (*nodeValue == '?') && 
    (*(nodeValue + strlen (nodeValue) - 1) == '?'))
  {
    const char* valStart = nodeValue + 1;

    while (*valStart == ' ') valStart++;
    CS_ASSERT (*valStart != 0);
    size_t valLen = strlen (valStart) - 1;
    if (valLen != 0)
    {
      while (*(valStart + valLen - 1) == ' ') valLen--;
      const char* space = strchr (valStart, ' ');
      /* The rightmost spaces were skipped and don't interest us
         any more. */
      if (space >= valStart + valLen) space = 0;
      size_t cmdLen;
      if (space != 0)
      {
        cmdLen = space - valStart;
      }
      else
      {
        cmdLen = valLen;
      }
      command.Replace (valStart, cmdLen);
      args.Replace (valStart + cmdLen, valLen - cmdLen);
      args.LTrim();
      return true;
    }
  }
  return false;
}

static void GetNextArg (const char*& p, TempString<>& arg)
{
  arg.Clear();
  if (p == 0) return;

  while (isspace (*p)) p++;

  if (*p == '"')
  {
    p++;
    while (*p && *p != '"')
    {
      if (*p == '\\')
      {
        p++;
        switch (*p)
        {
          case '"':
            arg << '"';
            p++;
            break;
          case '\\':
            arg << '\\';
            p++;
            break;
        }
      }
      else
        arg << *p++;
    }
    p++;
  }
  else
  {
    while (*p && !isspace (*p)) arg << *p++;
  }
}

static const int syntaxErrorSeverity = CS_REPORTER_SEVERITY_WARNING;

void csWrappedDocumentNode::ProcessInclude (const TempString<>& filename,
					    NodeProcessingState* state, 
					    iDocumentNode* node)
{
  iVFS* vfs = globalState->vfs;
  csRef<iDataBuffer> pathExpanded = vfs->ExpandPath (filename);
  
  csRef<iDocumentNode> includeNode;
  includeNode = globalState->includesCache.Get (pathExpanded->GetData(),
    (iDocumentNode*)0);
  if (!includeNode.IsValid())
  {
    csRef<iFile> include = vfs->Open (filename, VFS_FILE_READ);
    if (!include.IsValid ())
    {
      Report (syntaxErrorSeverity, node,
	"could not open %s", CS::Quote::Single (filename.GetData ()));
    }
    else
    {
      csRef<iDocumentSystem> docsys (
	csQueryRegistry<iDocumentSystem> (shared->objreg));
      if (!docsys.IsValid())
	docsys.AttachNew (new csTinyDocumentSystem ());
  
      csRef<iDocument> includeDoc = docsys->CreateDocument ();
      const char* err = includeDoc->Parse (include, false);
      if (err != 0)
      {
	Report (syntaxErrorSeverity, node,
	  "error parsing %s: %s", CS::Quote::Single (filename.GetData ()), err);
	return;
      }
      else
      {
	csRef<iDocumentNode> rootNode = includeDoc->GetRoot ();
        includeNode = rootNode->GetNode ("include");
	if (!includeNode)
	{
	  Report (syntaxErrorSeverity, rootNode,
	    "%s: no <include> node", filename.GetData ());
	  return;
	}
	globalState->includesCache.Put (pathExpanded->GetData(),
          includeNode);
      }
    }
  }
  
  csVfsDirectoryChanger dirChange (vfs);
  dirChange.ChangeTo (filename);

  csRef<iDocumentNodeIterator> it = includeNode->GetNodes ();
  while (it->HasNext ())
  {
    csRef<iDocumentNode> child = it->Next ();
    ProcessSingleWrappedNode (state, child);
  }
}

bool csWrappedDocumentNode::ProcessTemplate (iDocumentNode* templNode,
					     NodeProcessingState* state)
{
  if (!(state->templActive || state->generateActive))
    return false;

  Template::Nodes& templNodes = state->templ.nodes;
  csRef<iDocumentNode> node = templNode;
  if (node->GetType() == CS_NODE_UNKNOWN)
  {
    TempString<> tokenStr, args; 
    if (SplitNodeValue (node->GetValue(), tokenStr, args))
    {
      csStringID tokenID = shared->pitokens.Request (tokenStr);
      switch (tokenID)
      {
        case csWrappedDocumentNodeFactory::PITOKEN_ENDTEMPLATE:
          if (shared->plugin->do_verbose)
          {
            Report (CS_REPORTER_SEVERITY_WARNING, node,
              "Deprecated syntax, please use %s",
	      CS::Quote::Single ("Endtemplate"));
          }
          // Fall through
	case csWrappedDocumentNodeFactory::PITOKEN_ENDTEMPLATE_NEW:
          if (!state->generateActive)
          {
	      state->templNestLevel--;
	      if (state->templNestLevel != 0)
	        templNodes.Push (node);
          }
          else
            templNodes.Push (node);
	    break;
	case csWrappedDocumentNodeFactory::PITOKEN_TEMPLATE:
          if (shared->plugin->do_verbose)
          {
            Report (CS_REPORTER_SEVERITY_WARNING, node,
              "Deprecated syntax, please use %s",
	      CS::Quote::Single ("Template"));
          }
          // Fall through
	case csWrappedDocumentNodeFactory::PITOKEN_TEMPLATE_NEW:
	case csWrappedDocumentNodeFactory::PITOKEN_TEMPLATEWEAK:
          if (!state->generateActive)
	      state->templNestLevel++;
	    // Fall through
	default:
	    {
	      Template* templ;
	      if ((!state->generateActive)
                && (state->templNestLevel == 1)
		&& (templ = globalState->templates.GetElementPointer (tokenStr)))
	      {
		Template::Params templArgs;
		ParseTemplateArguments (args, templArgs, false);
                shared->DebugProcessing ("Invoking template %s\n", 
                  tokenStr.GetData());
		InvokeTemplate (templ, templArgs, templNodes);
	      }
	      else
                // Avoid recursion when the template is later invoked
		if (tokenStr != state->templateName) templNodes.Push (node);
	    }
	    break;
	case csWrappedDocumentNodeFactory::PITOKEN_GENERATE:
          if (state->generateActive)
	      state->generateNestLevel++;
	    templNodes.Push (node);
          break;
        case csWrappedDocumentNodeFactory::PITOKEN_ENDGENERATE:
          {
            if (state->generateActive)
            {
	        state->generateNestLevel--;
	        if (state->generateNestLevel != 0)
	          templNodes.Push (node);
            }
            else
              templNodes.Push (node);
          }
          break;
      }
    }
  }
  else
    templNodes.Push (node);

  if (state->generateActive)
  {
    if (state->generateNestLevel == 0)
    {
      state->generateActive = false;
      int start = state->generateStart;
      int end = state->generateEnd;
      int step = state->generateStep;

      Template generateTempl (state->templ);
      generateTempl.paramMap.Push (state->generateVar);
      if (state->generateValid)
      {
        int v = start;
        while (true)
        {
          if (state->generateStep >= 0)
          {
            if (v > end) break;
          }
          else
          {
            if (v < end) break;
          }

          Template::Nodes templatedNodes;
          Template::Params params;
          TempString<> s; s << v;
          params.Push (s);
          shared->DebugProcessing ("Starting generation\n");
          InvokeTemplate (&generateTempl, params, templatedNodes);
          size_t i;
          for (i = 0; i < templatedNodes.GetSize (); i++)
          {
            ProcessSingleWrappedNode (state, templatedNodes[i]);
          }

          v += step;
        }
      }
    }
  }
  else
  {
    if (state->templNestLevel == 0)
    {
      if (!state->templWeak || !globalState->templates.Contains (state->templateName))
        globalState->templates.PutUnique (state->templateName, state->templ);
      state->templActive = false;
    }
  }
  return true;
}

bool csWrappedDocumentNode::InvokeTemplate (Template* templ,
					    const Template::Params& params,
					    Template::Nodes& templatedNodes)
{
  if (!templ) return false;

  csRef<Substitutions> newSubst;
  {
    Substitutions paramSubst;
    for (size_t i = 0; i < csMin (params.GetSize (), templ->paramMap.GetSize ()); i++)
    {
      shared->DebugProcessing (" %s -> %s\n", templ->paramMap[i].GetData(), 
        params[i].GetData());
      paramSubst.Put (templ->paramMap[i], params[i]);
    }
    newSubst.AttachNew (new Substitutions (paramSubst));
  }

  for (size_t i = 0; i < templ->nodes.GetSize (); i++)
  {
    csRef<iDocumentNode> newNode = 
      shared->replacerFactory.CreateWrapper (templ->nodes.Get (i), 0,
      newSubst);
    templatedNodes.Push (newNode);
  }
  return true;
}

bool csWrappedDocumentNode::InvokeTemplate (const char* name,
                                            iDocumentNode* node,
					    NodeProcessingState* state, 
					    const Template::Params& params)
{
  shared->DebugProcessing ("Invoking template %s\n", name);
  Template* templNodes = 
    globalState->templates.GetElementPointer (name);

  Template::Nodes nodes;

  if (!InvokeTemplate (templNodes, params, nodes))
    return false;

  size_t i;
  for (i = 0; i < nodes.GetSize (); i++)
  {
    ProcessSingleWrappedNode (state, nodes[i]);
  }
  return true;
}

void csWrappedDocumentNode::ValidateTemplateEnd (iDocumentNode* node, 
						 NodeProcessingState* state)
{
  if ((state->templActive) && (state->templNestLevel != 0))
  {
    Report (syntaxErrorSeverity, node,
      "%s without %s",
      CS::Quote::Single ("Template"), CS::Quote::Single ("EndTemplate"));
  }
}

void csWrappedDocumentNode::ValidateGenerateEnd (iDocumentNode* node, 
						 NodeProcessingState* state)
{
  if ((state->generateActive) && (state->generateNestLevel != 0))
  {
    Report (syntaxErrorSeverity, node,
      "%s without %s",
      CS::Quote::Single ("Generate"), CS::Quote::Single ("EndGenerate"));
  }
}

void csWrappedDocumentNode::ValidateStaticIfEnd (iDocumentNode* node, 
						 NodeProcessingState* state)
{
  if (!state->staticIfStateStack.IsEmpty())
  {
    Report (syntaxErrorSeverity, node,
      "%s without %s",
      CS::Quote::Single ("SIfDef"), CS::Quote::Single ("SEndIf"));
  }
}

void csWrappedDocumentNode::ParseTemplateArguments (const char* str, 
						    Template::Params& strings,
                                                    bool omitEmpty)
{
  if (!str) return;

  TempString<> currentStr;
  while (*str != 0)
  {
    GetNextArg (str, currentStr);
    if (!omitEmpty || !currentStr.IsEmpty())
      strings.Push (currentStr);
  }
}

bool csWrappedDocumentNode::ProcessStaticIf (NodeProcessingState* state, 
                                             iDocumentNode* node)
{
  if (state->staticIfStateStack.IsEmpty()) return false;

  if (node->GetType() == CS_NODE_UNKNOWN)
  {
    TempString<> tokenStr, args; 
    if (SplitNodeValue (node->GetValue(), tokenStr, args))
    {
      csStringID tokenID = shared->pitokens.Request (tokenStr);
      switch (tokenID)
      {
        case csWrappedDocumentNodeFactory::PITOKEN_STATIC_IFDEF:
        case csWrappedDocumentNodeFactory::PITOKEN_STATIC_IFNDEF:
          ProcessStaticIfDef (state, node, args,
            tokenID == csWrappedDocumentNodeFactory::PITOKEN_STATIC_IFNDEF);
          return true;
        case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSE:
          {
            NodeProcessingState::StaticIfState ifState = 
              state->staticIfStateStack.Pop();
            if (ifState.elseBranch)
            {
              Report (syntaxErrorSeverity, node,
                "Multiple %ss in %s",
		CS::Quote::Single ("SElse"), CS::Quote::Single ("SIfDef"));
              ifState.processNodes = false;
            }
            else
            {
              bool lastState = 
                state->staticIfStateStack.IsEmpty() ? true : 
                state->staticIfStateStack.Top().processNodes;
              ifState.processNodes = !ifState.processNodes && lastState;
              ifState.elseBranch = true;
            }
            state->staticIfStateStack.Push (ifState);
            return true;
          }
        case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSIFDEF:
        case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSIFNDEF:
          {
            NodeProcessingState::StaticIfState ifState = 
              state->staticIfStateStack.Pop();
            if (ifState.elseBranch)
            {
              Report (syntaxErrorSeverity, node,
                "Multiple %ss in %s",
		CS::Quote::Single ("SElse"), CS::Quote::Single ("SIfDef"));
              ifState.processNodes = false;
            }
            else
            {
              bool lastState = 
                state->staticIfStateStack.IsEmpty() ? true : 
                state->staticIfStateStack.Top().processNodes;
              ifState.processNodes = !ifState.processNodes && lastState;
              ifState.elseBranch = true;
            }
            ProcessStaticIfDef (state, node, args,
              tokenID == csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSIFNDEF);
            state->staticIfNest.Pop();
            state->staticIfNest.Top()++;
          }
          return true;
        case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ENDIF:
          {
            uint nest = state->staticIfNest.Pop ();
            while (nest-- > 0) state->staticIfStateStack.Pop();
            return true;
          }
      }
    }
  }
  return !state->staticIfStateStack.Top().processNodes;
}

bool csWrappedDocumentNode::ProcessInstrTemplate (NodeProcessingState* state,
                                                  iDocumentNode* node, 
                                                  const TempString<>& args, bool weak)
{
  TempString<> templateName (args);
  Template newTempl;
  size_t templateEnd = templateName.FindFirst (' ');
  if (templateEnd != (size_t)-1)
  {
    // Parse template parameter names
    Template::Params paramNames;
    ParseTemplateArguments (templateName.GetData() + templateEnd + 1, 
      paramNames, true);

    csSet<TempString<>, TempHeapAlloc> dupeCheck;
    for (size_t i = 0; i < paramNames.GetSize (); i++)
    {
      if (dupeCheck.Contains (paramNames[i]))
      {
        Report (syntaxErrorSeverity, node,
                "Duplicate template parameter %s", 
	        CS::Quote::Single (paramNames[i].GetData()));
        return false;
      }
      newTempl.paramMap.Push (paramNames[i]);
      dupeCheck.Add (paramNames[i]);
    }

    templateName.Truncate (templateEnd);
  }
  if (templateName.IsEmpty())
  {
    Report (syntaxErrorSeverity, node,
            "%s without name",
            CS::Quote::Single ("template"));
    return false;
  }
  if (shared->pitokens.Request (templateName) != csInvalidStringID)
  {
    Report (syntaxErrorSeverity, node,
            "Reserved template name %s", CS::Quote::Single (templateName.GetData()));
    return false;
  }
  state->templateName = templateName;
  state->templ = newTempl;
  state->templActive = true;
  state->templWeak = weak;
  state->templNestLevel = 1;
  return true;
}

bool csWrappedDocumentNode::ProcessDefine (NodeProcessingState* state, 
                                           iDocumentNode* node, 
                                           const TempString<>& args)
{
  TempString<> param;
  const char* p = args;
  GetNextArg (p, param);
  if (p) { while (*p && isspace (*p)) p++; }
  if (param.IsEmpty() || (*p != 0))
  {
    Report (syntaxErrorSeverity, node,
            "One parameter expected for %s",
	    CS::Quote::Single ("Define"));
    return false;
  }
  globalState->defines.Add (param);
  return true;
}

bool csWrappedDocumentNode::ProcessUndef (NodeProcessingState* state, 
                                          iDocumentNode* node, 
                                          const TempString<>& args)
{
  TempString<> param;
  const char* p = args;
  GetNextArg (p, param);
  if (p) { while (*p && isspace (*p)) p++; }
  if (param.IsEmpty() || (*p != 0))
  {
    Report (syntaxErrorSeverity, node,
            "One parameter expected for %s",
	    CS::Quote::Single ("Undef"));
    return false;
  }
  globalState->defines.Delete (param);
  return true;
}

bool csWrappedDocumentNode::ProcessStaticIfDef (NodeProcessingState* state, 
                                                iDocumentNode* node, 
                                                const TempString<>& args, bool invert)
{
  TempString<> param;
  const char* p = args;
  GetNextArg (p, param);
  if (p) { while (*p && isspace (*p)) p++; }
  if (param.IsEmpty() || (*p != 0))
  {
    Report (syntaxErrorSeverity, node,
            "One parameter expected for %s",
	    CS::Quote::Single ("SIfDef"));
    return false;
  }
  bool lastState = 
    state->staticIfStateStack.IsEmpty() ? true : 
    state->staticIfStateStack.Top().processNodes;
  bool defineExists = globalState->defines.Contains (param);
  state->staticIfStateStack.Push (
    (invert ? !defineExists : defineExists) && lastState);
  state->staticIfNest.Push (1);
  return true;
}

void csWrappedDocumentNode::ProcessSingleWrappedNode (
  NodeProcessingState* state, iDocumentNode* node)
{
  CS_ASSERT(globalState);

  if (ProcessTemplate (node, state)) return;
  if (ProcessStaticIf (state, node))
  {
    // Template invokation has precedence over dropping nodes...
    TempString<> tokenStr, args; 
    if (SplitNodeValue (node->GetValue(), tokenStr, args))
    {
      Template::Params params;
      if (!args.IsEmpty())
      {
        ParseTemplateArguments (args, params, false);
      }
      InvokeTemplate (tokenStr, node, state, params);
    }
    return;
  }

  csRefArray<iDocumentNode>& currentWrapper = state->currentWrapper;
  bool handled = false;
  if (node->GetType() == CS_NODE_UNKNOWN)
  {
    TempString<> replaceScratch;
    const char* nodeValue = ReplaceEntities (node->GetValue(),
      replaceScratch);
    if ((nodeValue != 0) && (*nodeValue == '?') && 
      (*(nodeValue + strlen (nodeValue) - 1) == '?'))
    {
      const char* valStart = nodeValue + 1;
      if ((*valStart == '!') || (*valStart == '#'))
      {
	/* Discard PIs beginning with ! and #. This allows comments, e.g.
	 * <?! some comment ?>
	 * The difference to XML comments is that the PI comments do not
	 * appear in the final document after processing, hence are useful
	 * if some PIs themselves are to be commented, but it is undesireable
	 * to have an XML comment in the result. */
	return;
      }

      while (*valStart == ' ') valStart++;
      CS_ASSERT (*valStart != 0);
      size_t valLen = strlen (valStart) - 1;
      if (valLen == 0)
      {
	Report (syntaxErrorSeverity, node,
	  "Empty processing instruction");
      }
      else
      {
	while (*(valStart + valLen - 1) == ' ') valLen--;
	const char* space = strchr (valStart, ' ');
	/* The rightmost spaces were skipped and don't interest us
	    any more. */
	if (space >= valStart + valLen) space = 0;
	size_t cmdLen;
	if (space != 0)
	{
	  cmdLen = space - valStart;
	}
	else
	{
	  cmdLen = valLen;
	}

	TempString<> tokenStr; tokenStr.Replace (valStart, cmdLen);
	csStringID tokenID = shared->pitokens.Request (tokenStr);
	switch (tokenID)
	{
	  case csWrappedDocumentNodeFactory::PITOKEN_INCLUDE:
            if (shared->plugin->do_verbose)
            {
              Report (CS_REPORTER_SEVERITY_WARNING, node,
                "Deprecated syntax, please use %s",
		CS::Quote::Single ("Include"));
            }
            // Fall through
	  case csWrappedDocumentNodeFactory::PITOKEN_INCLUDE_NEW:
	    {
	      bool okay = true;
	      TempString<> filename;
	      const char* space = strchr (valStart, ' ');
	      /* The rightmost spaces were skipped and don't interest us
	       * any more. */
	      if (space != 0)
	      {
		filename.Replace (space + 1, valLen - cmdLen - 1);
		filename.Trim ();
	      }
	      if ((space == 0) || (filename.IsEmpty ()))
	      {
		Report (syntaxErrorSeverity, node,
		  "%s without filename", CS::Quote::Single ("Include"));
		okay = false;
	      }
	      if (okay)
	      {
		ProcessInclude (filename, state, node);
	      }
	      handled = true;
	    }
	    break;
	  case csWrappedDocumentNodeFactory::PITOKEN_TEMPLATE:
            if (shared->plugin->do_verbose)
            {
              Report (CS_REPORTER_SEVERITY_WARNING, node,
                "Deprecated syntax, please use %s",
		CS::Quote::Single ("Template"));
            }
            // Fall through
          case csWrappedDocumentNodeFactory::PITOKEN_TEMPLATE_NEW:
	  case csWrappedDocumentNodeFactory::PITOKEN_TEMPLATEWEAK:
            {
              TempString<> args (valStart + cmdLen, valLen - cmdLen);
              args.LTrim();
              ProcessInstrTemplate (state, node, args, 
                tokenID == csWrappedDocumentNodeFactory::PITOKEN_TEMPLATEWEAK);
	      handled = true;
            }
	    break;
	  case csWrappedDocumentNodeFactory::PITOKEN_ENDTEMPLATE:
            if (shared->plugin->do_verbose)
            {
              Report (CS_REPORTER_SEVERITY_WARNING, node,
                "Deprecated syntax, please use %s",
		CS::Quote::Single ("Endtemplate"));
	      handled = true;
            }
            // Fall through
	  case csWrappedDocumentNodeFactory::PITOKEN_ENDTEMPLATE_NEW:
	    {
	      Report (syntaxErrorSeverity, node,
		"%s without %s",
		CS::Quote::Single ("Endtemplate"),
		CS::Quote::Single ("Template"));
              // ProcessTemplate() would've handled it otherwise
	      handled = true;
	    }
	    break;
          case csWrappedDocumentNodeFactory::PITOKEN_GENERATE:
            {
	      bool okay = true;
              Template::Params args;
	      if (space != 0)
	      {
		TempString<> pStr (space + 1, valLen - cmdLen - 1);
		ParseTemplateArguments (pStr, args, false);
	      }
              if ((args.GetSize() < 3) || (args.GetSize() > 4))
              {
                okay = false;
	        Report (syntaxErrorSeverity, node,
		  "%s expects 3 or 4 arguments, got %zu",
		  CS::Quote::Single ("Generate"), args.GetSize());
              }
              if (okay)
              {
                state->generateVar = args[0];

                int start, end, step;
                char dummy;
                if (sscanf (args[1], "%d%c", &start, &dummy) != 1)
                {
	          Report (syntaxErrorSeverity, node,
		    "Argument %s is not an integer", CS::Quote::Single (args[1].GetData()));
                  okay = false;
                }
                if (okay && sscanf (args[2], "%d%c", &end, &dummy) != 1)
                {
	          Report (syntaxErrorSeverity, node,
		    "Argument %s is not an integer", CS::Quote::Single (args[2].GetData()));
                  okay = false;
                }
                if (okay)
                {
                  if (args.GetSize() == 4)
                  {
                    if (sscanf (args[3], "%d%c", &step, &dummy) != 1)
                    {
	              Report (syntaxErrorSeverity, node,
		        "Argument %s is not an integer", CS::Quote::Single (args[3].GetData()));
                      okay = false;
                    }
                  }
                  else
                  {
                    step = (end < start) ? -1 : 1;
                  }
                }
                if (okay)
                {
                  state->generateActive = true;
		              state->generateNestLevel = 1;
                  if (((start > end) && (step >= 0))
                    || ((end >= start) && (step <= 0)))
                  {
	                  Report (syntaxErrorSeverity, node,
		                  "Can't reach end value %d starting from %d with step %d", 
                      end, start, step);
                    state->generateValid = false;
                  }
                  else
                  {
                    state->generateValid = true;
                    state->generateStart = start;
                    state->generateEnd = end;
                    state->generateStep = step;
                    state->templ = Template();
                  }
                }
              }
	      handled = true;
            }
            break;
	  case csWrappedDocumentNodeFactory::PITOKEN_ENDGENERATE:
	    {
	      Report (syntaxErrorSeverity, node,
		"%s without %s",
		CS::Quote::Single ("Endgenerate"),
		CS::Quote::Single ("Generate"));
              // ProcessTemplate() would've handled it otherwise
	    }
	    break;
	  case csWrappedDocumentNodeFactory::PITOKEN_DEFINE:
            {
              TempString<> args (valStart + cmdLen, valLen - cmdLen);
              args.LTrim();
              ProcessDefine (state, node, args);
	      handled = true;
            }
	    break;
	  case csWrappedDocumentNodeFactory::PITOKEN_UNDEF:
            {
              TempString<> args (valStart + cmdLen, valLen - cmdLen);
              args.LTrim();
              ProcessUndef (state, node, args);
	      handled = true;
            }
	    break;
	  case csWrappedDocumentNodeFactory::PITOKEN_STATIC_IFDEF:
	  case csWrappedDocumentNodeFactory::PITOKEN_STATIC_IFNDEF:
            {
              TempString<> args (valStart + cmdLen, valLen - cmdLen);
              args.LTrim();
              ProcessStaticIfDef (state, node, args,
                tokenID == csWrappedDocumentNodeFactory::PITOKEN_STATIC_IFNDEF);
	      handled = true;
            }
            break;
          case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSIFDEF:
          case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSIFNDEF:
          case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ELSE:
          case csWrappedDocumentNodeFactory::PITOKEN_STATIC_ENDIF:
            {
	      Report (syntaxErrorSeverity, node,
		"%s without %s",
		CS::Quote::Single (shared->pitokens.Request (tokenID)),
		CS::Quote::Single ("SIfDef"));
              // ProcessStaticIf() would've handled it otherwise
	      handled = true;
            }
            break;
	  default:
	    {
	      Template::Params params;
	      if (space != 0)
	      {
		TempString<> pStr (space + 1, valLen - cmdLen - 1);
		ParseTemplateArguments (pStr, params, false);
	      }
	      if (InvokeTemplate (tokenStr, node, state, params))
	      {
            handled = true;
	      }
	      // If it's neither a template nor a recognized command, pass through
	    }
	}
      }
    }
  }
  if (!handled)
  {
    csRef<iDocumentNode> newWrapper;
    newWrapper.AttachNew (new csWrappedDocumentNode (this,
      node, shared, globalState));
    currentWrapper.Push (newWrapper);
  }
}

void csWrappedDocumentNode::ProcessWrappedNode (NodeProcessingState* state,
						iDocumentNode* wrappedNode)
{
  if ((wrappedNode->GetType() == CS_NODE_ELEMENT)
    || (wrappedNode->GetType() == CS_NODE_DOCUMENT))
  {
    state->iter = wrappedNode->GetNodes ();
    while (state->iter->HasNext ())
    {
      csRef<iDocumentNode> node = state->iter->Next();
      ProcessSingleWrappedNode (state, node);
    }
    ValidateTemplateEnd (wrappedNode, state);
    ValidateGenerateEnd (wrappedNode, state);
    ValidateStaticIfEnd (wrappedNode, state);
  }
}

void csWrappedDocumentNode::Report (int severity, iDocumentNode* node, 
				    const char* msg, ...)
{
  static const char* messageID = 
    "crystalspace.graphics3d.shadercompiler.xmlshader";

  va_list args;
  va_start (args, msg);

  csRef<iSyntaxService> synsrv =
    csQueryRegistry<iSyntaxService> (shared->objreg);
  if (synsrv.IsValid ())
  {
    synsrv->ReportV (messageID, severity, node, msg, args);
  }
  else
  {
    csReportV (shared->objreg, severity, messageID, msg, args);
  }
  va_end (args);
}

void csWrappedDocumentNode::AppendNodeText (WrapperWalker& walker, 
					    csString& text)
{
  while (walker.HasNext ())
  {
    iDocumentNode* node = walker.Peek ();
    if (node->GetType () != CS_NODE_TEXT)
      break;

    text.Append (node->GetValue ());

    walker.Next ();
  }
}

bool csWrappedDocumentNode::Equals (iDocumentNode* other)
{
  return wrappedNode->Equals (((csWrappedDocumentNode*)other)->wrappedNode);
}

const char* csWrappedDocumentNode::GetValue ()
{
  return wrappedNode->GetValue();
}

#include "csutil/custom_new_disable.h"

csRef<iDocumentNodeIterator> csWrappedDocumentNode::GetNodes ()
{
  if (wrappedChildren.GetSize() > 0)
  {
    csWrappedDocumentNodeIterator* iter = 
      new (shared->iterPool) csWrappedDocumentNodeIterator (this, 0);
    return csPtr<iDocumentNodeIterator> (iter);
  }
  else if (wrappedNode.IsValid())
    return wrappedNode->GetNodes();
  else
    return 0;
}

csRef<iDocumentNodeIterator> csWrappedDocumentNode::GetNodes (
  const char* value)
{
  if (wrappedChildren.GetSize() > 0)
  {
    csWrappedDocumentNodeIterator* iter = 
      new (shared->iterPool) csWrappedDocumentNodeIterator (this, value);
    return csPtr<iDocumentNodeIterator> (iter);
  }
  else if (wrappedNode.IsValid())
    return wrappedNode->GetNodes (value);
  else
    return 0;
}

#include "csutil/custom_new_enable.h"

csRef<iDocumentNode> csWrappedDocumentNode::GetNode (const char* value)
{
  if (wrappedChildren.GetSize() > 0)
  {
    WrapperWalker walker (wrappedChildren);
    while (walker.HasNext ())
    {
      iDocumentNode* node = walker.Next ();
      if (strcmp (node->GetValue (), value) == 0)
	return node;
    }
    return 0;
  }
  else if (wrappedNode.IsValid())
    return wrappedNode->GetNode (value);
  else
    return 0;
}

const char* csWrappedDocumentNode::GetContentsValue ()
{
  /* Note: it is tempting to reuse 'contents' here if not empty; however,
   * since the value may be different depending on the current resolver
   * and its state, the contents need to be reassembled every time.
   */
  contents.Clear();
  WrapperWalker walker (wrappedChildren);
  while (walker.HasNext ())
  {
    iDocumentNode* node = walker.Next ();
    if (node->GetType() == CS_NODE_TEXT)
    {
      contents.Append (node->GetValue ());
      AppendNodeText (walker, contents);
      return contents;
    }
  }
  return 0;
}

csRef<iDocumentAttributeIterator> csWrappedDocumentNode::GetAttributes ()
{
  return wrappedNode->GetAttributes ();
}

csRef<iDocumentAttribute> csWrappedDocumentNode::GetAttribute (const char* name)
{
  return wrappedNode->GetAttribute (name);
}

const char* csWrappedDocumentNode::GetAttributeValue (const char* name)
{
  return wrappedNode->GetAttributeValue (name);
}

int csWrappedDocumentNode::GetAttributeValueAsInt (const char* name)
{
  return wrappedNode->GetAttributeValueAsInt (name);
}

float csWrappedDocumentNode::GetAttributeValueAsFloat (const char* name)
{
  return wrappedNode->GetAttributeValueAsFloat (name);
}

bool csWrappedDocumentNode::GetAttributeValueAsBool (const char* name, 
  bool defaultvalue)
{
  return wrappedNode->GetAttributeValueAsBool (name, defaultvalue);
}

//---------------------------------------------------------------------------

csWrappedDocumentNode::WrapperWalker::WrapperWalker (
  const csRefArray<iDocumentNode>& wrappedChildren)
{
  SetData (wrappedChildren);
}
    
csWrappedDocumentNode::WrapperWalker::WrapperWalker ()
{
}

void csWrappedDocumentNode::WrapperWalker::SetData (
  const csRefArray<iDocumentNode>& wrappedChildren)
{
  currentIndex = 0;
  currentWrappers = &wrappedChildren;
}

bool csWrappedDocumentNode::WrapperWalker::HasNext ()
{
  return currentIndex < currentWrappers->GetSize();
}

iDocumentNode* csWrappedDocumentNode::WrapperWalker::Peek ()
{
  return (*currentWrappers)[currentIndex] ;
}

iDocumentNode* csWrappedDocumentNode::WrapperWalker::Next ()
{
  return (*currentWrappers)[currentIndex++];
}

//---------------------------------------------------------------------------

csTextNodeWrapper::csTextNodeWrapper (iDocumentNode* realMe, 
                                      const char* text) : 
  scfPooledImplementationType (this), realMe (realMe)
{  
  nodeText = CS::StrDup (text);
}

csTextNodeWrapper::~csTextNodeWrapper ()
{
  cs_free (nodeText);
}

//---------------------------------------------------------------------------

CS_LEAKGUARD_IMPLEMENT(csWrappedDocumentNodeIterator);

csWrappedDocumentNodeIterator::csWrappedDocumentNodeIterator (
  csWrappedDocumentNode* node, const char* filter) : 
  scfPooledImplementationType (this), filter (filter), parentNode (node)
{
  walker.SetData (parentNode->wrappedChildren);

  SeekNext();
}

csWrappedDocumentNodeIterator::~csWrappedDocumentNodeIterator ()
{
}

#include "csutil/custom_new_disable.h"

void csWrappedDocumentNodeIterator::SeekNext()
{
  next = 0;
  csRef<iDocumentNode> node;
  while (walker.HasNext ())
  {
    node = walker.Next ();
    if ((filter.GetData() == 0) || (strcmp (node->GetValue (), filter) == 0))
    {
      next = node;
      break;
    }
  }
  if (next.IsValid () && (next->GetType () == CS_NODE_TEXT))
  {
    csString str;
    str.Append (next->GetValue ());
    csWrappedDocumentNode::AppendNodeText (walker, str);
    csTextNodeWrapper* textNode = 
      new (parentNode->shared->textWrapperPool) csTextNodeWrapper (next, str);
    next.AttachNew (textNode);
  }
}

#include "csutil/custom_new_enable.h"

bool csWrappedDocumentNodeIterator::HasNext ()
{
  return next.IsValid();
}

csRef<iDocumentNode> csWrappedDocumentNodeIterator::Next ()
{
  csRef<iDocumentNode> ret = next;
  SeekNext();
  return ret;
}

//---------------------------------------------------------------------------

csWrappedDocumentNodeFactory::csWrappedDocumentNodeFactory (
  DocumentPreprocessor* plugin) : scfImplementationType (this),
  plugin (plugin), objreg (plugin->objectreg)
{
  InitTokenTable (pitokens);
  pitokens.Register ("Template", PITOKEN_TEMPLATE_NEW);
  pitokens.Register ("TemplateWeak", PITOKEN_TEMPLATEWEAK);
  pitokens.Register ("Endtemplate", PITOKEN_ENDTEMPLATE_NEW);
  pitokens.Register ("Include", PITOKEN_INCLUDE_NEW);
  pitokens.Register ("Generate", PITOKEN_GENERATE);
  pitokens.Register ("Endgenerate", PITOKEN_ENDGENERATE);
  pitokens.Register ("Define", PITOKEN_DEFINE);
  pitokens.Register ("Undef", PITOKEN_UNDEF);
  pitokens.Register ("SIfDef", PITOKEN_STATIC_IFDEF);
  pitokens.Register ("SIfNDef", PITOKEN_STATIC_IFNDEF);
  pitokens.Register ("SElsIfDef", PITOKEN_STATIC_ELSIFDEF);
  pitokens.Register ("SElsIfNDef", PITOKEN_STATIC_ELSIFNDEF);
  pitokens.Register ("SElse", PITOKEN_STATIC_ELSE);
  pitokens.Register ("SEndIf", PITOKEN_STATIC_ENDIF);
}

void csWrappedDocumentNodeFactory::DebugProcessing (const char* format, ...)
{
  if (!plugin->debugInstrProcessing) return;

  va_list args;
  va_start (args, format);
  csPrintfV (format, args);
  va_end (args);
}

csWrappedDocumentNode* csWrappedDocumentNodeFactory::CreateWrapperStatic (
  iDocumentNode* wrappedNode)
{
  csWrappedDocumentNode* node;
  {
    csRef<csWrappedDocumentNode::GlobalProcessingState> globalState;
    globalState.AttachNew (csWrappedDocumentNode::GlobalProcessingState::Create ());
    globalState->vfs = csQueryRegistry<iVFS> (objreg);
    CS_ASSERT (globalState->vfs);
    node = new csWrappedDocumentNode (0, wrappedNode, this,
      globalState);
    CS_ASSERT(globalState->GetRefCount() == 1);
  }
  return node;
}

}
CS_PLUGIN_NAMESPACE_END(DocPreproc)
