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

#include "cssysdef.h"

#include "imap/services.h"
#include "iutil/cfgmgr.h"
#include "iutil/document.h"
#include "iutil/objreg.h"
#include "iutil/vfs.h"
#include "ivaria/reporter.h"

#include "csplugincommon/shader/shadercachehelper.h"
#include "csplugincommon/shader/weavertypes.h"
#include "csutil/base64.h"
#include "csutil/cfgacc.h"
#include "csutil/checksum.h"
#include "csutil/csendian.h"
#include "csutil/documenthelper.h"
#include "csutil/fifo.h"
#include "csutil/scfstr.h"
#include "csutil/set.h"
#include "csutil/stringarray.h"
#include "csutil/stringquote.h"
#include "csutil/xmltiny.h"

#include "combiner_cg.h"

CS_PLUGIN_NAMESPACE_BEGIN(SLCombiner)
{
  using namespace CS::PluginCommon;

  CS_LEAKGUARD_IMPLEMENT (ShaderCombinerLoaderCg);
  
  SCF_IMPLEMENT_FACTORY (ShaderCombinerLoaderCg)
  
  ShaderCombinerLoaderCg::ShaderCombinerLoaderCg (iBase* parent) : 
    scfImplementationType (this, parent)
  {
  }
  
  csPtr<ShaderWeaver::iCombiner> 
    ShaderCombinerLoaderCg::GetCombiner (iDocumentNode* params)
  {
    bool writeVP = true/*false*/, writeFP = true/*false*/;
    
#if 0
    if (params != 0)
    {
      csRef<iDocumentNode> vertexNode = params->GetNode ("vertex");
      if (vertexNode.IsValid())
      {
        const char* content = vertexNode->GetContentsValue ();
        if (content)
          writeVP = (strcasecmp (content, "yes") == 0)
            || (strcasecmp (content, "true") == 0)
            || (atoi (content) != 0);
      }
      
      csRef<iDocumentNode> fragmentNode = params->GetNode ("fragment");
      if (fragmentNode.IsValid())
      {
        const char* content = fragmentNode->GetContentsValue ();
        if (content)
          writeFP = (strcasecmp (content, "yes") == 0)
            || (strcasecmp (content, "true") == 0)
            || (atoi (content) != 0);
      }
    }
#endif
    csRef<ShaderCombinerCg> newCombiner;
    newCombiner.AttachNew (new ShaderCombinerCg (this, writeVP, writeFP));
    
    return csPtr<ShaderWeaver::iCombiner> (newCombiner);
  }

  void ShaderCombinerLoaderCg::GenerateConstantInputBlocks (
    iDocumentNode* node, const char* locationPrefix, const csVector4& value,
    int usedComponents, const char* outputName)
  {
    csString code;
    code << outputName;
    code << " = ";
    if (usedComponents > 1) code << "float" << usedComponents << '(';
    code << value[0];
    for (int i = 1; i < usedComponents; i++)
      code << ", " << value[i];
    if (usedComponents > 1) code << ')';
    code << ';';

    ShaderCombinerLoaderCommon::GenerateConstantInputBlocks (node, locationPrefix, code);
  }

  void ShaderCombinerLoaderCg::GenerateSVInputBlocks (iDocumentNode* node, 
    const char* locationPrefix, const char* svName, const char* outputType, 
    const char* outputName, const char* uniqueTag)
  {
    csString cgIdent = MakeIdentifier (uniqueTag);
    bool isTexture = false;
    const WeaverCommon::TypeInfo* outputTypeInfo =
      WeaverCommon::QueryTypeInfo (outputType);
    if (outputTypeInfo != 0)
      isTexture = outputTypeInfo->baseType == WeaverCommon::TypeInfo::Sampler;

    csRef<iDocumentNode> blockNode;

    if (!isTexture)
    {
      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
	csString().Format ("%s:variablemap", locationPrefix));
      {
	csRef<iDocumentNode> varMapNode;
  
	varMapNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
	varMapNode->SetValue ("variablemap");
	varMapNode->SetAttribute ("variable", svName);
	varMapNode->SetAttribute ("destination", 
	  csString().Format ("vertexIn.%s", cgIdent.GetData()));
  
	varMapNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
	varMapNode->SetValue ("variablemap");
	varMapNode->SetAttribute ("variable", svName);
	varMapNode->SetAttribute ("destination", 
	  csString().Format ("fragmentIn.%s", cgIdent.GetData()));
      }
    }
    else
    {
      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", "pass");
      {
	csRef<iDocumentNode> textureNode;
  
	textureNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
	textureNode->SetValue ("texture");
	textureNode->SetAttribute ("name", svName);
	textureNode->SetAttribute ("destination", 
	  csString().Format ("vertexIn.%s", cgIdent.GetData()));
  
	textureNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
	textureNode->SetValue ("texture");
	textureNode->SetAttribute ("name", svName);
	textureNode->SetAttribute ("destination", 
	  csString().Format ("fragmentIn.%s", cgIdent.GetData()));
      }
    }

    {
      csRef<iDocumentNode> uniformNode;

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:fragmentIn", locationPrefix));

      uniformNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
      uniformNode->SetValue ("uniform");
      uniformNode->SetAttribute ("type", outputType);
      uniformNode->SetAttribute ("name", cgIdent);

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:vertexIn", locationPrefix));

      uniformNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
      uniformNode->SetValue ("uniform");
      uniformNode->SetAttribute ("type", outputType);
      uniformNode->SetAttribute ("name", cgIdent);
    }

    {
      csRef<iDocumentNode> contents;

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:fragmentMain", locationPrefix));
      contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
      contents->SetValue (csString().Format ("%s = fragmentIn.%s;",
        outputName, cgIdent.GetData()));

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:vertexMain", locationPrefix));
      contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
      contents->SetValue (csString().Format ("%s = vertexIn.%s;",
        outputName, cgIdent.GetData()));
    }
  }

  void ShaderCombinerLoaderCg::GenerateBufferInputBlocks (iDocumentNode* node, 
    const char* locationPrefix, const char* bufName, const char* outputType, 
    const char* outputName, const char* uniqueTag)
  {
    csString cgIdent = MakeIdentifier (uniqueTag);

    csRef<iDocumentNode> blockNode;

    blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
    blockNode->SetValue ("block");
    blockNode->SetAttribute ("location", "pass");
    {
      csRef<iDocumentNode> bufferNode;

      bufferNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
      bufferNode->SetValue ("buffer");
      bufferNode->SetAttribute ("source", bufName);
      bufferNode->SetAttribute ("destination", 
        csString().Format ("vertexIn.%s", cgIdent.GetData()));
    }

    {
      csRef<iDocumentNode> varyingNode;

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:vertexToFragment", locationPrefix));

      varyingNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
      varyingNode->SetValue ("varying");
      varyingNode->SetAttribute ("type", outputType);
      varyingNode->SetAttribute ("name", cgIdent);

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:vertexIn", locationPrefix));

      varyingNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
      varyingNode->SetValue ("varying");
      varyingNode->SetAttribute ("type", outputType);
      varyingNode->SetAttribute ("name", cgIdent);
    }

    {
      csRef<iDocumentNode> contents;

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:fragmentMain", locationPrefix));
      contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
      contents->SetValue (csString().Format ("%s = %s;\n",
        outputName, cgIdent.GetData()));

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location", 
        csString().Format ("%s:vertexMain", locationPrefix));
      contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
      contents->SetValue (csString().Format (
        "%s = vertexIn.%s;\n%s = vertexIn.%s;\n",
        outputName, cgIdent.GetData(),
        cgIdent.GetData(), cgIdent.GetData()));
    }
  }

  bool ShaderCombinerLoaderCg::Initialize (iObjectRegistry* reg)
  {
    if (!ShaderCombinerLoaderCommon::Initialize (reg))
      return false;
    
    csConfigAccess config (object_reg);
    
    const char* libraryPath =
      config->GetStr ("Video.OpenGL.Shader.Cg.Combiner.CoerceLibrary");
    if (!libraryPath || !*libraryPath)
    {
      Report (CS_REPORTER_SEVERITY_ERROR, "No coercion library set up");
      return false;
    }
    
    if (!LoadCoercionLibrary (libraryPath))
      return false;
    
    return true;
  }
  
  const char* ShaderCombinerLoaderCg::GetMessageID() const
  {
    return "crystalspace.graphics3d.shader.combiner.glcg";
  }
  
  //---------------------------------------------------------------------
  
  class ShaderCombinerCg::V2FAutoSematicsHelper
  {
    csString declsPreamble;
    csString declsPostamble;
    csString cycleAutoSem;
  public:
    V2FAutoSematicsHelper (ShaderCombinerLoaderCg* loader,
      const csArray<ShaderCombinerCg::Snippet>& allSnippets)
    {
      csSet<csString> allBindings;
      
      for (size_t n = 0; n < allSnippets.GetSize(); n++)
      {
	const ShaderCombinerCg::Snippet& snippet = allSnippets[n];
	for (size_t i = 0; i < snippet.vert2frag.GetSize(); i++)
	{
	  iDocumentNode* node = snippet.vert2frag[i];
	  if (node->GetType() == CS_NODE_ELEMENT)
	  {
	    csStringID id = loader->xmltokens_common.Request (node->GetValue());
	    if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING)
	    {
	      csString name = node->GetAttributeValue ("name");
	      if (name.IsEmpty()) continue;
	      csString binding = node->GetAttributeValue ("binding");
	      if (binding.IsEmpty()) continue;
	      
	      allBindings.Add (binding);
	    }
	  }
	}
      }
      if (allBindings.Contains ("COLOR"))
	allBindings.Add ("COLOR0");
      
      static const char* const possibleBindings[] = {"COLOR1", "COLOR0"};
      const size_t numPossibleBindings =
	sizeof(possibleBindings)/sizeof(const char*);
      
      csStringArray availableBindings;
      for (size_t i = 0; i < numPossibleBindings; i++)
      {
	if (!allBindings.Contains (possibleBindings[i]))
	  availableBindings.Push (possibleBindings[i]);
      }
      
      // @@@ FIXME: actually set that define somewhere
      declsPreamble.Append ("#ifdef HAVE_ARB_color_buffer_float\n");
      for (size_t i = 0; i < availableBindings.GetSize(); i++)
      {
	declsPreamble.AppendFmt ("#define _V2F_AUTOSEMANTIC_%zu\t: %s\n", i, availableBindings[i]);
	declsPostamble.AppendFmt ("#undef _V2F_AUTOSEMANTIC_%zu\n", i);
      }
      if (availableBindings.GetSize() == 0)
      {
	declsPreamble.AppendFmt ("#define _V2F_AUTOSEMANTIC\n");
      }
      else
      {
	declsPreamble.AppendFmt ("#define _V2F_AUTOSEMANTIC\t_V2F_AUTOSEMANTIC_0\n");
	declsPreamble.AppendFmt ("#define _V2F_AUTOSEMANTIC_CURRENT\t0\n");
      }
      declsPreamble.Append ("#else\n");
      declsPreamble.Append ("#define _V2F_AUTOSEMANTIC\n");
      declsPreamble.Append ("#endif\n");
      declsPostamble.AppendFmt ("#undef _V2F_AUTOSEMANTIC\n");
      
      cycleAutoSem.Append ("#ifdef HAVE_ARB_color_buffer_float\n");
      cycleAutoSem.Append ("#if 0\n");
      for (size_t i = 0; i < availableBindings.GetSize()-1; i++)
      {
	cycleAutoSem.AppendFmt ("#elif _V2F_AUTOSEMANTIC_CURRENT == %zu\n",
	  i);
	cycleAutoSem.Append ("#undef _V2F_AUTOSEMANTIC_CURRENT\n");
	cycleAutoSem.Append ("#undef _V2F_AUTOSEMANTIC\n");
	cycleAutoSem.AppendFmt ("#define _V2F_AUTOSEMANTIC_CURRENT\t%zu\n",
	  i+1);
	cycleAutoSem.AppendFmt ("#define _V2F_AUTOSEMANTIC\t_V2F_AUTOSEMANTIC_%zu\n",
	  i+1);
      }
      cycleAutoSem.AppendFmt ("#elif _V2F_AUTOSEMANTIC_CURRENT == %zu\n",
	availableBindings.GetSize()-1);
      cycleAutoSem.Append ("#undef _V2F_AUTOSEMANTIC_CURRENT\n");
      cycleAutoSem.Append ("#undef _V2F_AUTOSEMANTIC\n");
      cycleAutoSem.AppendFmt ("#define _V2F_AUTOSEMANTIC_CURRENT\t%zu\n",
	availableBindings.GetSize());
      cycleAutoSem.AppendFmt ("#define _V2F_AUTOSEMANTIC\n");
      cycleAutoSem.Append ("#endif\n");
      cycleAutoSem.Append ("#endif\n");
    }
  
    void WriteDeclarationsPreamble (DocNodeAppender& appender) const
    {
      appender.Append (declsPreamble);
    }
    void WriteDeclarationsPostamble (DocNodeAppender& appender) const
    {
      appender.Append (declsPostamble);
    }
  
    void WriteDeclarationPostamble (csString& str) const
    {
      str.Append (cycleAutoSem);
    }
  };
        
  //---------------------------------------------------------------------
  
  ShaderCombinerCg::ShaderCombinerCg (ShaderCombinerLoaderCg* loader, 
                                      bool vp, bool fp) : 
    scfImplementationType (this), loader (loader), writeVP (vp), writeFP (fp),
    uniqueCounter (0)
  {
  }
  
  void ShaderCombinerCg::BeginSnippet (const char* annotation)
  {
    currentSnippet.annotation = annotation;
  }
  
  void ShaderCombinerCg::AddInput (const char* name, const char* type)
  {
    bool alreadyUsed = currentSnippet.localIDs.Contains (name);
    if (!alreadyUsed) currentSnippet.localIDs.AddNoTest (name);
    if (loader->annotateCombined)
    {
      currentSnippet.locals.AppendFmt ("// Input: %s %s\n", type, name);
      if (alreadyUsed) currentSnippet.locals.Append ("//");
    }
    if (loader->annotateCombined || !alreadyUsed)
    {
      currentSnippet.locals.AppendFmt ("%s %s;\n", 
	CgType (type).GetData(), name);
    }
  }

  void ShaderCombinerCg::AddInputValue (const char* name, const char* type,
                                        const char* value)
  {
    bool alreadyUsed = currentSnippet.localIDs.Contains (name);
    if (!alreadyUsed) currentSnippet.localIDs.AddNoTest (name);
    if (loader->annotateCombined)
    {
      currentSnippet.locals.AppendFmt ("// Input: %s %s\n", type, name);
      if (alreadyUsed) currentSnippet.locals.Append ("//");
    }
    if (loader->annotateCombined || !alreadyUsed)
    {
      currentSnippet.locals.AppendFmt ("%s %s;\n", 
	CgType (type).GetData(), name);
    }
    if (!alreadyUsed)
    {
      csString valueExpr;
      // @@@ Prone to break with some types :P
      valueExpr.Format ("%s (%s)", CgType (type).GetData(), value);
      currentSnippet.inputMaps.Put (valueExpr, name);
    }
  }
  
  void ShaderCombinerCg::AddOutput (const char* name, const char* type)
  {
    bool alreadyUsed = currentSnippet.localIDs.Contains (name);
    if (!alreadyUsed) currentSnippet.localIDs.AddNoTest (name);
    if (loader->annotateCombined)
    {
      currentSnippet.locals.AppendFmt ("// Output: %s %s\n", type, name);
      if (alreadyUsed) currentSnippet.locals.Append ("//");
    }
    if (loader->annotateCombined || !alreadyUsed)
    {
      currentSnippet.locals.AppendFmt ("%s %s;\n", 
	CgType (type).GetData(), name);
    }
  }
  
  void ShaderCombinerCg::InputRename (const char* fromName, 
                                      const char* toName)
  {
    currentSnippet.inputMaps.Put (fromName, toName);
  }
  
  void ShaderCombinerCg::OutputRename (const char* fromName, 
				      const char* toName)
  {
    currentSnippet.outputMaps.Put (fromName, toName);
  }
  
  void ShaderCombinerCg::PropagateAttributes (const char* fromInput, 
                                              const char* toOutput)
  {
    const char* dstName = currentSnippet.outputMaps.Get (toOutput,
      (const char*)0);
    if (dstName == 0) return;

    AttributeArray* srcAttrs = attributes.GetElementPointer (fromInput);
    if (srcAttrs == 0) return;
    attributes.PutUnique (dstName, *srcAttrs);
    for (size_t a = 0; a < srcAttrs->GetSize(); a++)
    {
      Attribute& attr = srcAttrs->Get (a);
      csString inId (GetAttrIdentifier (fromInput, attr.name));
      csString outId (GetAttrIdentifier (toOutput, attr.name));
      currentSnippet.attrInputMaps.Put (inId, outId);

      AddOutputAttribute (toOutput, attr.name, attr.type);
    }
  }

  void ShaderCombinerCg::AddOutputAttribute (const char* outputName,  
    const char* name, const char* type)
  {
    const char* dstName = currentSnippet.outputMaps.Get (outputName,
      (const char*)0);
    if (dstName == 0) return;

    AttributeArray& dstAttrs = attributes.GetOrCreate (dstName);
    Attribute* a = FindAttr (dstAttrs, name, type);
    if (a == 0)
    {
      Attribute newAttr;
      newAttr.name = name;
      newAttr.type = type;
      dstAttrs.Push (newAttr);
    }
    csString outId (GetAttrIdentifier (outputName, name));
    csString outIdMapped (GetAttrIdentifier (dstName, name));
    currentSnippet.attrOutputMaps.Put (outId, outIdMapped);

    if (loader->annotateCombined)
    {
      /* Snippet annotation may be multi-line, first line usually contains the 
         name */
      csString annotation (currentSnippet.annotation);
      size_t linebreak = annotation.FindFirst ('\n');
      if (linebreak != (size_t)-1) annotation.Truncate (linebreak);
      globals.AppendFmt ("// Attribute %s for %s\n",
        CS::Quote::Single (csString().Format ("%s %s", type, name)),
	CS::Quote::Single (annotation.GetData()));
    }
    globals.AppendFmt ("%s %s;\n", 
      CgType (type).GetData(), outIdMapped.GetData());

    if (loader->annotateCombined)
      currentSnippet.locals.AppendFmt ("// Attribute %s\n",
        CS::Quote::Single (csString().Format("%s %s", type, name)));
    currentSnippet.locals.AppendFmt ("%s %s;\n", 
      CgType (type).GetData(), outId.GetData());
  }

  void ShaderCombinerCg::AddInputAttribute (const char* inputName, 
    const char* name, const char* type, const char* defVal)
  {
    const char* srcName = currentSnippet.inputMaps.Get (inputName,
      (const char*)0);
    if (srcName == 0) return;

    AttributeArray* srcAttrs = attributes.GetElementPointer (inputName);

    csString outId (GetAttrIdentifier (srcName, name));
    if (loader->annotateCombined)
      currentSnippet.locals.AppendFmt ("// Attribute %s\n",
        CS::Quote::Single (csString().Format ("%s %s", type, name)));
    currentSnippet.locals.AppendFmt ("%s %s;\n", 
      CgType (type).GetData(), outId.GetData());

    Attribute* a = srcAttrs ? FindAttr (*srcAttrs, name, type) : 0;
    if (a != 0)
    {
      csString inId (GetAttrIdentifier (inputName, name));
      currentSnippet.attrInputMaps.Put (inId, outId);
    }
    else
    {
      currentSnippet.attrInputMaps.Put (defVal, outId);
    }
  }

  ShaderCombinerCg::Attribute* ShaderCombinerCg::FindAttr (AttributeArray& arr, 
                                                           const char* name, 
                                                           const char* type)
  {
    for (size_t a = 0; a < arr.GetSize(); a++)
    {
      Attribute& attr = arr[a];
      if ((attr.name == name) && (attr.type == type)) return &attr;
    }
    return 0;
  }

  void ShaderCombinerCg::Link (const char* fromName, const char* toName)
  {
    currentSnippet.links.AppendFmt ("%s = %s;\n",
      toName, fromName);
  }
      
  bool ShaderCombinerCg::WriteBlock (const char* location, 
                                     iDocumentNode* blockNode)
  {
    csRefArray<iDocumentNode>* destNodes = 0;
    if (strcmp (location, "variablemap") == 0)
    {
      destNodes = &variableMaps;
    }
    else if (strcmp (location, "clips") == 0)
    {
      // Cheat a bit: they go to the same parent node anyway
      destNodes = &variableMaps;
    }
    else if (strcmp (location, "vertexCompilerArgs") == 0)
    {
      destNodes = &vertexCompilerArgs;
    }
    else if (strcmp (location, "fragmentCompilerArgs") == 0)
    {
      destNodes = &fragmentCompilerArgs;
    }
    else if (strcmp (location, "vertexToFragment") == 0)
    {
      destNodes = &currentSnippet.vert2frag;
    }
    else if (strcmp (location, "vertexMain") == 0)
    {
      destNodes = &currentSnippet.vertexBody;
    }
    else if (strcmp (location, "fragmentMain") == 0)
    {
      destNodes = &currentSnippet.fragmentBody;
    }
    else if (strcmp (location, "vertexIn") == 0)
    {
      destNodes = &currentSnippet.vertexIn;
    }
    else if (strcmp (location, "fragmentIn") == 0)
    {
      destNodes = &currentSnippet.fragmentIn;
    }
    else if (strcmp (location, "definitions") == 0)
    {
      destNodes = &definitions;
    }
    
    if (destNodes != 0)
    {
      csRef<iDocumentNodeIterator> nodes = blockNode->GetNodes();
      while (nodes->HasNext())
      {
        csRef<iDocumentNode> next = nodes->Next();
        destNodes->Push (next);
      }
      return true;
    }
    else
      return false;
  }
  
  bool ShaderCombinerCg::EndSnippet ()
  {
    for (size_t n = 0; n < currentSnippet.vert2frag.GetSize(); n++)
    {
      iDocumentNode* node = currentSnippet.vert2frag[n];
      if (node->GetType() == CS_NODE_ELEMENT)
      {
        const char* name = node->GetAttributeValue ("name");
        if (name != 0)
        {
          csString nameStr (name);
          int count;
          SplitOffArrayCount (nameStr, count);
          csString uniqueName;
          uniqueName.Format ("%s_%zu", nameStr.GetData(), uniqueCounter++);
          currentSnippet.v2fMaps.PutUnique (nameStr, uniqueName);
        }
      }
    }

    snippets.Push (currentSnippet);
    currentSnippet = Snippet ();
    return true;
  }
    
  void ShaderCombinerCg::AddGlobal (const char* name, const char* type,
                                    const char* annotation)
  {
    if (!globalIDs.Contains (name))
    {
      globalIDs.AddNoTest (name);
      if (annotation) globals.Append (MakeComment (annotation));
      globals.AppendFmt ("%s %s;\n", CgType (type).GetData(), name);
    }
  }
    
  void ShaderCombinerCg::SetOutput (csRenderTargetAttachment target,
                                    const char* name,
                                    const char* annotation)
  {
    csString outputName;
    if(target == rtaDepth)
    {
      outputName = "depth";
     }
    else if(target < rtaNumAttachments)
    {
      outputName.Format("color%d", target - rtaColor0);
    }
    else
    {
      CS_ASSERT_MSG ("Unsupported program output target", false);
    }

    outputAssign[target].Empty();
    if (annotation) outputAssign[target].Append (MakeComment (annotation));
    outputAssign[target].AppendFmt ("OUT.%s = %s;\n", outputName.GetData(), name);
  }
  
  csPtr<WeaverCommon::iCoerceChainIterator> 
  ShaderCombinerCg::QueryCoerceChain (const char* fromType, const char* toType)
  {
    return loader->QueryCoerceChain (fromType, toType);
  }
  
  uint ShaderCombinerCg::CoerceCost (const char* fromType, const char* toType)
  {
    return loader->CoerceCost (fromType, toType);
  }
  
  void ShaderCombinerCg::WriteToPass (iDocumentNode* pass)
  {
    V2FAutoSematicsHelper autoSem (loader, snippets);
    
    if (writeVP)
    {
      csRef<iDocumentNode> vpNode = pass->CreateNodeBefore (CS_NODE_ELEMENT);
      vpNode->SetValue ("vp");
      vpNode->SetAttribute ("plugin", "glcg");
      
      csRef<iDocumentNode> cgvpNode = vpNode->CreateNodeBefore (CS_NODE_ELEMENT);
      cgvpNode->SetValue ("cgvp");
      if (!description.IsEmpty())
      {
	csRef<iDocumentNode> descrNode = cgvpNode->CreateNodeBefore (CS_NODE_ELEMENT);
	descrNode->SetValue ("description");
	csRef<iDocumentNode> descrValue = descrNode->CreateNodeBefore (CS_NODE_TEXT);
	descrValue->SetValue (csString().Format ("%s VP", description.GetData()));
      }
      
      csRef<iDocumentNode> entryNode = cgvpNode->CreateNodeBefore (CS_NODE_ELEMENT);
      entryNode->SetValue ("entry");
      csRef<iDocumentNode> entryValue = entryNode->CreateNodeBefore (CS_NODE_TEXT);
      entryValue->SetValue ("vertexMain");
      
      for (size_t n = 0; n < vertexCompilerArgs.GetSize(); n++)
      {
        csRef<iDocumentNode> newArgNode = 
          cgvpNode->CreateNodeBefore (CS_NODE_ELEMENT);
        newArgNode->SetValue ("compilerargs");
        
        csRef<iDocumentNode> newNode = 
          newArgNode->CreateNodeBefore (vertexCompilerArgs[n]->GetType());
        CS::DocSystem::CloneNode (vertexCompilerArgs[n], newNode);
      }
      
      for (size_t n = 0; n < variableMaps.GetSize(); n++)
      {
        csRef<iDocumentNode> newNode = 
          cgvpNode->CreateNodeBefore (variableMaps[n]->GetType());
        CS::DocSystem::CloneNode (variableMaps[n], newNode);
      }
      
      csRef<iDocumentNode> programNode = cgvpNode->CreateNodeBefore (CS_NODE_ELEMENT);
      programNode->SetValue ("program");
      
      DocNodeAppender appender (programNode);

      if (definitions.GetSize() > 0)
      {
        appender.Append ("\n");
        appender.Append (definitions);
        appender.Append ("\n");
      }
      
      appender.Append ("struct vertex2fragment\n");
      appender.Append ("{\n");
      appender.Append ("  void dummy() {}\n");
      autoSem.WriteDeclarationsPreamble (appender);
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput_V2FDecl (snippets[s], autoSem, appender);
      }
      autoSem.WriteDeclarationsPostamble (appender);
      appender.Append ("};\n\n");
      
      appender.Append ("struct VertexInput\n");
      appender.Append ("{\n");
      appender.Append ("  void dummy() {}\n");
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput (snippets[s].vertexIn, appender);
      }
      appender.Append ("};\n\n");
      
      appender.Append ("vertex2fragment vertexMain (VertexInput vertexIn)\n");
      appender.Append ("{\n");
      appender.Append ("  vertex2fragment vertexToFragment;\n");
      appender.Append (globals);
      
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        if (!snippets[s].locals.IsEmpty()
          || !snippets[s].inputMaps.IsEmpty()
          || (snippets[s].vertexBody.GetSize() > 0)
          || !snippets[s].outputMaps.IsEmpty())
	{
          if (!snippets[s].annotation.IsEmpty())
            appender.AppendFmt (MakeComment (snippets[s].annotation.GetData()));
	  appender.Append ("{\n");
          if (loader->annotateCombined)
            appender.Append ("// Locally used names for inputs + outputs\n");
	  appender.Append (snippets[s].locals);
	  AppendProgramInput_V2FLocals (snippets[s], appender);
          AppendSnippetMap (snippets[s].inputMaps, appender);
          AppendSnippetMap (snippets[s].attrInputMaps, appender);
	  appender.Append (snippets[s].vertexBody);
          appender.Append ("\n");
          AppendProgramInput_V2FVP (snippets[s], appender);
	  AppendSnippetMap (snippets[s].outputMaps, appender);
          AppendSnippetMap (snippets[s].attrOutputMaps, appender);
	  appender.Append ("}\n");
	}
	appender.Append (snippets[s].links);
        appender.Append ("\n");
      }
      
      appender.Append ("  return vertexToFragment;\n");
      appender.Append ("}\n");
    }
    
    if (writeFP)
    {
      csRef<iDocumentNode> fpNode = pass->CreateNodeBefore (CS_NODE_ELEMENT);
      fpNode->SetValue ("fp");
      fpNode->SetAttribute ("plugin", "glcg");
      
      csRef<iDocumentNode> cgfpNode = fpNode->CreateNodeBefore (CS_NODE_ELEMENT);
      cgfpNode->SetValue ("cgfp");
      if (!description.IsEmpty())
      {
	csRef<iDocumentNode> descrNode = cgfpNode->CreateNodeBefore (CS_NODE_ELEMENT);
	descrNode->SetValue ("description");
	csRef<iDocumentNode> descrValue = descrNode->CreateNodeBefore (CS_NODE_TEXT);
	descrValue->SetValue (csString().Format ("%s FP", description.GetData()));
      }
      
      csRef<iDocumentNode> entryNode = cgfpNode->CreateNodeBefore (CS_NODE_ELEMENT);
      entryNode->SetValue ("entry");
      csRef<iDocumentNode> entryValue = entryNode->CreateNodeBefore (CS_NODE_TEXT);
      entryValue->SetValue ("fragmentMain");
      
      for (size_t n = 0; n < fragmentCompilerArgs.GetSize(); n++)
      {
        csRef<iDocumentNode> newNode = 
          cgfpNode->CreateNodeBefore (fragmentCompilerArgs[n]->GetType());
        CS::DocSystem::CloneNode (fragmentCompilerArgs[n], newNode);
      }
      
      for (size_t n = 0; n < variableMaps.GetSize(); n++)
      {
        csRef<iDocumentNode> newNode = 
          cgfpNode->CreateNodeBefore (variableMaps[n]->GetType());
        CS::DocSystem::CloneNode (variableMaps[n], newNode);
      }
      
      csRef<iDocumentNode> programNode = cgfpNode->CreateNodeBefore (CS_NODE_ELEMENT);
      programNode->SetValue ("program");
      
      DocNodeAppender appender (programNode);
      
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput_V2FHead (snippets[s], appender);
      }
      
      if (definitions.GetSize() > 0)
      {
        appender.Append ("\n");
        appender.Append (definitions);
        appender.Append ("\n");
      }
      
      appender.Append ("struct vertex2fragment\n");
      appender.Append ("{\n");
      appender.Append ("  void dummy() {}\n");
      autoSem.WriteDeclarationsPreamble (appender);
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput_V2FDecl (snippets[s], autoSem, appender);
      }
      autoSem.WriteDeclarationsPostamble (appender);
      appender.Append ("};\n\n");
      
      appender.Append ("struct FragmentInput\n");
      appender.Append ("{\n");
      appender.Append ("  void dummy() {}\n");
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput (snippets[s].fragmentIn, appender);
      }
      appender.Append ("};\n\n");
      
      appender.Append ("struct FragmentOutput\n");
      appender.Append ("{\n");
      for(size_t s = 0; s < rtaNumAttachments - rtaColor0; ++s)
      {
        appender.AppendFmt ("  float4 color%d : COLOR%d;\n", s, s);
      }
      appender.Append ("  float depth : DEPTH;\n");
      appender.Append ("};\n\n");
      
      appender.Append ("FragmentOutput fragmentMain (vertex2fragment vertexToFragment, FragmentInput fragmentIn)\n");
      appender.Append ("{\n");
      appender.Append (globals);
      
      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        if (!snippets[s].locals.IsEmpty()
          || !snippets[s].inputMaps.IsEmpty()
          || (snippets[s].fragmentBody.GetSize() > 0)
          || !snippets[s].outputMaps.IsEmpty())
	{
          if (!snippets[s].annotation.IsEmpty())
            appender.AppendFmt (MakeComment (snippets[s].annotation.GetData()));
	  appender.Append ("{\n");
          if (loader->annotateCombined)
            appender.Append ("// Locally used names for inputs + outputs\n");
	  appender.Append (snippets[s].locals);
	  AppendProgramInput_V2FLocals (snippets[s], appender);
          AppendSnippetMap (snippets[s].inputMaps, appender);
          AppendSnippetMap (snippets[s].attrInputMaps, appender);
          AppendProgramInput_V2FFP (snippets[s], appender);
	  appender.Append (snippets[s].fragmentBody);
          appender.Append ("\n");
          AppendSnippetMap (snippets[s].outputMaps, appender);
          AppendSnippetMap (snippets[s].attrOutputMaps, appender);
	  appender.Append ("}\n");
	}
	appender.Append (snippets[s].links);
        appender.Append ("\n");
      }
      
      if (loader->annotateCombined)
        appender.Append ("  // Fragment program output\n");
      appender.Append ("  FragmentOutput OUT;\n");
      for (int a = 0; a < rtaNumAttachments; a++)
        appender.Append (outputAssign[a]);
      appender.Append ("  return OUT;\n");
      appender.Append ("}\n");
    }
  }
    
  bool ShaderCombinerCg::CompatibleParams (iDocumentNode* params)
  {
    return true;
  }
    
  csRef<iString> ShaderCombinerCg::QueryInputTag (const char* location, 
                                                  iDocumentNode* blockNode)
  {
    csRef<iString> result;
    /* @@@ FIXME: Also check vertexToFragment? */
    if ((strcmp (location, "vertexIn") == 0)
      || (strcmp (location, "fragmentIn") == 0))
    {
      csRef<iDocumentNodeIterator> nodes = blockNode->GetNodes();
      while (nodes->HasNext())
      {
        csRef<iDocumentNode> node = nodes->Next();
        if (node->GetType() != CS_NODE_ELEMENT) continue;

        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if ((id == ShaderCombinerLoaderCommon::XMLTOKEN_UNIFORM)
          || (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING))
        {
          const char* binding = node->GetAttributeValue ("binding");
          if (!binding || !*binding) continue;
          // For now only support 1 tag...
          if (result.IsValid() 
            && (strcmp (result->GetData(), binding) != 0)) return 0;
          result.AttachNew (new scfString (binding));
        }
      }
    }
    else if (strcmp (location, "variablemap") == 0)
    {
      csRef<iDocumentNodeIterator> nodes = blockNode->GetNodes();
      while (nodes->HasNext())
      {
        csRef<iDocumentNode> node = nodes->Next();
        if (node->GetType() != CS_NODE_ELEMENT) continue;

        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARIABLEMAP)
        {
          const char* variable = node->GetAttributeValue ("variable");
          const char* type = node->GetAttributeValue ("type");
          if (variable && *variable)
          {
            // For now only support 1 tag...
            if (result.IsValid() 
              && (strcmp (result->GetData(), variable) != 0)) return 0;
            result.AttachNew (new scfString (variable));
          }
          else if (type && ((strcmp (type, "expr") == 0)
              || (strcmp (type, "expression") == 0)))
          {
            csString tagStr;
            csRef<iDocumentNodeIterator> children = node->GetNodes();
            while (children->HasNext())
            {
              csRef<iDocumentNode> child = children->Next();
              if (child->GetType() == CS_NODE_COMMENT) continue;
              tagStr.Append (CS::DocSystem::FlattenNode (child));
            }
            // For now only support 1 tag...
            if (result.IsValid() 
              && (strcmp (result->GetData(), tagStr) != 0)) return 0;
            result.AttachNew (new scfString (tagStr));
          }
        }
      }
    }
    return result;
  }

  void ShaderCombinerCg::AppendProgramInput (
    const csRefArray<iDocumentNode>& nodes,
    DocNodeAppender& appender)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < nodes.GetSize(); n++)
    {
      iDocumentNode* node = nodes[n];
      AppendProgramInput (node, appender);
    }
  }
  
  void ShaderCombinerCg::AppendProgramInput_V2FHead (
    const Snippet& snippet, DocNodeAppender& appender)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < snippet.vert2frag.GetSize(); n++)
    {
      iDocumentNode* node = snippet.vert2frag[n];
      if (node->GetType() == CS_NODE_ELEMENT)
      {
        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING)
        {
          csString name = node->GetAttributeValue ("name");
          if (name.IsEmpty()) continue;
          int count;
          SplitOffArrayCount (name, count);
          
          const csString& uniqueName = snippet.v2fMaps.Get (name, name);
          if (count > 0)
          {
	    csString defineName;
	    for (int i = 0; i < count; i++)
	    {
	      defineName.Format ("PARAM_vertexToFragment_%s_%d__UNUSED", 
		uniqueName.GetData(), i);
	      appender.AppendFmt ("//@@UNUSED? %s\n", defineName.GetData());
	    }
          }
          else
          {
	    csString defineName;
	    defineName.Format ("PARAM_vertexToFragment_%s_UNUSED", 
	      uniqueName.GetData());
	    appender.AppendFmt ("//@@UNUSED? %s\n", defineName.GetData());
	  }
        }
      }
    }
  }
  
  void ShaderCombinerCg::AppendProgramInput_V2FDecl (
    const Snippet& snippet, const V2FAutoSematicsHelper& semanticsHelper,
    DocNodeAppender& appender)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < snippet.vert2frag.GetSize(); n++)
    {
      iDocumentNode* node = snippet.vert2frag[n];
      if (node->GetType() == CS_NODE_ELEMENT)
      {
        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING)
        {
          csString name = node->GetAttributeValue ("name");
          if (name.IsEmpty()) continue;
          int count;
          SplitOffArrayCount (name, count);
          
          const csString& uniqueName = snippet.v2fMaps.Get (name, name);
          if (count > 0)
          {
	    appender.AppendFmt ("#if 0\n");
	    csString defineName;
	    for (int i = count; i-- > 0; )
	    {
	      defineName.Format ("PARAM_vertexToFragment_%s_%d__UNUSED", 
		uniqueName.GetData(), i);
	      appender.AppendFmt ("#elif !defined(%s)\n", defineName.GetData());
	      const char* type = node->GetAttributeValue ("type");
	      const char* binding = node->GetAttributeValue ("binding");
	      if (type && *type)
	      {
		csString bindingStr;
		if (binding) bindingStr.Format (" : %s", binding);
		csString str;
		str.Format ("varying %s %s[%d]%s;\n", 
		  CgType (type).GetData(), uniqueName.GetData(), 
		  i+1, bindingStr.GetDataSafe());
		appender.Append (str);
	      }
	    }
	    appender.Append ("#endif\n");
          }
          else
          {
	    csString defineName;
	    defineName.Format ("PARAM_vertexToFragment_%s_UNUSED", 
	      uniqueName.GetData());
	    appender.AppendFmt ("#ifndef %s\n", defineName.GetData());
	    const char* type = node->GetAttributeValue ("type");
	    const char* binding = node->GetAttributeValue ("binding");
	    if (type && *type)
	    {
	      csString bindingStr;
	      if (binding) bindingStr.Format (" : %s", binding);
	      csString str;
	      if (bindingStr.IsEmpty())
	      {
		str.Format ("varying %s %s _V2F_AUTOSEMANTIC ;\n", 
		  CgType (type).GetData(), uniqueName.GetData());
		semanticsHelper.WriteDeclarationPostamble (str);
	      }
	      else
		str.Format ("varying %s %s%s;\n", 
		  CgType (type).GetData(), uniqueName.GetData(), 
		  bindingStr.GetDataSafe());
	      appender.Append (str);
	    }
	    appender.Append ("#endif\n");
	  }
        }
      }
      else
      {
        AppendProgramInput (node, appender);
      }
    }
  }
  
  void ShaderCombinerCg::AppendProgramInput_V2FLocals (
    const Snippet& snippet, DocNodeAppender& appender)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < snippet.vert2frag.GetSize(); n++)
    {
      iDocumentNode* node = snippet.vert2frag[n];
      if (node->GetType() == CS_NODE_ELEMENT)
      {
        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING)
        {
          csString name = node->GetAttributeValue ("name");
	  const char* type = node->GetAttributeValue ("type");
          int count;
          SplitOffArrayCount (name, count);
          
	  bool alreadyUsed = snippet.localIDs.Contains (name);
	  if (loader->annotateCombined)
	  {
	    appender.AppendFmt ("// Vertex to fragment: %s %s\n", type, 
	      name.GetData());
	    if (alreadyUsed) appender.AppendFmt ("//");
	  }
	  if (loader->annotateCombined || !alreadyUsed)
	  {
	    if (count > 0)
	    {
	      appender.AppendFmt ("%s %s[%d];\n", 
		CgType (type).GetData(), name.GetData(), count);
	    }
	    else
	    {
	      appender.AppendFmt ("%s %s;\n", 
		CgType (type).GetData(), name.GetData());
	    }
	  }
        }
      }
      else
      {
        AppendProgramInput (node, appender);
      }
    }
  }
  
  void ShaderCombinerCg::AppendProgramInput_V2FVP (
    const Snippet& snippet, DocNodeAppender& appender)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < snippet.vert2frag.GetSize(); n++)
    {
      iDocumentNode* node = snippet.vert2frag[n];
      if (node->GetType() == CS_NODE_ELEMENT)
      {
        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING)
        {
          csString name = node->GetAttributeValue ("name");
          int count;
          SplitOffArrayCount (name, count);
          const csString& uniqueName = snippet.v2fMaps.Get (name, name);
          
          if (count > 0)
          {
	    csString defineName;
	    for (int i = 0; i < count; i++)
	    {
	      defineName.Format ("PARAM_vertexToFragment_%s_%d__UNUSED", 
		uniqueName.GetData(), i);
	      appender.AppendFmt ("#ifndef %s\n", defineName.GetData());
	      appender.AppendFmt ("vertexToFragment.%s[%d] = %s[%d];\n", 
		uniqueName.GetData(), i, name.GetData(), i);
	      appender.Append ("#endif\n");
	    }
          }
          else
          {
	    csString defineName;
	    defineName.Format ("PARAM_vertexToFragment_%s_UNUSED", 
	      uniqueName.GetData());
	    appender.AppendFmt ("#ifndef %s\n", defineName.GetData());
	    appender.AppendFmt ("vertexToFragment.%s = %s;\n", 
	      uniqueName.GetData(), name.GetData());
	    appender.Append ("#endif\n");
	  }
        }
      }
      else
      {
        AppendProgramInput (node, appender);
      }
    }
  }
  
  void ShaderCombinerCg::AppendProgramInput_V2FFP (
    const Snippet& snippet, DocNodeAppender& appender)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < snippet.vert2frag.GetSize(); n++)
    {
      iDocumentNode* node = snippet.vert2frag[n];
      if (node->GetType() == CS_NODE_ELEMENT)
      {
        csStringID id = loader->xmltokens_common.Request (node->GetValue());
        if (id == ShaderCombinerLoaderCommon::XMLTOKEN_VARYING)
        {
          csString name = node->GetAttributeValue ("name");
          int count;
          SplitOffArrayCount (name, count);
          const csString& uniqueName = snippet.v2fMaps.Get (name, name);
          
          if (count > 0)
          {
	    csString defineName;
	    for (int i = 0; i < count; i++)
	    {
	      defineName.Format ("PARAM_vertexToFragment_%s_%d__UNUSED", 
		uniqueName.GetData(), i);
	      appender.AppendFmt ("#ifndef %s\n", defineName.GetData());
	      appender.AppendFmt ("%s[%d] = vertexToFragment.%s[%d];\n", 
		name.GetData(), i, uniqueName.GetData(), i);
	      appender.Append ("#else\n");
	      appender.Append ("#ifdef _INITIALIZE_UNUSED_V2F\n");
	      appender.AppendFmt ("%s[%d] = %s(0);\n", 
		name.GetData(), i, 
		CgType (node->GetAttributeValue ("type")).GetData());
	      appender.Append ("#endif\n");
	      appender.Append ("#endif\n");
	    }
          }
          else
          {
	    csString defineName;
	    defineName.Format ("PARAM_vertexToFragment_%s_UNUSED", 
	      uniqueName.GetData());
	    appender.AppendFmt ("#ifndef %s\n", defineName.GetData());
	    appender.AppendFmt ("%s = vertexToFragment.%s;\n", 
	      name.GetData(), uniqueName.GetData());
	    appender.Append ("#else\n");
	      appender.Append ("#ifdef _INITIALIZE_UNUSED_V2F\n");
	    appender.AppendFmt ("%s = %s(0);\n", 
	      name.GetData(), 
	      CgType (node->GetAttributeValue ("type")).GetData());
	      appender.Append ("#endif\n");
	    appender.Append ("#endif\n");
	  }
        }
      }
      else
      {
        AppendProgramInput (node, appender);
      }
    }
  }
  
  void ShaderCombinerCg::AppendProgramInput (iDocumentNode* node,
    DocNodeAppender& appender)
  {
    if (node->GetType() == CS_NODE_ELEMENT)
    {
      csStringID id = loader->xmltokens_common.Request (node->GetValue());
      switch (id)
      {
	case ShaderCombinerLoaderCommon::XMLTOKEN_UNIFORM:
	case ShaderCombinerLoaderCommon::XMLTOKEN_VARYING:
          {
	    const char* name = node->GetAttributeValue ("name");
	    const char* type = node->GetAttributeValue ("type");
	    const ShaderWeaver::TypeInfo* typeInfo = 
	      ShaderWeaver::QueryTypeInfo (type);
	    const char* binding = node->GetAttributeValue ("binding");
	    if (name && *name && type && *type)
	    {
	      csString bindingStr;
	      if (binding) bindingStr.Format (" : %s", binding);
	      csString str;
              str.Format ("%s %s %s%s;\n", 
	        (id == ShaderCombinerLoaderCommon::XMLTOKEN_UNIFORM) ? "uniform" :
	        "varying",
	        typeInfo ? CgType (typeInfo).GetData() : type,
		name, bindingStr.GetDataSafe());
	      if (typeInfo && (typeInfo->baseType == ShaderWeaver::TypeInfo::Sampler))
	      {
		/* This is a workaround for a weird Cg bug:
		   In FP2.0/PS1.x programs a constant will not be used
		   correctly if it directly follows a sampler.
		   Workaround: add an unused constant right after the sampler ... */
		str.AppendFmt ("uniform float4 %s_Cg_WTF;\n", name);
	      }
    	      appender.Append (str);
            }
          }
          break;
      }
    }
    else
    {
      appender.Append (node);
    }
  }
  
  csString ShaderCombinerCg::CgType (const char* weaverType)
  {
    const ShaderWeaver::TypeInfo* typeInfo = 
      ShaderWeaver::QueryTypeInfo (weaverType);
    csString result;
    if (typeInfo)
    {
      result = CgType (typeInfo);
    }
    // @@@ Hmmm... what fallback, if any?
    return result.IsEmpty() ? csString (weaverType) : result;
  }
  
  csString ShaderCombinerCg::CgType (const ShaderWeaver::TypeInfo* typeInfo)
  {
    switch (typeInfo->baseType)
    {
      case ShaderWeaver::TypeInfo::Vector:
      case ShaderWeaver::TypeInfo::VectorB:
      case ShaderWeaver::TypeInfo::VectorI:
	{
	  static const char* const baseTypeStrs[] = 
	  { "float", "bool", "int" };
	  const char* baseTypeStr = baseTypeStrs[typeInfo->baseType -
	    ShaderWeaver::TypeInfo::Vector];
	  if (typeInfo->dimensions == 1)
	    return baseTypeStr;
	  else
	    return csString().Format ("%s%d", baseTypeStr, typeInfo->dimensions);
	}
      case ShaderWeaver::TypeInfo::Matrix:
      case ShaderWeaver::TypeInfo::MatrixB:
      case ShaderWeaver::TypeInfo::MatrixI:
        {
          static const char* const baseTypeStrs[] =
          { "float", "bool", "int" };
          const char* baseTypeStr = baseTypeStrs[typeInfo->baseType -
            ShaderWeaver::TypeInfo::Matrix];
          return csString().Format ("%s%dx%d", baseTypeStr, typeInfo->dimensions, typeInfo->dimensions2);
        }
      case ShaderWeaver::TypeInfo::Sampler:
	if (typeInfo->samplerIsCube)
	  return "samplerCUBE";
	else
	  return csString().Format ("sampler%dD", typeInfo->dimensions);
    }
    return csString();
  }

  csString ShaderCombinerCg::GetAttrIdentifier (const char* var, 
                                                const char* attr)
  {
    csString s;
    s.Format ("%s_attr_%s", var, attr);
    return s;
  }

  const char* ShaderCombinerCg::MakeComment (const char* s)
  {
    const char* linebreak;
    linebreak = strpbrk (s, "\r\n");
    if (strpbrk (s, "\r\n") == 0)
    {
      annotateStr.Format ("// %s\n", s);
    }
    else
    {
      annotateStr.Replace ("/* ");
      const char* lineStart = s;
      while (linebreak != 0)
      {
        annotateStr.Append (lineStart, linebreak - lineStart);
        annotateStr.Append ("\n   ");
        lineStart = linebreak+1;
        linebreak = strpbrk (lineStart, "\r\n");
      }
      annotateStr.Append (lineStart);
        annotateStr.Append ("\n */\n");
    }
    return annotateStr;
  }

  void ShaderCombinerCg::AppendComment (csString& s, const char* contents)
  {
    const char* linebreak;
    linebreak = strpbrk (contents, "\r\n");
    if (linebreak == 0)
    {
      s.AppendFmt ("// %s\n", contents);
    }
    else
    {
      s.Append ("/* ");
      const char* lineStart = contents;
      while (linebreak != 0)
      {
        s.Append (lineStart, linebreak - lineStart);
        s.Append ("\n   ");
        lineStart = linebreak+1;
        linebreak = strpbrk (lineStart, "\r\n");
      }
      s.Append (lineStart);
        s.Append ("\n */\n");
    }
  }

  void ShaderCombinerCg::AppendSnippetMap (const csHash<csString, csString>& map, 
                                           DocNodeAppender& appender)
  {
    csHash<csString, csString>::ConstGlobalIterator it = map.GetIterator ();
    while (it.HasNext())
    {
      csString fromName;
      const csString& toName = it.Next (fromName);
      if (fromName != toName)
        appender.AppendFmt ("%s = %s;\n", toName.GetData(), fromName.GetData());
    }
  }
  
}
CS_PLUGIN_NAMESPACE_END(SLCombiner)
