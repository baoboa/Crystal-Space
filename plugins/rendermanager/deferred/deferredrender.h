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

#ifndef __DEFERREDRENDER_H__
#define __DEFERREDRENDER_H__

#include "cssysdef.h"

#include "csplugincommon/rendermanager/render.h"
#include "csplugincommon/rendermanager/rendertree.h"

#include "deferredlightrender.h"
#include "deferredoperations.h"
#include "gbuffer.h"

CS_PLUGIN_NAMESPACE_BEGIN(RMDeferred)
{
  // @@@TODO: add new comment explaining how to use this renderer
  template<typename RenderTree, typename ShadowHandler>
  class DeferredTreeRenderer
  {
  public:
    typedef typename RenderTree::ContextNode ContextNodeType;
    typedef CS::RenderManager::SimpleContextRender<RenderTree> RenderType;
    typedef DeferredLightRenderer<ShadowHandler> LightRenderType;

    DeferredTreeRenderer(iGraphics3D* g3d, 
                         iShaderManager* shaderMgr,
                         typename LightRenderType::PersistentData& lightRenderPersistent,
			 GBuffer& gbuffer,
                         size_t deferredLayer,
			 size_t lightingLayer,
                         size_t zonlyLayer,
                         bool drawLightVolumes)
      : 
    meshRender(g3d, shaderMgr),
    graphics3D(g3d),
    shaderMgr(shaderMgr),
    lightRenderPersistent(lightRenderPersistent),
    gbuffer(gbuffer),
    deferredLayer(deferredLayer),
    lightingLayer(lightingLayer),
    zonlyLayer(zonlyLayer),
    drawLightVolumes(drawLightVolumes),
    useDeferredShading(lightingLayer == (size_t)-1),
    context(nullptr),
    rview(nullptr),
    hasTarget(false)
    {}

    ~DeferredTreeRenderer() 
    {
      if(context)
	RenderContextStack();
    }

    /**
     * Render all contexts.
     */
    void operator()(ContextNodeType* newContext)
    {
      if(IsNew(newContext))
      {
        // New context, render out the old ones
        if(context)
	  RenderContextStack();

	// set comparison variables accordingly
        context = newContext;
        rview = context->renderView;

	// check whether this stack will be rendered off-screen
	hasTarget = false;
	for(int i = 0; i < rtaNumAttachments; ++i)
	{
	  if(context->renderTargets[i].texHandle != (iTextureHandle*)nullptr)
	  {
	    hasTarget = true;
	    break;
	  }
	}
      }
      
      contextStack.Push(newContext);
    }

  protected:

    /**
     * Returns true if the given context is different from the last context.
     */
    bool IsNew(ContextNodeType* newContext)
    {
      if(!context || rview != newContext->renderView)
	return true;
      else
	return !HasSameTargets(newContext);
    }

    /**
     * Returns true if the given context has the same target buffers 
     * as the last context.
     */
    bool HasSameTargets(ContextNodeType* newContext)
    {
      for(int i = 0; i < rtaNumAttachments; ++i)
      {
	if(newContext->renderTargets[i].subtexture != context->renderTargets[i].subtexture
	|| newContext->renderTargets[i].texHandle  != context->renderTargets[i].texHandle)
	{
	  return false;
	}
      }

      return newContext->doDeferred == context->doDeferred;
    }

    // sets up render targets for the final draw
    void SetupTargets()
    {
      // check whether we have any targets to attach
      if(hasTarget)
      {
	// check whether we need persistent targets
	bool persist = !(context->drawFlags & CSDRAW_CLEARSCREEN);

	// do the attachment
	for(int a = 0; a < rtaNumAttachments; a++)
	  graphics3D->SetRenderTarget(context->renderTargets[a].texHandle, persist,
	      context->renderTargets[a].subtexture, csRenderTargetAttachment(a));

	// validate the targets
	CS_ASSERT(graphics3D->ValidateRenderTargets());
      }
    }

    // renders for visibility culling
    void RenderVisibilityCulling()
    {
      // visculling only needs to be rendered once per sector.
      csSet<iSector*> sectors;
      for(size_t c = 0; c < contextStack.GetSize(); ++c)
      {
	ContextNodeType* ctx = contextStack[c];

	// check whether this sector was already handled
	if(!sectors.Contains(ctx->sector))
	{
	  // ensure it won't be handled again
	  sectors.AddNoTest(ctx->sector);

	  // set camera transform
          graphics3D->SetWorldToCamera(ctx->cameraTransform.GetInverse());

	  // render for visibility culling if required by culler
	  ctx->sector->GetVisibilityCuller()->RenderViscull(rview, ctx->shadervars);
	}
      }
    }

    void RenderContextStack()
    {
      // get number of contexts in this stack - we'll use this while rendering them
      const size_t ctxCount = contextStack.GetSize();

      // obtain our camera
      iCamera* cam = rview->GetCamera();
      CS_ASSERT(cam);

      // shared setup for all passes - projection only
      CS::Math::Matrix4 projMatrix(context->perspectiveFixup * cam->GetProjectionMatrix());

      // check up to how many layers we have to draw
      size_t layerCount = 0;
      {
	/* Different contexts may have different numbers of layers,
	 * so determine the upper layer number */
	for(size_t i = 0; i < ctxCount; ++i)
	{
	  layerCount = csMax(layerCount,
	    contextStack[i]->svArrays.GetNumLayers());
	}
      }

      // not a deferred stack, just render by layer
      if(!context->doDeferred)
      {
	// setup projection matrix
	graphics3D->SetProjectionMatrix(projMatrix);

	// attach targets
	SetupTargets();

	// setup clipper
	graphics3D->SetClipper(rview->GetClipper(), CS_CLIPPER_TOPLEVEL);

        int drawFlags = CSDRAW_3DGRAPHICS | context->drawFlags;
	drawFlags |= CSDRAW_CLEARZBUFFER;

	// start the draw
        CS::RenderManager::BeginFinishDrawScope bd(graphics3D, drawFlags);

	// we don't have a z-buffer, yet, use pass 1 modes
	graphics3D->SetZMode(CS_ZBUF_MESH);

	// Visibility Culling
	RenderVisibilityCulling();

	// render out all layers
	for(size_t i = 0; i < layerCount; ++i)
	{
	  RenderLayer<false>(i, ctxCount);
	}

	// clear clipper
	graphics3D->SetClipper(nullptr, CS_CLIPPER_TOPLEVEL);

	contextStack.Empty();

	return;
      }

      // create the light render here as we'll use it a lot
      LightRenderType lightRender(graphics3D, shaderMgr, rview,
				  gbuffer, lightRenderPersistent);

      // shared setup for deferred passes
      graphics3D->SetProjectionMatrix(context->gbufferFixup * projMatrix);

      // set tex scale to default
      lightRenderPersistent.scale->SetValue(csVector4(0.5,0.5,0.5,0.5));

      // gbuffer fill step
      gbuffer.Attach();
      {
	// setup clipper
	if(context->useClipper)
	  graphics3D->SetClipper(rview->GetClipper(), CS_CLIPPER_TOPLEVEL);
	else
	  graphics3D->SetClipper(nullptr, CS_CLIPPER_TOPLEVEL);

        int drawFlags = CSDRAW_3DGRAPHICS | context->drawFlags;
        drawFlags |= CSDRAW_CLEARSCREEN | CSDRAW_CLEARZBUFFER;

	// start the draw
        CS::RenderManager::BeginFinishDrawScope bd(graphics3D, drawFlags);

	// we want to fill the depth buffer, use pass 1 modes
        graphics3D->SetZMode(CS_ZBUF_MESH);

        // z only pass - maybe we shouldn't allow disabling it.
	if(zonlyLayer != (size_t)-1)
        {
	  RenderLayer<false>(zonlyLayer, ctxCount);
        }

	// deferred pass
        // @@@TODO: we could check for CS_ENTITY_NOLIGHTING and
        //          CS_ENTITY_NOSHADOWS here and use it to fill
        //          the stencil buffer so those parts can be
        //          skipped during the lighting pass
	RenderLayer<true>(deferredLayer, ctxCount);

	// clear clipper
	graphics3D->SetClipper(nullptr, CS_CLIPPER_TOPLEVEL);
      }

      // light accumulation step
      gbuffer.AttachAccumulation();
      {
	// setup clipper
	if(context->useClipper)
	  graphics3D->SetClipper(rview->GetClipper(), CS_CLIPPER_TOPLEVEL);
	else
	  graphics3D->SetClipper(nullptr, CS_CLIPPER_TOPLEVEL);

        int drawFlags = CSDRAW_3DGRAPHICS | context->drawFlags;
	drawFlags |= CSDRAW_CLEARSCREEN | CSDRAW_CLEARZBUFFER;

	// start the draw
	CS::RenderManager::BeginFinishDrawScope bd(graphics3D, drawFlags);

	// use pass 1 zmodes for re-populating the zbuffer
	graphics3D->SetZMode(CS_ZBUF_MESH);

	// we use the inaccurate version from our gbuffer here - it's sufficient
        // @@@NOTE: appearently using an explicit depth output prevents hardware
        //          optimizations for the zbuffer to kick in - maybe we should
	//	    just go for a fragkill solution?
	lightRender.OutputDepth();

	// we're done with all depth writes - use pass 2 modes
	graphics3D->SetZMode(CS_ZBUF_MESH2);

	// accumulate lighting data
	RenderLights(deferredLayer, ctxCount, lightRender);

	// clear clipper
	graphics3D->SetClipper(nullptr, CS_CLIPPER_TOPLEVEL);
      }

      // setup projection matrix for final pass
      graphics3D->SetProjectionMatrix(projMatrix);

      // attach output render targets if any
      SetupTargets();
      {
	// setup clipper
	graphics3D->SetClipper(rview->GetClipper(), CS_CLIPPER_TOPLEVEL);

        int drawFlags = CSDRAW_3DGRAPHICS | context->drawFlags;
	drawFlags |= CSDRAW_CLEARZBUFFER;

	// start the draw
        CS::RenderManager::BeginFinishDrawScope bd(graphics3D, drawFlags);

	// we want to re-populate the depth buffer, use pass 1 modes.
	graphics3D->SetZMode(CS_ZBUF_MESH);

	// Visibility Culling
	RenderVisibilityCulling();

	// set tex scale for lookups.
	lightRenderPersistent.scale->SetValue(context->texScale);

	// deferred shading - output step
	if(useDeferredShading)
	{
	  lightRender.OutputResults();
	}
	// deferred lighting - output step
	else
	{
	  RenderLayer<true>(lightingLayer, ctxCount);
	}

        // forward rendering
	RenderForwardMeshes(layerCount, ctxCount);

	// deferred rendering - debug step if wanted
	if(drawLightVolumes)
	{
          LightVolumeRenderer<LightRenderType> lightVolumeRender(lightRender, true, 0.5f);

	  // output light volumes
	  RenderLights(deferredLayer, ctxCount, lightVolumeRender);
	}

	// clear clipper
	graphics3D->SetClipper(nullptr, CS_CLIPPER_TOPLEVEL);
      }

      // clear context stack
      contextStack.Empty();
    }

    template<bool deferredOnly>
    void RenderLayer(size_t layer, const size_t ctxCount)
    {
      // set layer
      meshRender.SetLayer(layer);

      // render meshes
      if(deferredOnly)
	RenderObjects<RenderType, ForEachDeferredMeshNode>(layer, ctxCount, meshRender);
      else
	RenderObjects<RenderType, CS::RenderManager::ForEachMeshNode>(layer, ctxCount, meshRender);
    }

    template<typename T>
    void RenderLights(size_t layer, const size_t ctxCount, T& render)
    {
      // render all lights
      RenderObjects<T, ForEachLight>(layer, ctxCount, render);
    }

    void RenderForwardMeshes(size_t layerCount, const size_t ctxCount)
    {
      // iterate over all layers
      for (size_t layer = 0; layer < layerCount; ++layer)
      {
	// set layer
	meshRender.SetLayer(layer);

	// render all forward meshes
	RenderObjects<RenderType, ForEachForwardMeshNode>(layer, ctxCount, meshRender);
      }
    }

    template<typename T, void fn(ContextNodeType&,T&)>
    inline void RenderObjects(size_t layer, const size_t ctxCount, T& render)
    {
      for(size_t i = 0; i < ctxCount; ++i)
      {
        ContextNodeType *ctx = contextStack[i];

        // check whether this context needs to be rendered
        size_t layerCount = ctx->svArrays.GetNumLayers();
        if(layer >= layerCount)
	  continue;

	// set camera transform
        graphics3D->SetWorldToCamera(ctx->cameraTransform.GetInverse ());

	// render all objects given a render
	fn(*ctx, render);
      }
    }

  private:

    // renderer
    RenderType meshRender;

    // data from parent
    iGraphics3D* graphics3D;
    iShaderManager* shaderMgr;

    // render objects from parent
    typename LightRenderType::PersistentData& lightRenderPersistent;
    GBuffer& gbuffer;

    // render layer data from parent
    size_t deferredLayer;
    size_t lightingLayer;
    size_t zonlyLayer;

    // render options from parent
    bool drawLightVolumes;
    bool useDeferredShading;

    // current context stack we're going to render
    csArray<ContextNodeType*> contextStack;

    // data for current context
    ContextNodeType* context;
    CS::RenderManager::RenderView* rview;
    bool hasTarget;
  };

}
CS_PLUGIN_NAMESPACE_END(RMDeferred)

#endif // __DEFERREDRENDER_H__
