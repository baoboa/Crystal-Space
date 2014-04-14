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

#include "cssysdef.h"

#include "combiner_glsl.h"

#include "ivaria/reporter.h"

#include "csutil/cfgacc.h"
#include "csutil/scfstr.h"
#include "csutil/stringarray.h"
#include "csutil/stringquote.h"
#include "csutil/stringreader.h"
#include "csplugincommon/shader/weavertypes.h"

CS_PLUGIN_NAMESPACE_BEGIN(SLCombiner)
{
  using namespace CS::PluginCommon;

  SCF_IMPLEMENT_FACTORY (ShaderCombinerLoaderGLSL)
  
  ShaderCombinerLoaderGLSL::ShaderCombinerLoaderGLSL (iBase* parent) :
    scfImplementationType (this, parent)
  {
  }

  bool ShaderCombinerLoaderGLSL::Initialize (iObjectRegistry* reg)
  {
    if (!ShaderCombinerLoaderCommon::Initialize (reg))
      return false;

    csConfigAccess config (object_reg);

    const char* libraryPath =
      config->GetStr ("Video.OpenGL.Shader.GLSL.Combiner.CoerceLibrary");
    if (!libraryPath || !*libraryPath)
    {
      Report (CS_REPORTER_SEVERITY_ERROR, "No coercion library set up");
      return false;
    }

    if (!LoadCoercionLibrary (libraryPath))
      return false;

    return true;
  }

  csPtr<ShaderWeaver::iCombiner>
    ShaderCombinerLoaderGLSL::GetCombiner (iDocumentNode* params)
  {
    csRef<ShaderCombinerGLSL> newCombiner;
    newCombiner.AttachNew (new ShaderCombinerGLSL (this));

    return csPtr<ShaderWeaver::iCombiner> (newCombiner);
  }

  void ShaderCombinerLoaderGLSL::GenerateConstantInputBlocks (
    iDocumentNode* node, const char* locationPrefix, const csVector4& value,
    int usedComponents, const char* outputName)
  {
    csString code;
    code << outputName;
    code << " = ";
    if (usedComponents > 1) code << "vec" << usedComponents << '(';
    code << value[0];
    for (int i = 1; i < usedComponents; i++)
      code << ", " << value[i];
    if (usedComponents > 1) code << ')';
    code << ';';

    ShaderCombinerLoaderCommon::GenerateConstantInputBlocks (node, locationPrefix, code);
  }

  void ShaderCombinerLoaderGLSL::GenerateSVInputBlocks (iDocumentNode* node,
    const char* locationPrefix, const char* svName, const char* outputType,
    const char* outputName, const char* uniqueTag)
  {
    csString ident = MakeIdentifier (uniqueTag);
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
          csString().Format ("in_%s", ident.GetData()));
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
          csString().Format ("in_%s", ident.GetData()));
      }
    }

    {
      csRef<iDocumentNode> uniformNode;

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location",
        csString().Format ("%s:inputs", locationPrefix));

      uniformNode = blockNode->CreateNodeBefore (CS_NODE_ELEMENT);
      uniformNode->SetValue ("uniform");
      uniformNode->SetAttribute ("type", outputType);
      uniformNode->SetAttribute ("name",
        csString().Format ("in_%s", ident.GetData()));
    }

    {
      csRef<iDocumentNode> contents;

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location",
        csString().Format ("%s:fragmentMain", locationPrefix));
      contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
      if (!isTexture)
        contents->SetValue (csString().Format ("%s = in_%s;",
          outputName, ident.GetData()));
      else
        contents->SetValue (csString().Format ("#sampler_assign %s in_%s",
          outputName, ident.GetData()));

      blockNode = node->CreateNodeBefore (CS_NODE_ELEMENT);
      blockNode->SetValue ("block");
      blockNode->SetAttribute ("location",
        csString().Format ("%s:vertexMain", locationPrefix));
      contents = blockNode->CreateNodeBefore (CS_NODE_TEXT);
      if (!isTexture)
        contents->SetValue (csString().Format ("%s = in_%s;",
          outputName, ident.GetData()));
      else
        contents->SetValue (csString().Format ("#sampler_assign %s in_%s",
          outputName, ident.GetData()));
    }
  }

  void ShaderCombinerLoaderGLSL::GenerateBufferInputBlocks (iDocumentNode* node,
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
        csString().Format ("vp_%s", cgIdent.GetData()));
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
        "%s = vp_%s;\n%s = vp_%s;\n",
        outputName, cgIdent.GetData(),
        cgIdent.GetData(), cgIdent.GetData()));
    }
  }

  const char* ShaderCombinerLoaderGLSL::GetMessageID() const
  {
    return "crystalspace.graphics3d.shader.combiner.glsl";
  }

  //---------------------------------------------------------------------
  
  ShaderCombinerGLSL::ShaderCombinerGLSL (ShaderCombinerLoaderGLSL* loader) :
    scfImplementationType (this), loader (loader), uniqueCounter (0),
    requiredVersion (0)
  {
  }

  void ShaderCombinerGLSL::BeginSnippet (const char* annotation)
  {
    csHash<const WeaverCommon::TypeInfo*, csString>::GlobalIterator globalsTypesIt (globalsTypes.GetIterator());
    while (globalsTypesIt.HasNext())
    {
      csString globalName;
      const WeaverCommon::TypeInfo* globalType (globalsTypesIt.Next (globalName));
      currentSnippet.inputMapsTypes.Put (globalName, globalType);
    }

    currentSnippet.annotation = annotation;
  }

  void ShaderCombinerGLSL::AddInput (const char* name, const char* type)
  {
    bool alreadyUsed = currentSnippet.localIDs.Contains (name);
    if (!alreadyUsed) currentSnippet.localIDs.AddNoTest (name);
    if (loader->annotateCombined)
    {
      currentSnippet.locals.AppendFmt ("// Input: %s %s\n", type, name);
      if (alreadyUsed) currentSnippet.locals.Append ("//");
    }
    const ShaderWeaver::TypeInfo* typeInfo =
      ShaderWeaver::QueryTypeInfo (type);
    if (typeInfo && (typeInfo->baseType == ShaderWeaver::TypeInfo::Sampler))
    {
      if (!loader->annotateCombined || !alreadyUsed) currentSnippet.locals.Append ("//");
    }
    if (loader->annotateCombined || !alreadyUsed)
    {
      currentSnippet.locals.AppendFmt ("%s %s;\n",
        typeInfo ? GLSLType (typeInfo).GetData() : type, name);
    }
    currentSnippet.inputMapsTypes.Put (name, typeInfo);
  }

  void ShaderCombinerGLSL::AddInputValue (const char* name, const char* type,
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
        GLSLType (type).GetData(), name);
    }
    if (!alreadyUsed)
    {
      csString valueExpr;
      // @@@ Prone to break with some types :P
      valueExpr.Format ("%s (%s)", GLSLType (type).GetData(), value);
      currentSnippet.inputMaps.Put (valueExpr, name);
      currentSnippet.inputMapsTypes.Put (valueExpr, WeaverCommon::QueryTypeInfo (type));
    }
  }

  void ShaderCombinerGLSL::AddOutput (const char* name, const char* type)
  {
    bool alreadyUsed = currentSnippet.localIDs.Contains (name);
    if (!alreadyUsed) currentSnippet.localIDs.AddNoTest (name);
    if (loader->annotateCombined)
    {
      currentSnippet.locals.AppendFmt ("// Output: %s %s\n", type, name);
      if (alreadyUsed) currentSnippet.locals.Append ("//");
    }
    const ShaderWeaver::TypeInfo* typeInfo =
      ShaderWeaver::QueryTypeInfo (type);
    if (typeInfo && (typeInfo->baseType == ShaderWeaver::TypeInfo::Sampler))
    {
      if (!loader->annotateCombined || !alreadyUsed) currentSnippet.locals.Append ("//");
    }
    if (loader->annotateCombined || !alreadyUsed)
    {
      currentSnippet.locals.AppendFmt ("%s %s;\n",
        GLSLType (type).GetData(), name);
    }
    currentSnippet.outputMapsTypes.Put (name, typeInfo);
  }

  void ShaderCombinerGLSL::InputRename (const char* fromName,
                                        const char* toName)
  {
    currentSnippet.inputMaps.Put (fromName, toName);
    currentSnippet.inputMapsTypes.Put (toName,
                                       currentSnippet.inputMapsTypes.Get (fromName, nullptr));
  }

  void ShaderCombinerGLSL::OutputRename (const char* fromName,
                                         const char* toName)
  {
    currentSnippet.outputMaps.Put (fromName, toName);
    currentSnippet.outputMapsTypes.Put (toName,
                                        currentSnippet.outputMapsTypes.Get (fromName, nullptr));
  }

  void ShaderCombinerGLSL::PropagateAttributes (const char* fromInput,
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
      currentSnippet.attrInputMapsTypes.Put (inId, WeaverCommon::QueryTypeInfo (attr.type));

      AddOutputAttribute (toOutput, attr.name, attr.type);
    }
  }

  void ShaderCombinerGLSL::AddOutputAttribute (const char* outputName,
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
    currentSnippet.attrOutputMapsTypes.Put (outId, ShaderWeaver::QueryTypeInfo (type));

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
      GLSLType (type).GetData(), outIdMapped.GetData());

    if (loader->annotateCombined)
      currentSnippet.locals.AppendFmt ("// Attribute %s\n",
        CS::Quote::Single (csString().Format("%s %s", type, name)));
    currentSnippet.locals.AppendFmt ("%s %s;\n",
      GLSLType (type).GetData(), outId.GetData());
  }

  void ShaderCombinerGLSL::AddInputAttribute (const char* inputName,
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
      GLSLType (type).GetData(), outId.GetData());

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

  void ShaderCombinerGLSL::Link (const char* fromName, const char* toName)
  {
    currentSnippet.links.AppendFmt ("%s = %s;\n",
      toName, fromName);
  }

  bool ShaderCombinerGLSL::WriteBlock (const char* location,
                                     iDocumentNode* blockNode)
  {
    csRefArray<iDocumentNode>* destNodes = 0;
    if (strcmp (location, "variablemap") == 0)
    {
      destNodes = &variableMaps;
    }
    /*else if (strcmp (location, "clips") == 0)
    {
      // Cheat a bit: they go to the same parent node anyway
      destNodes = &variableMaps;
    }*/
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
    else if (strcmp (location, "inputs") == 0)
    {
      destNodes = &currentSnippet.inputs;
    }
    else if (strcmp (location, "definitions") == 0)
    {
      destNodes = &definitions;
    }
    else if (strcmp (location, "version") == 0)
    {
      requiredVersion = csMax (requiredVersion, blockNode->GetContentsValueAsInt());
      return true;
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

  bool ShaderCombinerGLSL::EndSnippet ()
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

  void ShaderCombinerGLSL::AddGlobal (const char* name, const char* type,
                                      const char* annotation)
  {
    if (!globalIDs.Contains (name))
    {
      globalIDs.AddNoTest (name);
      if (annotation)
        AppendComment (globals, annotation);
      const ShaderWeaver::TypeInfo* typeInfo =
        ShaderWeaver::QueryTypeInfo (type);
      if (typeInfo && (typeInfo->baseType == ShaderWeaver::TypeInfo::Sampler))
      {
        globals.Append ("//");
      }
      globals.AppendFmt ("%s %s;\n", typeInfo ? GLSLType (typeInfo).GetData() : type, name);
      globalsTypes.Put (name, typeInfo);
    }
  }

  void ShaderCombinerGLSL::SetOutput (csRenderTargetAttachment target,
                                      const char* name,
                                      const char* annotation)
  {
    csString outputName;
    if(target == rtaDepth)
    {
      outputName = "gl_FragDepth";
    }
    else if(target < rtaNumAttachments)
    {
      outputName.Format("%s[%d]", (requiredVersion >= 130) ? "output_color" : "gl_FragData",
                                  target - rtaColor0);
    }
    else
    {
      CS_ASSERT_MSG ("Unsupported program output target", false);
    }

    outputAssign[target].Empty();
    if (annotation)
      AppendComment (outputAssign[target], annotation);
    outputAssign[target].AppendFmt ("%s = %s;\n", outputName.GetData(), name);
  }

  csPtr<WeaverCommon::iCoerceChainIterator>
  ShaderCombinerGLSL::QueryCoerceChain (const char* fromType, const char* toType)
  {
    return loader->QueryCoerceChain (fromType, toType);
  }

  uint ShaderCombinerGLSL::CoerceCost (const char* fromType, const char* toType)
  {
    return loader->CoerceCost (fromType, toType);
  }

  void ShaderCombinerGLSL::WriteToPass (iDocumentNode* pass)
  {
    csRef<iDocumentNode> shaderNode = pass->CreateNodeBefore (CS_NODE_ELEMENT);
    shaderNode->SetValue ("shader");
    shaderNode->SetAttribute ("plugin", "glsl");

    if (!description.IsEmpty())
    {
      csRef<iDocumentNode> descrNode = shaderNode->CreateNodeBefore (CS_NODE_ELEMENT);
      descrNode->SetValue ("description");
      csRef<iDocumentNode> descrValue = descrNode->CreateNodeBefore (CS_NODE_TEXT);
      descrValue->SetValue (description);
    }

    for (size_t n = 0; n < variableMaps.GetSize(); n++)
    {
      csRef<iDocumentNode> newNode =
        shaderNode->CreateNodeBefore (variableMaps[n]->GetType());
      CS::DocSystem::CloneNode (variableMaps[n], newNode);
    }
    {
      csRef<iDocumentNode> programNode = shaderNode->CreateNodeBefore (CS_NODE_ELEMENT);
      programNode->SetValue ("vp");

      DocNodeAppender appender (programNode);

      if (requiredVersion > 0)
      {
        appender.Append (csString().Format ("#version %d\n", requiredVersion));
      }

      if (definitions.GetSize() > 0)
      {
        appender.Append ("\n");
        appender.Append (definitions);
        appender.Append ("\n");
      }

      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput_V2FDecl (snippets[s], progVP, appender);
      }

      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput (snippets[s].inputs, appender);
      }

      appender.Append ("void main ()\n");
      appender.Append ("{\n");
      appender.Append (globals);

      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        if (!snippets[s].locals.IsEmpty()
          || !snippets[s].inputMaps.IsEmpty()
          || (snippets[s].vertexBody.GetSize() > 0)
          || !snippets[s].outputMaps.IsEmpty())
        {
          if (!snippets[s].annotation.IsEmpty())
          {
            csString comment;
            AppendComment (comment, snippets[s].annotation.GetData());
            appender.Append (comment);
          }
          appender.Append ("{\n");
          if (loader->annotateCombined)
            appender.Append ("// Locally used names for inputs + outputs\n");
          appender.Append (snippets[s].locals);
          AppendProgramInput_V2FLocals (snippets[s], appender);
          AppendSnippetMap (snippets[s].inputMaps, snippets[s].inputMapsTypes, appender);
          AppendSnippetMap (snippets[s].attrInputMaps, snippets[s].attrInputMapsTypes, appender);
          AppendProgramBody (snippets[s].vertexBody, snippets[s], appender);
          appender.Append ("\n");
          AppendProgramInput_V2FVP (snippets[s], appender);
          AppendSnippetMap (snippets[s].outputMaps, snippets[s].outputMapsTypes, appender);
          AppendSnippetMap (snippets[s].attrOutputMaps, snippets[s].attrOutputMapsTypes, appender);
          AppendSnippetMapCleanup (snippets[s].inputMaps, snippets[s].inputMapsTypes, appender);
          AppendSnippetMapCleanup (snippets[s].attrInputMaps, snippets[s].attrInputMapsTypes, appender);
          appender.Append ("}\n");
        }
        appender.Append (snippets[s].links);
        appender.Append ("\n");
      }

      appender.Append ("}\n");
    }

    {
      csRef<iDocumentNode> programNode = shaderNode->CreateNodeBefore (CS_NODE_ELEMENT);
      programNode->SetValue ("fp");

      DocNodeAppender appender (programNode);

      if (requiredVersion > 0)
      {
        appender.Append (csString().Format ("#version %d\n", requiredVersion));
      }

      if (definitions.GetSize() > 0)
      {
        appender.Append ("\n");
        appender.Append (definitions);
        appender.Append ("\n");
      }

      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput_V2FDecl (snippets[s], progFP, appender);
      }

      for (size_t s = 0; s < snippets.GetSize(); s++)
      {
        AppendProgramInput (snippets[s].inputs, appender, progFP);
      }

      if (requiredVersion >= 130)
      {
        appender.AppendFmt ("out vec4 output_color[%d];\n", rtaNumAttachments - rtaColor0);
      }

      appender.Append ("void main ()\n");
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
          {
            csString annotation;
            AppendComment (annotation, snippets[s].annotation.GetData());
            appender.AppendFmt (annotation);
          }
          appender.Append ("{\n");
          if (loader->annotateCombined)
            appender.Append ("// Locally used names for inputs + outputs\n");
          appender.Append (snippets[s].locals);
          AppendProgramInput_V2FLocals (snippets[s], appender);
          AppendSnippetMap (snippets[s].inputMaps, snippets[s].inputMapsTypes, appender);
          AppendSnippetMap (snippets[s].attrInputMaps, snippets[s].attrInputMapsTypes, appender);
          AppendProgramInput_V2FFP (snippets[s], appender);
          AppendProgramBody (snippets[s].fragmentBody, snippets[s], appender);
          appender.Append ("\n");
          AppendSnippetMap (snippets[s].outputMaps, snippets[s].outputMapsTypes, appender);
          AppendSnippetMap (snippets[s].attrOutputMaps, snippets[s].attrOutputMapsTypes, appender);
          AppendSnippetMapCleanup (snippets[s].inputMaps, snippets[s].inputMapsTypes, appender);
          AppendSnippetMapCleanup (snippets[s].attrInputMaps, snippets[s].attrInputMapsTypes, appender);
          appender.Append ("}\n");
        }
        appender.Append (snippets[s].links);
        appender.Append ("\n");
      }

      if (loader->annotateCombined)
        appender.Append ("  // Fragment program output\n");
      for (int a = 0; a < rtaNumAttachments; a++)
        appender.Append (outputAssign[a]);
      appender.Append ("}\n");
    }
  }

  bool ShaderCombinerGLSL::CompatibleParams (iDocumentNode* params)
  {
    return true;
  }

  csRef<iString> ShaderCombinerGLSL::QueryInputTag (const char* location,
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

  void ShaderCombinerGLSL::AppendComment (csString& s, const char* contents)
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

  ShaderCombinerGLSL::Attribute* ShaderCombinerGLSL::FindAttr (AttributeArray& arr,
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

  csString ShaderCombinerGLSL::GLSLType (const char* weaverType)
  {
    const ShaderWeaver::TypeInfo* typeInfo =
      ShaderWeaver::QueryTypeInfo (weaverType);
    csString result;
    if (typeInfo)
    {
      result = GLSLType (typeInfo);
    }
    // @@@ Hmmm... what fallback, if any?
    return result.IsEmpty() ? csString (weaverType) : result;
  }

  void ShaderCombinerGLSL::AppendProgramInput (
    const csRefArray<iDocumentNode>& nodes,
    DocNodeAppender& appender, ProgramType progType)
  {
    // FIXME: error handling here
    for (size_t n = 0; n < nodes.GetSize(); n++)
    {
      iDocumentNode* node = nodes[n];
      AppendProgramInput (node, appender, progType);
    }
  }

  void ShaderCombinerGLSL::AppendProgramInput_V2FDecl (
    const Snippet& snippet, ProgramType progType, DocNodeAppender& appender)
  {
    // FIXME: error handling here
    const char* declarator;
    if (requiredVersion >= 130)
      declarator = (progType == progFP) ? "in" : "out";
    else
      declarator = "varying";
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
            const char* type = node->GetAttributeValue ("type");
            if (type && *type)
            {
              csString str;
              str.Format ("%s %s _v2f_%s_[%d];\n",
                declarator,
                GLSLType (type).GetData(), uniqueName.GetData(),
                count);
              appender.Append (str);
            }
          }
          else
          {
            const char* type = node->GetAttributeValue ("type");
            if (type && *type)
            {
              csString str;
              str.Format ("%s %s _v2f_%s_;\n",
                declarator,
                GLSLType (type).GetData(), uniqueName.GetData());
              appender.Append (str);
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

  void ShaderCombinerGLSL::AppendProgramInput_V2FLocals (
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
                GLSLType (type).GetData(), name.GetData(), count);
            }
            else
            {
              appender.AppendFmt ("%s %s;\n",
                GLSLType (type).GetData(), name.GetData());
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

  void ShaderCombinerGLSL::AppendProgramInput_V2FVP (
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
            for (int i = 0; i < count; i++)
            {
              appender.AppendFmt ("_v2f_%s_[%d] = %s[%d];\n",
                uniqueName.GetData(), i, name.GetData(), i);
            }
          }
          else
          {
            appender.AppendFmt ("_v2f_%s_ = %s;\n",
              uniqueName.GetData(), name.GetData());
          }
        }
      }
      else
      {
        AppendProgramInput (node, appender);
      }
    }
  }

  void ShaderCombinerGLSL::AppendProgramInput_V2FFP (
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
              appender.AppendFmt ("%s[%d] = _v2f_%s_[%d];\n",
                name.GetData(), i, uniqueName.GetData(), i);
            }
          }
          else
          {
            appender.AppendFmt ("%s = _v2f_%s_;\n",
              name.GetData(), uniqueName.GetData());
          }
        }
      }
      else
      {
        AppendProgramInput (node, appender);
      }
    }
  }

  void ShaderCombinerGLSL::AppendProgramInput (iDocumentNode* node,
    DocNodeAppender& appender, ProgramType progType)
  {
    if (node->GetType() == CS_NODE_ELEMENT)
    {
      csStringID id = loader->xmltokens_common.Request (node->GetValue());
      switch (id)
      {
      case ShaderCombinerLoaderCommon::XMLTOKEN_VARYING:
        if (progType == progFP)
          // Don't emit attribute inputs to FPs
          break;
      case ShaderCombinerLoaderCommon::XMLTOKEN_UNIFORM:
        {
          const char* name = node->GetAttributeValue ("name");
          const char* type = node->GetAttributeValue ("type");
          const ShaderWeaver::TypeInfo* typeInfo =
            ShaderWeaver::QueryTypeInfo (type);
          if (name && *name && type && *type)
          {
            const char* declarator;
            if (id == ShaderCombinerLoaderCommon::XMLTOKEN_UNIFORM)
              declarator = "uniform";
            else if (requiredVersion >= 130)
              declarator = "in";
            else
              declarator = "attribute";
            csString str;
            str.Format ("%s %s %s;\n", declarator,
              typeInfo ? GLSLType (typeInfo).GetData() : type,
              name);
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

  void ShaderCombinerGLSL::AppendSnippetMap (const csHash<csString, csString>& map,
                                             const csHash<const ShaderWeaver::TypeInfo*, csString>& fromTypeMap,
                                             DocNodeAppender& appender)
  {
    csHash<csString, csString>::ConstGlobalIterator it = map.GetIterator ();
    while (it.HasNext())
    {
      csString fromName;
      const csString& toName = it.Next (fromName);
      const ShaderWeaver::TypeInfo* fromType (fromTypeMap.Get (fromName, nullptr));
      if (fromName != toName)
      {
        if (fromType && (fromType->baseType == ShaderWeaver::TypeInfo::Sampler))
        {
          appender.AppendFmt ("#define %s %s\n", toName.GetData(), fromName.GetData());
        }
        else
          appender.AppendFmt ("%s = %s;\n", toName.GetData(), fromName.GetData());
      }
    }
  }

  void ShaderCombinerGLSL::AppendSnippetMapCleanup (const csHash<csString, csString>& map,
                                                    const csHash<const ShaderWeaver::TypeInfo*, csString>& fromTypeMap,
                                                    DocNodeAppender& appender)
  {
    csHash<csString, csString>::ConstGlobalIterator it = map.GetIterator ();
    while (it.HasNext())
    {
      csString fromName;
      const csString& toName = it.Next (fromName);
      const ShaderWeaver::TypeInfo* fromType (fromTypeMap.Get (fromName, nullptr));
      if (fromName != toName)
      {
        if (fromType && (fromType->baseType == ShaderWeaver::TypeInfo::Sampler))
          appender.AppendFmt ("#undef %s\n", toName.GetData());
      }
    }
  }
  
  void ShaderCombinerGLSL::AppendProgramBody (const csRefArray<iDocumentNode>& nodes,
                                              Snippet& snippet,
                                              DocNodeAppender& appender)
  {
    for (size_t n = 0; n < nodes.GetSize(); n++)
    {
      iDocumentNode* node = nodes[n];
      if (node->GetType() == CS_NODE_TEXT)
      {
        appender.Append (ProcessSamplerAssigns (node->GetValue(), snippet));
      }
      else
        appender.Append (node);
    }
  }
  
  csString ShaderCombinerGLSL::ProcessSamplerAssigns (const csString& originalText,
                                                      Snippet& snippet)
  {
    csString output;
    output.SetCapacity (originalText.Length());
    
    csStringReader input (originalText);
    csString line;
    while (input.GetLine (line))
    {
      bool line_handled (false);
      size_t pos (0);
      // Skip whitespace. \r and \n are removed by csStringReader
      while ((pos < line.Length()) && ((line[pos] == ' ') || (line[pos] == '\t'))) pos++;
      
      if ((pos < line.Length()) && (line[pos] == '#'))
      {
        // Might be a directive...
        pos++;
        while ((pos < line.Length()) && ((line[pos] == ' ') || (line[pos] == '\t'))) pos++;
        CS::Utility::StringArray<> directive_tokens;
        directive_tokens.SplitString (line.Slice (pos), " \t",
                                      CS::Utility::StringArray<>::delimIgnore);
        if (directive_tokens.GetSize() > 0)
        {
          if (strcmp (directive_tokens[0], "sampler_assign") == 0)
          {
            line_handled = true;
            
            if (directive_tokens.GetSize() != 3)
            {
              output.Append("#error Wrong argument count for #sampler_assign\n");
            }
            else
            {
              // Switch the mapping in outputmaps to use the 'assigned' sampler
              const csString& fromName (directive_tokens[1]);
              csHash<csString, csString>::GlobalIterator outputMapsIt (snippet.outputMaps.GetIterator());
              while (outputMapsIt.HasNext())
              {
                csString mapFrom;
                csString mapTo (outputMapsIt.NextNoAdvance (mapFrom));
                if (mapFrom.Compare (fromName))
                {
                  snippet.outputMapsTypes.PutUnique (directive_tokens[2],
                                                     snippet.outputMapsTypes.Get (fromName, nullptr));
                  snippet.outputMapsTypes.DeleteAll (fromName);
                  snippet.outputMaps.DeleteElement (outputMapsIt);
                  snippet.outputMaps.PutUnique (directive_tokens[2], mapTo);
                  break;
                }
                outputMapsIt.Advance();
              }
            }
          }
        }
      }
      
      if (!line_handled)
      {
        output.Append (line);
        output.Append ("\n");
      }
    }
    
    return output;
  }

  csString ShaderCombinerGLSL::GLSLType (const ShaderWeaver::TypeInfo* typeInfo)
  {
    switch (typeInfo->baseType)
    {
      case ShaderWeaver::TypeInfo::Vector:
      case ShaderWeaver::TypeInfo::VectorB:
      case ShaderWeaver::TypeInfo::VectorI:
    {
      if (typeInfo->dimensions == 1)
      {
        static const char* const baseTypeStrs[] =
        { "float", "bool", "int" };
        return baseTypeStrs[typeInfo->baseType -
          ShaderWeaver::TypeInfo::Vector];;
      }
      else
      {
        static const char* const baseTypeStrs[] =
        { "vec", "bvec", "ivec" };
        const char* baseTypeStr = baseTypeStrs[typeInfo->baseType -
          ShaderWeaver::TypeInfo::Vector];
        return csString().Format ("%s%d", baseTypeStr, typeInfo->dimensions);
      }
    }
      case ShaderWeaver::TypeInfo::Matrix:
      case ShaderWeaver::TypeInfo::MatrixB:
      case ShaderWeaver::TypeInfo::MatrixI:
        {
          if (typeInfo->baseType == ShaderWeaver::TypeInfo::Matrix)
          {
            if(typeInfo->dimensions == typeInfo->dimensions2)
              return csString().Format ("mat%d", typeInfo->dimensions);
            else
            {
              // @@@ NOTE: only requires 100 for GLSL ES - maybe we should keep track of both minimas?
              // @@@ FIXME: can't set version here as it's already too late, it was already outputted
              //requiredVersion = csMax(requiredVersion, 120);
              return csString().Format ("mat%dx%d", typeInfo->dimensions2, typeInfo->dimensions);
            }
          }
          /* @@@ FIXME: Support:
           * - non-float matrices (not supported as of GLSL 420)
           */
          break;
        }
      case ShaderWeaver::TypeInfo::Sampler:
    if (typeInfo->samplerIsCube)
      return "samplerCube";
    else
      return csString().Format ("sampler%dD", typeInfo->dimensions);
    }
    return csString();
  }

  csString ShaderCombinerGLSL::GetAttrIdentifier (const char* var,
                                                  const char* attr)
  {
    csString s;
    s.Format ("%s_attr_%s", var, attr);
    return s;
  }

}
CS_PLUGIN_NAMESPACE_END(SLCombiner)
