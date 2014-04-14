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

#include "cssysdef.h"

#include "csplugincommon/rendermanager/renderview.h"
#include "csplugincommon/rendermanager/dependenttarget.h"
#include "csplugincommon/rendermanager/hdrhelper.h"
#include "csplugincommon/rendermanager/lightsetup.h"
#include "csplugincommon/rendermanager/occluvis.h"
#include "csplugincommon/rendermanager/operations.h"
#include "csplugincommon/rendermanager/portalsetup.h"
#include "csplugincommon/rendermanager/posteffects.h"
#include "csplugincommon/rendermanager/render.h"
#include "csplugincommon/rendermanager/renderlayers.h"
#include "csplugincommon/rendermanager/rendertree.h"
#include "csplugincommon/rendermanager/shadersetup.h"
#include "csplugincommon/rendermanager/standardsorter.h"
#include "csplugincommon/rendermanager/svsetup.h"
#include "csplugincommon/rendermanager/viscull.h"

#include "iengine.h"
#include "ivideo.h"
#include "ivaria/reporter.h"
#include "csutil/cfgacc.h"

#include "deferredoperations.h"
#include "deferredshadersetup.h"
#include "deferredrender.h"
#include "deferred.h"

using namespace CS::RenderManager;

CS_PLUGIN_NAMESPACE_BEGIN(RMDeferred)
{

SCF_IMPLEMENT_FACTORY(RMDeferred);

//----------------------------------------------------------------------

template<typename RenderTreeType, typename LayerConfigType>
class StandardContextSetup
{
public:
  typedef StandardContextSetup<RenderTreeType, LayerConfigType> ThisType;
  typedef StandardPortalSetup<RenderTreeType, ThisType> PortalSetupType;
  typedef typename RMDeferred::LightSetupType LightSetupType;
  typedef typename LightSetupType::ShadowHandlerType ShadowType;

  StandardContextSetup(RMDeferred *rmanager, const LayerConfigType &layerConfig)
    : 
  rmanager(rmanager),
  layerConfig(layerConfig),
  recurseCount(0), 
  deferredLayer(rmanager->deferredLayer),
  lightingLayer(rmanager->lightingLayer),
  zonlyLayer(rmanager->zonlyLayer),
  maxPortalRecurse(rmanager->maxPortalRecurse)
  {}

  StandardContextSetup (const StandardContextSetup &other, const LayerConfigType &layerConfig)
    :
  rmanager(other.rmanager), 
  layerConfig(layerConfig),
  recurseCount(other.recurseCount),
  deferredLayer(other.deferredLayer),
  lightingLayer(other.lightingLayer),
  zonlyLayer(other.zonlyLayer),
  maxPortalRecurse(other.maxPortalRecurse)
  {}

  void operator()(typename RenderTreeType::ContextNode &context, 
    typename PortalSetupType::ContextSetupData &portalSetupData,
    bool recursePortals = true)
  {
    if (recurseCount > maxPortalRecurse) return;

    CS::RenderManager::RenderView* rview = context.renderView;
    iSector* sector = rview->GetThisSector ();
    
    iShaderManager* shaderManager = rmanager->shaderManager;

    // @@@ This is somewhat "boilerplate" sector/rview setup.
    sector->PrepareDraw (rview);

    // Make sure the clip-planes are ok
    CS::RenderViewClipper::SetupClipPlanes (rview->GetRenderContext ());

    // Do the culling
    iVisibilityCuller *culler = sector->GetVisibilityCuller ();
    Viscull<RenderTreeType> (context, rview, culler);

    // enable deferred rendering for this context
    context.doDeferred = true;

    // setup gbuffer transform.
    // @@@TODO: we shouldn't have to recalculate this for all contexts - it only changes if the attachment/rview changes
    context.useClipper = SetupTarget(context.renderTargets, context.gbufferFixup, context.texScale);

    // Set up all portals
    if (recursePortals)
    {
      recurseCount++;
      PortalSetupType portalSetup (rmanager->portalPersistent, *this);      
      portalSetup (context, portalSetupData);
      recurseCount--;
    }
    
    // Sort the mesh lists  
    {
      StandardMeshSorter<RenderTreeType> mySorter (rview->GetEngine ());
      mySorter.SetupCameraLocation (rview->GetCamera ()->GetTransform ().GetOrigin ());
      ForEachMeshNode (context, mySorter);
    }

    // After sorting, assign in-context per-mesh indices
    {
      SingleMeshContextNumbering<RenderTreeType> numbering;
      ForEachMeshNode (context, numbering);
    }

    // Setup the SV arrays
    // Push the default stuff
    SetupStandardSVs (context, layerConfig, shaderManager, sector);

    // Setup the material&mesh SVs
    {
      StandardSVSetup<RenderTreeType, MultipleRenderLayer> svSetup (context.svArrays, layerConfig);
      ForEachMeshNode (context, svSetup);
    }

    // Setup shaders and tickets
    DeferredSetupShader (context, shaderManager, layerConfig, deferredLayer, lightingLayer, zonlyLayer);

    // Setup shadows and lighting
    {
      typename ShadowType::ShadowParameters shadowParam (rmanager->lightPersistent.shadowPersist, context.owner, rview);

      if(rmanager->doShadows)
      {
	// setup shadows for all lights
	ForEachLight (context, shadowParam);

	// setup shadows for all meshes
	ForEachMeshNode (context, shadowParam);

	// finish shadow setup
	shadowParam();
      }

      LightSetupType lightSetup (rmanager->lightPersistent, 
				 rmanager->lightManager,
				 context.svArrays, 
				 layerConfig, 
				 shadowParam);

      ForEachForwardMeshNode (context, lightSetup);

      // Setup shaders and tickets
      SetupStandardTicket (context, shaderManager, lightSetup.GetPostLightingLayers ());
    }

    {
      ThisType ctxRefl (*this, layerConfig);
      ThisType ctxRefr (*this, layerConfig);
      RMDeferred::AutoReflectRefractType fxRR (
        rmanager->reflectRefractPersistent, ctxRefl, ctxRefr);
        
      RMDeferred::AutoFramebufferTexType fxFB (
        rmanager->framebufferTexPersistent);
      
      // Set up a functor that combines the AutoFX functors
      typedef CS::Meta::CompositeFunctorType2<
        RMDeferred::AutoReflectRefractType,
        RMDeferred::AutoFramebufferTexType> FXFunctor;
      FXFunctor fxf (fxRR, fxFB);
      
      typedef TraverseUsedSVSets<RenderTreeType,
        FXFunctor> SVTraverseType;
      SVTraverseType svTraverser
        (fxf, shaderManager->GetSVNameStringset ()->GetSize (),
	 fxRR.svUserFlags | fxFB.svUserFlags);
      // And do the iteration
      ForEachMeshNode (context, svTraverser);
    }
  }

  // Called by AutoReflectRefractType
  void operator() (typename RenderTreeType::ContextNode& context)
  {
    typename PortalSetupType::ContextSetupData portalData (&context);

    operator() (context, portalData);
  }

private:

  bool SetupTarget(typename RenderTreeType::ContextNode::TargetTexture* targets, CS::Math::Matrix4& m, csVector4& v)
  {
    // find a target if there is any
    // MRTs are required to be of equal size, so one is enough
    iTextureHandle* target = nullptr;
    for(int i = 0; i < rtaNumAttachments; ++i)
    {
      if(targets[i].texHandle)
      {
	target = targets[i].texHandle;
	break;
      }
    }

    // init to identity
    m = CS::Math::Matrix4();
    if(target)
    {
      int width, height;
      rmanager->gbuffer.GetDimensions(width,height);
      int targetW, targetH;
      target->GetRendererDimensions (targetW, targetH);

      if(width != targetW || height != targetH)
      {
	// disable clipping for deferred passes if we have to scale down
	// @@@FIXME: it'd be better to scale the clipper instead
	bool useClipper = !(targetW > width || targetH > height);

	// we have to scale down if target is bigger than gbuffer
	if(!useClipper)
	{
	  targetW = csMin(targetW, width);
	  targetH = csMin(targetH, height);
	}

	// calculate perspective fixup for gbuffer pass.
	float scaleX = float(targetW)/float(width);
	float scaleY = float(targetH)/float(height);
	float shiftX = scaleX - 1.0f;
	float shiftY = scaleY - 1.0f;

	m = CS::Math::Matrix4 (
	  scaleX, 0, 0, shiftX,
	  0, scaleY, 0, shiftY,
	  0, 0, 1, 0,
	  0, 0, 0, 1);

	// calculate texture scale xform for gbuffer reads
        float scaleXTex = 0.5f*scaleX;
        float scaleYTex = 0.5f*scaleY;
        v = csVector4(scaleXTex, scaleYTex, scaleXTex, 1.0f-scaleYTex);

	return useClipper;
      }
      else
      {
        // matching sizes and no flipped y.
        v = csVector4(0.5,0.5,0.5,0.5);
      }
    }
    else
    {
      // we're rendering to the screen, so we have flipped y during lookups.
      v = csVector4(0.5,-0.5,0.5,0.5);
    }
    return true;
  }

  RMDeferred *rmanager;

  const LayerConfigType &layerConfig;

  int recurseCount;
  size_t deferredLayer;
  size_t lightingLayer;
  size_t zonlyLayer;
  int maxPortalRecurse;
};

//----------------------------------------------------------------------
RMDeferred::RMDeferred(iBase *parent) 
  : 
scfImplementationType(this, parent),
doHDRExposure (false),
targets (*this)
{
  SetTreePersistent (treePersistent);
}

//----------------------------------------------------------------------
bool RMDeferred::Initialize(iObjectRegistry *registry)
{
  const char *messageID = "crystalspace.rendermanager.deferred";

  objRegistry = registry;

  csRef<iGraphics3D> graphics3D = csQueryRegistry<iGraphics3D> (objRegistry);
  iGraphics2D *graphics2D = graphics3D->GetDriver2D ();
   
  shaderManager = csQueryRegistry<iShaderManager> (objRegistry);
  lightManager = csQueryRegistry<iLightManager> (objRegistry);
  stringSet = csQueryRegistryTagInterface<iStringSet> (objRegistry, "crystalspace.shared.stringset");

  // Initialize the extra data in the persistent tree data.
  RenderTreeType::TreeTraitsType::Initialize (treePersistent, registry);
  
  // Read Config settings.
  csConfigAccess cfg (objRegistry);
  maxPortalRecurse = cfg->GetInt ("RenderManager.Deferred.MaxPortalRecurse", 30);
  doShadows = cfg->GetBool ("RenderManager.Deferred.Shadows.Enabled", true);
  showGBuffer = false;
  drawLightVolumes = false;

  // get shader type IDs
  csStringID depthWriteID = stringSet->Request ("depthwrite");
  csStringID deferredFullID = stringSet->Request ("deferred full");
  csStringID deferredFillID = stringSet->Request ("deferred fill");
  csStringID deferredUseID = stringSet->Request ("deferred use");

  // load render layers
  bool deferredFull = true;
  bool layersValid = false;
  const char *layersFile = cfg->GetStr ("RenderManager.Deferred.Layers", nullptr);
  if (layersFile)
  {
    csReport (objRegistry, CS_REPORTER_SEVERITY_NOTIFY, messageID, 
      "Reading render layers from %s", CS::Quote::Single (layersFile));

    layersValid = CS::RenderManager::AddLayersFromFile (objRegistry, layersFile, renderLayer);
    
    if (layersValid) 
    {
      // Locates the deferred shading layer.
      deferredLayer = LocateLayer (renderLayer, deferredFullID);

      // check whether we have a full deferred layer
      if (deferredLayer != (size_t)-1)
      {
	// we have one - use full deffered shading
	lightingLayer = (size_t)-1;
      }
      else
      {
	// no full deferred layer, locate deferred lighting layers
        deferredLayer = LocateLayer (renderLayer, deferredFillID);
	lightingLayer = LocateLayer (renderLayer, deferredUseID);

	// check whether we got both
	if (deferredLayer != (size_t)-1 && lightingLayer != (size_t)-1)
	{
	  // we'll use deferred lighting
	  deferredFull = false;
	}
	else
	{
	  // couldn't locate any deferred layer - warn and use default full shading
	  csReport (objRegistry, CS_REPORTER_SEVERITY_WARNING,
	    messageID, "The render layers file %s contains neither a %s nor both %s and %s layers. Using default %s layer.",
	    CS::Quote::Single (layersFile),
	    CS::Quote::Single ("deferred full"),
	    CS::Quote::Single ("deferred fill"),
	    CS::Quote::Single ("deferred use"),
	    CS::Quote::Single ("deferred full"));

	  deferredLayer = AddLayer (renderLayer, deferredFullID, "gbuffer_fill_full", "/shader/deferred/full/fill.xml");
	  lightingLayer = (size_t)-1;
	}
      }

      // Locates the zonly shading layer.
      zonlyLayer = LocateLayer (renderLayer, depthWriteID);
    }
    else
    {
      renderLayer.Clear ();
    }
  }
  
  csRef<iLoader> loader = csQueryRegistry<iLoader> (objRegistry);
  if (!layersValid)
  {
    csReport (objRegistry, CS_REPORTER_SEVERITY_NOTIFY, messageID,
      "Using default render layers");

    zonlyLayer = AddLayer (renderLayer, depthWriteID, "z_only", "/shader/early_z/z_only.xml");
    deferredLayer = AddLayer (renderLayer, deferredFullID, "gbuffer_fill_full", "/shader/deferred/full/fill.xml");
    lightingLayer = (size_t)-1;

    if (!loader->LoadShader ("/shader/lighting/lighting_default.xml"))
    {
      csReport (objRegistry, CS_REPORTER_SEVERITY_WARNING,
        messageID, "Could not load lighting_default shader");
    }

    CS::RenderManager::AddDefaultBaseLayers (objRegistry, renderLayer);
  }

  csReport (objRegistry, CS_REPORTER_SEVERITY_NOTIFY, messageID,
    "Using deferred %s.", deferredFull ? "shading" : "lighting");

  // Create GBuffer
  // @@@TODO: not really flexible without touching lighting shaders which aren't as flexible atm
  const char *gbufferFmt = cfg->GetStr ("RenderManager.Deferred.GBuffer.BufferFormat", "rgba16_f");
  const char *accumFmt = cfg->GetStr ("RenderManager.Deferred.GBuffer.AccumBufferFormat", deferredFull ? "rgba16_f" : "rgb16_f");
  int bufferCount = cfg->GetInt ("RenderManager.Deferred.GBuffer.BufferCount", deferredFull ? 5 : 1);
  int accumCount = cfg->GetInt ("RenderManager.Deferred.GBuffer.AccumBufferCount", deferredFull ? 1 : 2);

  gbufferDescription.colorBufferCount = bufferCount;
  gbufferDescription.accumBufferCount = accumCount;
  gbufferDescription.width = graphics2D->GetWidth ();
  gbufferDescription.height = graphics2D->GetHeight ();
  gbufferDescription.colorBufferFormat = gbufferFmt;
  gbufferDescription.accumBufferFormat = accumFmt;

  if (!gbuffer.Initialize (gbufferDescription, 
                           graphics3D, 
                           shaderManager->GetSVNameStringset (), 
                           objRegistry))
  {
    return false;
  }

  treePersistent.Initialize (shaderManager);
  portalPersistent.Initialize (shaderManager, graphics3D, treePersistent.debugPersist);
  lightPersistent.shadowPersist.SetConfigPrefix ("RenderManager.Deferred");
  lightPersistent.Initialize (registry, treePersistent.debugPersist);
  if(!lightRenderPersistent.Initialize (registry, lightPersistent.shadowPersist, doShadows, deferredFull))
  {
    return false;
  }

  // initialize post-effects
  PostEffectsSupport::Initialize (registry, "RenderManager.Deferred");

  // initialize hdr
  HDRSettings hdrSettings (cfg, "RenderManager.Deferred");
  if (hdrSettings.IsEnabled())
  {
    doHDRExposure = true;
    
    hdr.Setup (registry, 
      hdrSettings.GetQuality(), 
      hdrSettings.GetColorRange());
    postEffects.SetChainedOutput (hdr.GetHDRPostEffects());
  
    hdrExposure.Initialize (registry, hdr, hdrSettings);
  }

  // initialize reflect/refract
  dbgFlagClipPlanes = treePersistent.debugPersist.RegisterDebugFlag ("draw.clipplanes.view");
  reflectRefractPersistent.Initialize (registry, treePersistent.debugPersist, &postEffects);
  framebufferTexPersistent.Initialize (registry, &postEffects);

  RMViscullCommon::Initialize (objRegistry, "RenderManager.Deferred");
  
  return true;
}

//----------------------------------------------------------------------
bool RMDeferred::RenderView(iView *view, bool recursePortals)
{
  iGraphics3D *graphics3D = view->GetContext ();

  view->UpdateClipper ();

  int frameWidth = graphics3D->GetWidth ();
  int frameHeight = graphics3D->GetHeight ();
  view->GetCamera ()->SetViewportSize (frameWidth, frameHeight);

  // Setup renderview
  csRef<CS::RenderManager::RenderView> rview;
  rview = treePersistent.renderViews.GetRenderView (view);
  rview->SetOriginalCamera (view->GetCamera ());

  // Computes the left, right, top, and bottom of the view frustum.
  iPerspectiveCamera *camera = view->GetPerspectiveCamera ();
  float invFov = camera->GetInvFOV ();
  float l = -invFov * camera->GetShiftX ();
  float r =  invFov * (frameWidth - camera->GetShiftX ());
  float t = -invFov * camera->GetShiftY ();
  float b =  invFov * (frameHeight - camera->GetShiftY ());
  rview->SetFrustum (l, r, t, b);

  contextsScannedForTargets.Empty ();
  portalPersistent.UpdateNewFrame ();
  lightPersistent.UpdateNewFrame ();
  lightRenderPersistent.UpdateNewFrame ();
  reflectRefractPersistent.UpdateNewFrame ();
  framebufferTexPersistent.UpdateNewFrame ();

  iEngine *engine = view->GetEngine ();
  engine->UpdateNewFrame ();
  engine->FireStartFrame (rview);

  iSector *startSector = rview->GetThisSector ();
  if (!startSector)
    return false;

  RenderTreeType renderTree (treePersistent);
  RenderTreeType::ContextNode *startContext = renderTree.CreateContext (rview);
  startContext->drawFlags |= CSDRAW_CLEARSCREEN;

  // Add gbuffer textures to be visualized.
  if (showGBuffer)
    ShowGBuffer (renderTree, &gbuffer);

  startContext->renderTargets[rtaColor0].texHandle = postEffects.GetScreenTarget ();
  postEffects.SetupView (view, startContext->perspectiveFixup);

  // Setup the main context
  {
    ContextSetupType contextSetup (this, renderLayer);
    ContextSetupType::PortalSetupType::ContextSetupData portalData (startContext);

    contextSetup (*startContext, portalData, recursePortals);

    targets.StartRendering (shaderManager);
    targets.EnqueueTargets (renderTree, shaderManager, renderLayer, contextsScannedForTargets);
  }

  // Setup all dependent targets
  while (targets.HaveMoreTargets ())
  {
    TargetManagerType::TargetSettings ts;
    targets.GetNextTarget (ts);

    HandleTarget (renderTree, ts, recursePortals, graphics3D);
  }

  targets.FinishRendering ();

  // Render all contexts.
  {
    DeferredTreeRenderer<RenderTreeType, ShadowType>
    render(graphics3D, shaderManager, lightRenderPersistent, gbuffer,
	   deferredLayer, lightingLayer, zonlyLayer, drawLightVolumes);

    ForEachContextReverse (renderTree, render);
  }

  postEffects.DrawPostEffects (renderTree);

  if (doHDRExposure) hdrExposure.ApplyExposure (renderTree, view);

  DebugFrameRender (rview, renderTree);

  return true;
}

bool RMDeferred::RenderView(iView *view)
{
  return RenderView (view, true);
}

//----------------------------------------------------------------------
bool RMDeferred::PrecacheView(iView *view)
{
  return RenderView (view, false);

  postEffects.ClearIntermediates ();
  hdr.GetHDRPostEffects().ClearIntermediates();
}

//----------------------------------------------------------------------
bool RMDeferred::HandleTarget (RenderTreeType& renderTree,
                               const TargetManagerType::TargetSettings& settings,
                               bool recursePortals, iGraphics3D* g3d)
{
  // Prepare
  csRef<CS::RenderManager::RenderView> rview;
  rview = treePersistent.renderViews.GetRenderView (settings.view);
  iCamera* c = settings.view->GetCamera ();
  rview->SetOriginalCamera (c);

  iSector* startSector = rview->GetThisSector ();

  if (!startSector)
    return false;

  RenderTreeType::ContextNode* startContext = renderTree.CreateContext (rview);
  startContext->renderTargets[rtaColor0].texHandle = settings.target;
  startContext->renderTargets[rtaColor0].subtexture = settings.targetSubTexture;
  startContext->drawFlags = settings.drawFlags;

  ContextSetupType contextSetup (this, renderLayer);
  ContextSetupType::PortalSetupType::ContextSetupData portalData (startContext);

  contextSetup (*startContext, portalData, recursePortals);
  
  targets.EnqueueTargets (renderTree, shaderManager, renderLayer, contextsScannedForTargets);

  return true;
}

//----------------------------------------------------------------------
size_t RMDeferred::AddLayer(CS::RenderManager::MultipleRenderLayer& layers, csStringID type, const char* name, const char* file)
{
  const char *messageID = "crystalspace.rendermanager.deferred";

  csRef<iLoader> loader = csQueryRegistry<iLoader> (objRegistry);

  if (!loader->LoadShader (file))
  {
    csReport (objRegistry, CS_REPORTER_SEVERITY_WARNING,
      messageID, "Could not load %s shader", name);
  }

  iShader *shader = shaderManager->GetShader (name);

  SingleRenderLayer baseLayer (shader, 0, 0);
  baseLayer.AddShaderType (type);

  layers.AddLayers (baseLayer);

  return layers.GetLayerCount () - 1;
}

//----------------------------------------------------------------------
size_t RMDeferred::LocateLayer(const CS::RenderManager::MultipleRenderLayer &layers,
                            csStringID shaderType)
{
  size_t count = renderLayer.GetLayerCount ();
  for (size_t i = 0; i < count; i++)
  {
    size_t num;
    const csStringID *strID = renderLayer.GetShaderTypes (i, num);
    for (size_t j = 0; j < num; j++)
    {
      if (strID[j] == shaderType)
      {
        return i;
      }
    }
  }

  return (size_t)-1;
}

//----------------------------------------------------------------------
void RMDeferred::ShowGBuffer(RenderTreeType &tree, GBuffer* buffer)
{
  int w, h;
  buffer->GetColorBuffer (0)->GetRendererDimensions (w, h);
  float aspect = (float)w / h;

  for (size_t i = 0; i < buffer->GetColorBufferCount(); i++)
  {
    tree.AddDebugTexture (buffer->GetColorBuffer (i), aspect);
  }

  tree.AddDebugTexture (buffer->GetDepthBuffer (), aspect);

  for (size_t i = 0; i < buffer->GetAccumulationBufferCount(); ++i)
  {
    tree.AddDebugTexture (buffer->GetAccumulationBuffer(i), aspect);
  }
}

//----------------------------------------------------------------------
bool RMDeferred::DebugCommand(const char *cmd)
{
  if (strcmp (cmd, "toggle_visualize_gbuffer") == 0)
  {
    showGBuffer = !showGBuffer;
    return true;
  }
  else if (strcmp (cmd, "toggle_visualize_lightvolumes") == 0)
  {
    drawLightVolumes = !drawLightVolumes;
    return true;
  }

  return RMDebugCommonBase::DebugCommand(cmd);
}

}
CS_PLUGIN_NAMESPACE_END(RMDeferred)
