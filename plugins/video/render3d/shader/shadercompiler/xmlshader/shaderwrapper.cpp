/*
  Copyright (C) 2003-2006 by Marten Svanfeldt
		2004-2012 by Frank Richter

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

#include "shaderwrapper.h"

#include "iutil/stringarray.h"
#include "csutil/scfstringarray.h"

#include "shadertech.h"

CS_PLUGIN_NAMESPACE_BEGIN(XMLShader)
{

  static const char wrapperProgType[] = "*wrapper";

CS_LEAKGUARD_IMPLEMENT (csXMLShaderWrapper);

csXMLShaderPluginWrapper::csXMLShaderPluginWrapper (iDocumentNode *node, csXMLShaderTech* tech, size_t variant)
 : scfImplementationType (this), tech (tech), variant (variant)
{
  FillCacheInfo (node, cacheInfoVP, cacheInfoFP);
}

bool csXMLShaderPluginWrapper::FillCacheInfo (iDocumentNode *node,
                                              csXMLShaderTech::CachedPlugin& cacheInfoVP,
                                              csXMLShaderTech::CachedPlugin& cacheInfoFP)
{
  {
    csRef<iDocumentNode> programNodeFP;
    programNodeFP = node->GetNode (tech->xmltokens.Request (
      csXMLShaderCompiler::XMLTOKEN_FP));

    if (programNodeFP)
      if (!tech->GetProgramPlugin (programNodeFP, cacheInfoFP, variant))
        return false;
  }

  {
    csRef<iDocumentNode> programNodeVP;
  programNodeVP = node->GetNode(tech->xmltokens.Request (
      csXMLShaderCompiler::XMLTOKEN_VP));

    if (programNodeVP)
      if (!tech->GetProgramPlugin (programNodeVP, cacheInfoVP, variant))
        return false;
  }

  return true;
}

csPtr<iShaderProgram> csXMLShaderPluginWrapper::CreateProgram (const char* type)
{
  if (!type || (strcmp (type, wrapperProgType) != 0))
    return (iShaderProgram*)nullptr;

  csRef<iShaderProgram> prog;
  prog.AttachNew (new csXMLShaderWrapper (this));
  return csPtr<iShaderProgram> (prog);
}

bool csXMLShaderPluginWrapper::SupportType (const char* type)
{
  return type && strcmp (type, wrapperProgType) == 0;
}

csPtr<iStringArray> csXMLShaderPluginWrapper::QueryPrecacheTags (const char* type)
{
  if (!type || (strcmp (type, wrapperProgType) != 0))
    return (iStringArray*)nullptr;

  csArray<CS::Utility::StringArray<> > tuples;
  tuples.Push (CS::Utility::StringArray<> ());

  for (size_t i = 0; i < 2; i++)
  {
    csXMLShaderTech::CachedPlugin plugin;
    switch (i)
    {
      case 0: plugin = cacheInfoFP; break;
      case 1: plugin = cacheInfoVP; break;
    }
    if (plugin.programNode == 0)
    {
      for (size_t j = 0; j < tuples.GetSize(); j++)
      {
        tuples[j].Push ("");
      }
      continue;
    }

    csArray<CS::Utility::StringArray<> > newTuples;

    csRef<iStringArray> tags = plugin.programPlugin->QueryPrecacheTags (
      plugin.progType);
    for (size_t j = 0; j < tuples.GetSize(); j++)
    {
      CS::Utility::StringArray<> tupleStrs (tuples[j]);
      for (size_t t = 0; t < tags->GetSize(); t++)
      {
        CS::Utility::StringArray<> newTupleStrs (tupleStrs);
        newTupleStrs.Push (tags->Get (t));
        newTuples.Push (newTupleStrs);
      }
    }
    tuples = newTuples;
  }

  csRef<scfStringArray> tuple_strs;
  tuple_strs.AttachNew (new scfStringArray);
  for (size_t i = 0; i < tuples.GetSize(); i++)
  {
    tuple_strs->Push (tuples[i].Join ("_"));
  }
  return csPtr<iStringArray> (tuple_strs);
}

class PrecacheOutputWrapper :
  public scfImplementation1<PrecacheOutputWrapper,
                            scfFakeInterface<iShaderDestinationResolver> >,
  public WrappedShaderDestinationResolver<PrecacheOutputWrapper>
{
  csRef<iBase> fp;
  csRef<iBase> vp;
public:
  PrecacheOutputWrapper (iBase* fp, iBase* vp)
    : scfImplementationType (this), fp (fp), vp (vp) {}

  iBase* GetVP() { return vp; }
  iBase* GetFP() { return fp; }
};


bool csXMLShaderPluginWrapper::Precache (const char* type, const char* tag,
                                         iBase* previous, iDocumentNode* node,
                                         iHierarchicalCache* cacheTo, csRef<iBase>* outObj)
{
  if (!type || (strcmp (type, wrapperProgType) != 0))
    return false;

  CS::Utility::StringArray<> tag_strs;
  tag_strs.SplitString (tag, "_");

  csRef<iBase> fp_precached;
  if (tag_strs.GetSize() >= 1)
  {
    csRef<iHierarchicalCache> fpCache;
    if (cacheTo) fpCache = cacheTo->GetRootedCache ("/fp");
    if (!cacheInfoFP.programPlugin->Precache (cacheInfoFP.progType, tag_strs[0],
                                              nullptr, cacheInfoFP.programNode,
                                              fpCache, &fp_precached))
      return false;
  }
  csRef<iBase> vp_precached;
  if (tag_strs.GetSize() >= 2)
  {
    csRef<iHierarchicalCache> vpCache;
    if (cacheTo) vpCache = cacheTo->GetRootedCache ("/vp");
    if (cacheInfoVP.programPlugin->Precache (cacheInfoVP.progType, tag_strs[1],
                                             fp_precached, cacheInfoVP.programNode,
                                             vpCache, &vp_precached))
      return false;
  }

  if (outObj)
  {
    outObj->AttachNew (new PrecacheOutputWrapper (fp_precached, vp_precached));
  }

  return true;
}

//---------------------------------------------------------------------------

CS_LEAKGUARD_IMPLEMENT (csXMLShaderWrapper);

bool csXMLShaderWrapper::FillCachedPlugin (csXMLShaderTech* tech,
                                           iDocumentNode *node,
                                           csXMLShaderTech::CachedPlugin& cacheInfo,
                                           size_t variant)
{
  csRef<csXMLShaderPluginWrapper> wrapper;
  wrapper.AttachNew (new csXMLShaderPluginWrapper (node, tech, variant));

  cacheInfo.programPlugin = wrapper;
  cacheInfo.progType = wrapperProgType;
  cacheInfo.programNode = node;
  cacheInfo.available = true;

  return true;
}


csXMLShaderWrapper::csXMLShaderWrapper (csXMLShaderPluginWrapper* loadingWrapper) :
  scfImplementationType (this), loadingWrapper (loadingWrapper)
{
}

csXMLShaderWrapper::~csXMLShaderWrapper ()
{
}

void csXMLShaderWrapper::Activate ()
{
  if (vp) vp->Activate ();
  if (fp) fp->Activate ();
}

void csXMLShaderWrapper::Deactivate ()
{
  if (fp) fp->Deactivate ();
  if (vp) vp->Deactivate ();
}

bool csXMLShaderWrapper::Compile (iHierarchicalCache *cacheTo,
                                  csRef<iString> *cacheTag)
{
  csString tagFP, tagVP;

  if (fp)
  {
    csRef<iHierarchicalCache> fpCache;
    if (cacheTo) fpCache = cacheTo->GetRootedCache ("/fp");

    csRef<iString> cacheTagFP;
    if (!fp->Compile (fpCache, &cacheTagFP))
      return false;
    if (cacheTagFP) tagFP = cacheTagFP->GetData();
  }
  if (vp)
  {
    csRef<iHierarchicalCache> vpCache;
    if (cacheTo) vpCache = cacheTo->GetRootedCache ("/vp");

    csRef<iString> cacheTagVP;
    if (!vp->Compile (vpCache, &cacheTagVP))
      return false;
    if (cacheTagVP) tagVP = cacheTagVP->GetData();
  }

  if (cacheTag)
    cacheTag->AttachNew (new scfString (csString().Format ("%s;%s", tagVP.GetDataSafe(), tagFP.GetDataSafe())));

  return true;
}

void csXMLShaderWrapper::GetUsedShaderVars (csBitArray& bits) const
{
  if (vp) vp->GetUsedShaderVars (bits);
  if (fp) fp->GetUsedShaderVars (bits);
}

bool csXMLShaderWrapper::Load (iShaderDestinationResolver *resolve,
                               const char *program,
                               const csArray<csShaderVarMapping> &mappings)
{
  // Loading from source code is not supported
  return false;
}

bool csXMLShaderWrapper::Load (iShaderDestinationResolver *resolve,
                               iDocumentNode *node)
{
  if (!loadingWrapper) return false;

  csXMLShaderTech::CachedPlugin& cacheInfoFP (loadingWrapper->cacheInfoFP);
  csXMLShaderTech::CachedPlugin& cacheInfoVP (loadingWrapper->cacheInfoVP);

  csRef<iShaderDestinationResolver> resolveFP;

  // load fp
  /* This is done before VP loading b/c the VP could query for TU bindings
    * which are currently handled by the FP. */
  if (cacheInfoFP.programNode)
  {
    csRef<iShaderProgram> program;

    program = cacheInfoFP.programPlugin->CreateProgram (cacheInfoFP.progType);
    if (program == 0)
      return false;
    if (!program->Load (nullptr, cacheInfoFP.programNode))
      return false;

    fp = program;
    resolveFP = scfQueryInterfaceSafe<iShaderDestinationResolver> (fp);
  }

  if (cacheInfoVP.programNode)
  {
    csRef<iShaderProgram> program;

    program = cacheInfoVP.programPlugin->CreateProgram (cacheInfoVP.progType);
    if (program == 0)
      return false;
    if (!program->Load (resolveFP, cacheInfoVP.programNode))
      return false;

    vp = program;
  }

  return true;
}

