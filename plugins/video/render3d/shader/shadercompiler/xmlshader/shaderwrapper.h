/*
  Copyright (C) 2003-2006 by Marten Svanfeldt
		2005-2012 by Frank Richter

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

#ifndef __CS_SHADERWRAPPER_H__
#define __CS_SHADERWRAPPER_H__

#include "cssysdef.h"

#include "csutil/objreg.h"
#include "csutil/ref.h"
#include "csutil/scf.h"
#include "csutil/scfstr.h"
#include "csutil/stringreader.h"
#include "iutil/document.h"
#include "iutil/hiercache.h"
#include "iutil/string.h"
#include "ivideo/shader/shader.h"

#include "shader.h"
#include "xmlshader.h"

CS_PLUGIN_NAMESPACE_BEGIN(XMLShader)
{

class csXMLShaderTech;
class csXMLShaderWrapper;

class csXMLShaderPluginWrapper :
  public scfImplementation1<csXMLShaderPluginWrapper,
                            iShaderProgramPlugin>
{
  csXMLShaderTech* tech;
  size_t variant;

  bool FillCacheInfo (iDocumentNode *node,
                      csXMLShaderTech::CachedPlugin& cacheInfoVP,
                      csXMLShaderTech::CachedPlugin& cacheInfoFP);

private:
  friend class csXMLShaderWrapper;

  csXMLShaderTech::CachedPlugin cacheInfoFP;
  csXMLShaderTech::CachedPlugin cacheInfoVP;
public:
  CS_LEAKGUARD_DECLARE (csXMLShaderPluginWrapper);

  csXMLShaderPluginWrapper (iDocumentNode *node, csXMLShaderTech* tech, size_t variant);

  virtual csPtr<iShaderProgram> CreateProgram (const char* type);
  virtual bool SupportType (const char* type);

  virtual csPtr<iStringArray> QueryPrecacheTags (const char* type);
  /**
   * Warm the given cache with the program specified in \a node.
   * \a outObj can return an object which exposes iShaderDestinationResolver.
   */
  virtual bool Precache (const char* type, const char* tag,
    iBase* previous, iDocumentNode* node,
    iHierarchicalCache* cacheTo, csRef<iBase>* outObj = 0);
};

template<typename Impl>
class WrappedShaderDestinationResolver : public virtual iShaderDestinationResolver
{
public:
  int ResolveTU (const char* binding)
  {
    int tu (-1);

    csRef<iShaderDestinationResolver> resolveFP (
      scfQueryInterfaceSafe<iShaderDestinationResolver> (static_cast<Impl*> (this)->GetFP()));

    // Give FP precedence
    if (resolveFP)
      tu = resolveFP->ResolveTU (binding);

    csRef<iShaderDestinationResolver> resolveVP (
      scfQueryInterfaceSafe<iShaderDestinationResolver> (static_cast<Impl*> (this)->GetVP()));
    // Fall back to VP for TU resolution
    if ((tu < 0) && resolveVP)
      tu = resolveVP->ResolveTU (binding);

    return tu;
  }

  csVertexAttrib ResolveBufferDestination (const char* binding)
  {
    csRef<iShaderDestinationResolver> resolveVP (
      scfQueryInterfaceSafe<iShaderDestinationResolver> (static_cast<Impl*> (this)->GetVP()));
    if (resolveVP)
      return resolveVP->ResolveBufferDestination (binding);
    // FPs usually don't have buffer destinations, do they?
    return CS_VATTRIB_INVALID;
  }
};

class csXMLShaderWrapper :
  public scfImplementation2<csXMLShaderWrapper,
                            iShaderProgram,
                            scfFakeInterface<iShaderDestinationResolver> >,
  public WrappedShaderDestinationResolver<csXMLShaderWrapper>
{
private:
  csRef<csXMLShaderPluginWrapper> loadingWrapper;

  // wrapped programs
  csRef<iShaderProgram> vp;
  csRef<iShaderProgram> fp;

  iShaderProgram::CacheLoadResult LoadProgram (iHierarchicalCache *cache, iBase *previous,
                                               const csXMLShaderTech::CachedPlugin& cacheInfo,
                                               csRef<iString> *failReason,
                                               csRef<iShaderProgram>& prog,
                                               csString& tag);
public:
  CS_LEAKGUARD_DECLARE (csXMLShaderWrapper);

  static bool FillCachedPlugin (csXMLShaderTech* tech,
                                iDocumentNode *node,
                                csXMLShaderTech::CachedPlugin& cacheInfo,
                                size_t variant);

  csXMLShaderWrapper (csXMLShaderPluginWrapper* loadingWrapper);
  ~csXMLShaderWrapper ();

  virtual void Activate ();
  virtual void Deactivate ();

  virtual bool Compile (iHierarchicalCache *cacheTo,
                        csRef<iString> *cacheTag = 0);

  virtual void GetUsedShaderVars (csBitArray& bits) const;

  virtual bool Load (iShaderDestinationResolver *resolve,
                     const char *program,
                     const csArray<csShaderVarMapping> &mappings);
  virtual bool Load (iShaderDestinationResolver *resolve,
                     iDocumentNode *node);

  virtual iShaderProgram::CacheLoadResult LoadFromCache (
      iHierarchicalCache *cache, iBase *previous, iDocumentNode *programNode,
      csRef<iString> *failReason = 0, csRef<iString> *cacheTag = 0);

  virtual void ResetState ();

  virtual void SetupState (const CS::Graphics::RenderMesh *mesh,
                           CS::Graphics::RenderMeshModes &modes,
                           const csShaderVariableStack &stack);

  // wrapper specific functions: set vertex/fragment shaders
  iShaderProgram* GetVP() { return vp; }
  iShaderProgram* GetFP() { return fp; }
};

}
CS_PLUGIN_NAMESPACE_END(XMLShader)

#endif // __CS_SHADERWRAPPER_H__
