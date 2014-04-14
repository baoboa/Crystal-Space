/*
    Copyright (C) 2010 by Joe Forte

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

#ifndef __DEFERRED_H__
#define __DEFERRED_H__

#include "cssysdef.h"

#include "csplugincommon/rendermanager/standardtreetraits.h"
#include "csplugincommon/rendermanager/dependenttarget.h"
#include "csplugincommon/rendermanager/rendertree.h"
#include "csplugincommon/rendermanager/debugcommon.h"
#include "csplugincommon/rendermanager/renderlayers.h"
#include "csplugincommon/rendermanager/autofx_framebuffertex.h"
#include "csplugincommon/rendermanager/autofx_reflrefr.h"
#include "csplugincommon/rendermanager/shadow_pssm.h"
#include "csplugincommon/rendermanager/posteffectssupport.h"
#include "csplugincommon/rendermanager/hdrexposure.h"
#include "csplugincommon/rendermanager/viscullcommon.h"

#include "iutil/comp.h"
#include "csutil/scf_implementation.h"
#include "iengine/rendermanager.h"
#include "itexture.h"

#include "deferredtreetraits.h"

CS_PLUGIN_NAMESPACE_BEGIN(RMDeferred)
{
  typedef CS::RenderManager::RenderTree<RenderTreeDeferredTraits> 
    RenderTreeType;

  template<typename RenderTreeType, typename LayerConfigType>
  class StandardContextSetup;

  class RMDeferred : public scfImplementation6<RMDeferred, 
                                               iRenderManager,
                                               iRenderManagerTargets,
                                               scfFakeInterface<iRenderManagerVisCull>,
                                               iComponent,
                                               scfFakeInterface<iRenderManagerPostEffects>,
                                               scfFakeInterface<iDebugHelper> >,
                     public CS::RenderManager::RMDebugCommon<RenderTreeType>,
		     public CS::RenderManager::PostEffectsSupport,
		     public CS::RenderManager::RMViscullCommon
  {
  public:

    /// Constructor.
    RMDeferred(iBase *parent);

    //---- iComponent Interface ----
    virtual bool Initialize(iObjectRegistry *registry);

    //---- iRenderManager Interface ----
    virtual bool RenderView(iView *view);
    virtual bool PrecacheView(iView *view);

    //---- iRenderManagerTargets Interface ----
    virtual void RegisterRenderTarget (iTextureHandle* target, 
      iView* view, int subtexture = 0, uint flags = 0)
    {
      targets.RegisterRenderTarget (target, view, subtexture, flags);
    }
    virtual void UnregisterRenderTarget (iTextureHandle* target,
      int subtexture = 0)
    {
      targets.UnregisterRenderTarget (target, subtexture);
    }
    virtual void MarkAsUsed (iTextureHandle* target)
    {
      targets.MarkAsUsed (target);
    }

    typedef RMDeferred
      ThisType;

    typedef CS::RenderManager::MultipleRenderLayer
      RenderLayerType;

    typedef CS::RenderManager::ShadowPSSM<RenderTreeType, RenderLayerType>
      ShadowType;

    typedef StandardContextSetup<RenderTreeType, RenderLayerType> 
      ContextSetupType;

    typedef CS::RenderManager::StandardPortalSetup<RenderTreeType, ContextSetupType> 
      PortalSetupType;

    typedef CS::RenderManager::LightSetup<RenderTreeType, RenderLayerType, ShadowType> 
      LightSetupType;

    typedef CS::RenderManager::DependentTargetManager<RenderTreeType, ThisType>
      TargetManagerType;

    typedef CS::RenderManager::AutoFX::ReflectRefract<RenderTreeType, ContextSetupType>
      AutoReflectRefractType;

    typedef CS::RenderManager::AutoFX::FramebufferTex<RenderTreeType>
      AutoFramebufferTexType;

    //---- iDebugHelper Interface ----
    virtual bool DebugCommand(const char *cmd);

  public:

    bool RenderView(iView *view, bool recursePortals);
    bool HandleTarget (RenderTreeType& renderTree, 
      const TargetManagerType::TargetSettings& settings,
      bool recursePortals, iGraphics3D* g3d);

    // Target manager handler
    bool HandleTargetSetup (CS::ShaderVarStringID svName, csShaderVariable* sv, 
      iTextureHandle* textureHandle, iView*& localView)
    {
      return false;
    }

    size_t AddLayer(CS::RenderManager::MultipleRenderLayer& layers, csStringID type, const char* name, const char* file);
    size_t LocateLayer(const CS::RenderManager::MultipleRenderLayer &layers, csStringID shaderType);

    void ShowGBuffer(RenderTreeType &tree, GBuffer* buffer);

    iObjectRegistry *objRegistry;

    RenderTreeType::PersistentData treePersistent;
    PortalSetupType::PersistentData portalPersistent;
    LightSetupType::PersistentData lightPersistent;
    DeferredLightRenderer<ShadowType>::PersistentData lightRenderPersistent;

    AutoReflectRefractType::PersistentData reflectRefractPersistent;
    AutoFramebufferTexType::PersistentData framebufferTexPersistent;

    CS::RenderManager::HDRHelper hdr;
    CS::RenderManager::HDR::Exposure::Configurable hdrExposure;
    bool doHDRExposure;

    CS::RenderManager::MultipleRenderLayer renderLayer;

    csRef<iShaderManager> shaderManager;
    csRef<iLightManager> lightManager;
    csRef<iStringSet> stringSet;

    TargetManagerType targets;
    csSet<RenderTreeType::ContextNode*> contextsScannedForTargets;

    GBuffer gbuffer;
    GBuffer::Description gbufferDescription;

    size_t deferredLayer;
    size_t lightingLayer;
    size_t zonlyLayer;
    int maxPortalRecurse;
    bool doShadows;

    bool showGBuffer;
    bool drawLightVolumes;

    uint dbgFlagClipPlanes;
  };
}
CS_PLUGIN_NAMESPACE_END(RMDeferred)

#endif // __DEFERRED_H__
