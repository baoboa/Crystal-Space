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

#include "cssysdef.h"

#include "shadersets.h"

#include "cstool/vfsdirchange.h"
#include "csutil/stringquote.h"
#include "imap/services.h"
#include "iutil/vfs.h"

#include "csthreadedloader.h"

CS_PLUGIN_NAMESPACE_BEGIN(csparser)
{
  ShaderSets::ShaderSets (csThreadedLoader* parent) : parent (parent)
  {
  }

  bool ShaderSets::ParseShaderSets (const char* vfsPath)
  {
    csRef<iFile> setsFile = parent->vfs->Open (vfsPath, VFS_FILE_READ);
    if (!setsFile)
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             "Could not open shadersets file %s",
                             CS::Quote::Double (vfsPath));
      return false;
    }

    csRef<iDocument> setsDoc;
    if (!parent->LoadStructuredDoc (vfsPath, setsFile, setsDoc))
      return false;

    return ParseShaderSets (setsDoc, vfsPath);
  }

  bool ShaderSets::ParseShaderSets (iDocument* doc, const char* filename)
  {
    csRef<iDocumentNode> docRoot = doc->GetRoot();
    csRef<iDocumentNode> setsNode = docRoot->GetNode ("shadersets");
    if (!setsNode)
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             "%s is not a shadersets file",
                             CS::Quote::Double (filename));
      return false;
    }

    return ParseShaderSets (setsNode);
  }

  bool ShaderSets::ApplyShaderSet (iLoaderContext* ldr_context, const char* setName,
                                   MaterialShaderMappingsArray& mappings)
  {
    ParsedShaderSet* set (shaderSets.GetElementPointer (setName));
    if (!set)
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             "Shader set %s does note exist",
                             CS::Quote::Double (setName));
      return false;
    }
    for (size_t i = 0; i < set->GetSize(); i++)
    {
      const ParsedShaderSetEntry& setEntry (set->Get (i));
      csRef<iShader> shader;
      // Try to retrieve shader from context
      if (ldr_context) shader = ldr_context->FindShader (setEntry.shaderName);
      // Try to retrieve shader from XML <shaders>
      if (!shader)
      {
        ShaderInfo* shaderInfo (set_shaders.GetElementPointer (setEntry.shaderName));
        if (shaderInfo)
        {
          shader = ShaderFromInfo (ldr_context, *shaderInfo);
        }
      }
      if (shader)
      {
        MaterialShaderMapping mapping;
        mapping.shaderType = setEntry.shaderType;
        mapping.shader = shader;
        mappings.Push (mapping);
      }
    }
    return true;
  }

  bool ShaderSets::ParseShaderSets (iDocumentNode* shadersetsNode)
  {
    csRef<iDocumentNodeIterator> it = shadersetsNode->GetNodes ();
    while (it->HasNext ())
    {
      csRef<iDocumentNode> child = it->Next ();
      if (child->GetType () != CS_NODE_ELEMENT) continue;
      const char* value = child->GetValue ();
      csStringID id = parent->xmltokens.Request (value);
      switch (id)
      {
      case csThreadedLoader::XMLTOKEN_SHADERS:
        if (!ParseShaders (child))
          return false;
        break;
      case csThreadedLoader::XMLTOKEN_SHADERSET:
        if (!ParseShaderSet (child))
          return false;
        break;
      default:
        {
          parent->SyntaxService->ReportBadToken (child);
          return false;
        }
      }
    }
    return true;
  }

  bool ShaderSets::ParseShaderSet (iDocumentNode* shadersetNode)
  {
    const char* setName = shadersetNode->GetAttributeValue ("name");
    if (!setName || !*setName)
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             shadersetNode,
                             "Shader set without name");
      return false;
    }

    if (shaderSets.Contains (setName))
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             shadersetNode,
                             "Shader set %s already specified",
                             CS::Quote::Double (setName));
      return false;
    }

    ParsedShaderSet shaderSet;
    csRef<iDocumentNodeIterator> it = shadersetNode->GetNodes ();
    while (it->HasNext ())
    {
      csRef<iDocumentNode> child = it->Next ();
      if (child->GetType () != CS_NODE_ELEMENT) continue;
      const char* value = child->GetValue ();
      csStringID id = parent->xmltokens.Request (value);
      switch (id)
      {
      case csThreadedLoader::XMLTOKEN_SHADER:
        {
          const char* shadername = child->GetContentsValue ();
          if (!shadername || !*shadername)
          {
            parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                                   child,
                                   "No shader name given in shader set %s",
                                   CS::Quote::Double (setName));
            return false;
          }
          const char* shadertype = child->GetAttributeValue ("type");
          if (!shadertype)
          {
            parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                                   child,
                                   "No shadertype for shader %s in shader set %s",
                                   CS::Quote::Double (shadername),
                                   CS::Quote::Double (setName));
            return false;
          }
          ParsedShaderSetEntry setEntry;
          setEntry.shaderType = parent->stringSet->Request (shadertype);
          setEntry.shaderName = shadername;
          shaderSet.Push (setEntry);
        }
        break;
      default:
        {
          parent->SyntaxService->ReportBadToken (child);
          return false;
        }
      }
    }
    shaderSets.PutUnique (setName, shaderSet);
    return true;
  }

  bool ShaderSets::ParseShaders (iDocumentNode* shadersNode)
  {
    csRef<iDocumentNodeIterator> itr = shadersNode->GetNodes("shader");
    while(itr->HasNext())
    {
      csRef<iDocumentNode> shaderNode (itr->Next());
      if (!ParseShader (shaderNode))
        return false;
    }
    return true;
  }

  bool ShaderSets::ParseShader (iDocumentNode* shaderNode)
  {
    ShaderInfo shaderInfo;
    shaderInfo.node = parent->GetShaderDataNode (shaderNode, shaderInfo.filename);
    if (!shaderInfo.node) return false;

    const char* shaderName = shaderInfo.node->GetAttributeValue ("name");
    if (!shaderName || !*shaderName)
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             shaderNode,
                             "Shader without name");
      return false;
    }

    if (set_shaders.Contains (shaderName))
    {
      parent->ReportWarning ("crystalspace.level.threadedloader.shadersets",
                             shaderNode,
                             "Shader %s already specified",
                             CS::Quote::Double (shaderName));
      return false;
    }
    set_shaders.PutUnique (shaderName, shaderInfo);
    return true;
  }

  csPtr<iShader> ShaderSets::ShaderFromInfo (iLoaderContext* ldr_context,
                                             const ShaderInfo& info)
  {
    csVfsDirectoryChanger dirChanger (parent->vfs);

    if (!info.filename.IsEmpty())
      dirChanger.ChangeTo (info.filename);

    return parent->SyntaxService->ParseShader (ldr_context, info.node);
  }
}
CS_PLUGIN_NAMESPACE_END(csparser)