iShaderProgram::CacheLoadResult csXMLShaderWrapper::LoadProgram (iHierarchicalCache *cache, iBase *previous,
                                                                 const csXMLShaderTech::CachedPlugin& cacheInfo,
                                                                 csRef<iString> *failReason,
                                                                 csRef<iShaderProgram>& prog,
                                                                 csString& tag)
{
  prog = cacheInfo.programPlugin->CreateProgram (cacheInfo.progType);
  csRef<iString> progTag;
  csRef<iString> progFailReason;
  iShaderProgram::CacheLoadResult loadRes = prog->LoadFromCache (cache,
    previous, cacheInfo.programNode, &progFailReason, &progTag);
  if (loadRes == iShaderProgram::loadFail)
  {
    if (failReason)
    {
      const char* reasonStr (progFailReason.IsValid() ? progFailReason->GetData() : "no reason given");
      (*failReason).AttachNew (
        new scfString (csString().Format ("error reading %s: %s",
                                          cacheInfo.progType.GetData(), reasonStr)));
    }
    return iShaderProgram::loadFail;
  }

  if (progTag.IsValid()) tag = progTag->GetData();

  return loadRes;
}

iShaderProgram::CacheLoadResult csXMLShaderWrapper::LoadFromCache (
  iHierarchicalCache *cache, iBase *previous, iDocumentNode *programNode,
  csRef<iString> *failReason, csRef<iString> *cacheTag)
{
  if (!loadingWrapper) return iShaderProgram::loadFail;

  csXMLShaderTech::CachedPlugin& cacheInfoFP (loadingWrapper->cacheInfoFP);
  csXMLShaderTech::CachedPlugin& cacheInfoVP (loadingWrapper->cacheInfoVP);

  iShaderProgram::CacheLoadResult loadRes;
  csString tagFP, tagVP;

  // load shaders
  if (cacheInfoFP.available)
  {
    csRef<iHierarchicalCache> fpCache;
    if (cache) fpCache = cache->GetRootedCache ("/fp");

    loadRes = LoadProgram (fpCache, nullptr, cacheInfoFP, failReason, fp, tagFP);
    if (loadRes != iShaderProgram::loadSuccessShaderValid) return loadRes;
  }
  if (cacheInfoVP.available)
  {
    csRef<iHierarchicalCache> vpCache;
    if (cache) vpCache = cache->GetRootedCache ("/vp");
    loadRes = LoadProgram (vpCache, (iShaderProgram*)fp, cacheInfoVP, failReason, vp, tagVP);
    if (loadRes != iShaderProgram::loadSuccessShaderValid) return loadRes;
  }

  if (cacheTag)
  {
    cacheTag->AttachNew (new scfString (csString().Format ("%s;%s", tagVP.GetDataSafe(), tagFP.GetDataSafe())));
  }

  return iShaderProgram::loadSuccessShaderValid; // hm.
}

void csXMLShaderWrapper::ResetState ()
{
  if (vp) vp->ResetState ();
  if (fp) fp->ResetState ();
}

void csXMLShaderWrapper::SetupState (const CS::Graphics::RenderMesh *mesh,
                                     CS::Graphics::RenderMeshModes &modes,
                                     const csShaderVariableStack &stack)
{
  if (vp) vp->SetupState (mesh, modes, stack);
  if (fp) fp->SetupState (mesh, modes, stack);
}

}
CS_PLUGIN_NAMESPACE_END(XMLShader)
