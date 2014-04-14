/*
    Copyright (C) 2007-2008 by Marten Svanfeldt

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

#ifndef __CS_CSPLUGINCOMMON_RENDERMANAGER_PORTALSETUP_H__
#define __CS_CSPLUGINCOMMON_RENDERMANAGER_PORTALSETUP_H__

/**\file
 * Render manager portal setup.
 */

#include "iengine/movable.h"
#include "iengine/portal.h"
#include "iengine/portalcontainer.h"
#include "iengine/sector.h"
#include "csgeom/math3d.h"
#include "csgeom/polyclip.h"
#include "csgfx/renderbuffer.h"
#include "csgfx/shadervarcontext.h"
#include "cstool/rbuflock.h"

#include "csplugincommon/rendermanager/renderview.h"
#include "csplugincommon/rendermanager/svsetup.h"
#include "csplugincommon/rendermanager/texturecache.h"

namespace CS
{
namespace RenderManager
{
  /**
   * Base class for StandardPortalSetup, containing types and
   * members which are independent of the template arguments that can be
   * provided to StandardPortalSetup.
   */
  class CS_CRYSTALSPACE_EXPORT StandardPortalSetup_Base
  {
  public:
    /**
      * Data used by the helper that needs to persist over multiple frames.
      * Render managers must store an instance of this class and provide
      * it to the helper upon instantiation.
      */
    struct CS_CRYSTALSPACE_EXPORT PersistentData
    {

      /**
       * Cache structure for portal render buffers
       */
      struct PortalBuffers
      {
        csRef<iRenderBuffer> coordBuf;
        csRef<iRenderBuffer> tcBuf;
        csRef<iRenderBuffer> indexBuf;
        csRef<csRenderBufferHolder> holder;
      };

      /**
       * GenericCache-constraints for PortalBuffer caching
       */
      struct CS_CRYSTALSPACE_EXPORT PortalBufferConstraint
      {
        typedef size_t KeyType;

        static bool IsEqual (const PortalBuffers& b1,
                             const PortalBuffers& b2);
        static bool IsLargerEqual (const PortalBuffers& b1,
                                   const PortalBuffers& b2);
			     
        static bool IsEqual (const PortalBuffers& b1,
                            const KeyType& s2);
        static bool IsLargerEqual (const PortalBuffers& b1,
                                  const KeyType& s2);
        static bool IsLargerEqual (const KeyType& s1,
                                   const PortalBuffers& b2);
      };
      CS::Utility::GenericResourceCache<PortalBuffers, csTicks,
        PortalBufferConstraint> bufCache;

      struct BoxClipperCacheRefCounted;
      /**
       * Cache-helper for box clipper caching
       */
      struct CS_CRYSTALSPACE_EXPORT csBoxClipperCached : public csBoxClipper
      {
        BoxClipperCacheRefCounted* owningCache;

        csBoxClipperCached (BoxClipperCacheRefCounted* owningCache,
          const csBox2& box) : csBoxClipper (box),
          owningCache (owningCache)
        { }

        void operator delete (void* p, void* q);
        void operator delete (void* p);
      };
      struct csBoxClipperCachedStore
      {
        uint bytes[(sizeof(csBoxClipperCached) + sizeof (uint) - 1)/sizeof(uint)];
	
	csBoxClipperCachedStore()
	{ 
	  // Avoid gcc complaining about uninitialised use
	  memset (bytes, 0, sizeof (bytes));
	}
      };
      typedef CS::Utility::GenericResourceCache<csBoxClipperCachedStore, csTicks,
        CS::Utility::ResourceCache::SortingNone,
        CS::Utility::ResourceCache::ReuseConditionFlagged> BoxClipperCacheType;
      struct BoxClipperCacheRefCounted : public BoxClipperCacheType,
                                         public CS::Utility::FastRefCount<BoxClipperCacheRefCounted>
      {
        BoxClipperCacheRefCounted (
          const CS::Utility::ResourceCache::ReuseConditionFlagged& reuse,
          const CS::Utility::ResourceCache::PurgeConditionAfterTime<uint>& purge)
          : BoxClipperCacheType (reuse, purge) {}

        void FreeCachedClipper (csBoxClipperCached* bcc);
      };
      csRef<BoxClipperCacheRefCounted> boxClipperCache;

      CS::ShaderVarStringID svNameTexPortal;
    #ifdef CS_DEBUG
      csFrameDataHolder<csStringBase> stringHolder;
    #endif

      TextureCache texCache;

      /* Set these values to a positive value to fix the width and/or height of textures
       * queried from the texture cache. */
      int fixedTexCacheWidth;
      int fixedTexCacheHeight;

      /// Abstracts the usage of the texture cache.
      iTextureHandle* QueryUnusedTexture (int width, int height, 
                                          int& real_w, int& real_h)
      {
        if (fixedTexCacheWidth > 0)
          width = fixedTexCacheWidth;
        if (fixedTexCacheHeight > 0)
          height = fixedTexCacheHeight;

        return texCache.QueryUnusedTexture (width, height,
                                            real_w, real_h);
      }
      
      uint dbgDrawPortalOutlines;
      uint dbgDrawPortalPlanes;
      uint dbgShowPortalTextures;

      /// Construct helper
      PersistentData(int textCachOptions = TextureCache::tcachePowerOfTwo);

      /**
       * Initialize helper. Fetches various required values from objects in
       * the object registry.
       */
      void Initialize (iShaderManager* shmgr, iGraphics3D* g3d,
                       RenderTreeBase::DebugPersistent& dbgPersist);

      /**
       * Do per-frame house keeping - \b MUST be called every frame/
       * RenderView() execution.
       */
      void UpdateNewFrame ()
      {
        csTicks time = csGetTicks ();
        texCache.AdvanceFrame (time);
        bufCache.AdvanceTime (time);
        boxClipperCache->AdvanceTime (time);
      }
    };
  
    StandardPortalSetup_Base (PersistentData& persistentData)
      : persistentData (persistentData)
    {}
  protected:
    PersistentData& persistentData;

    void PortalDebugDraw (RenderTreeBase& renderTree,
                          iPortal* portal,
                          size_t count, const csVector2* portalVerts2d,
                          const csVector3* portalVerts3d,
                          int screenH,
                          bool isSimple, int skipRec);

    /// Create a box clipper for the given box
    csPtr<iClipper2D> CreateBoxClipper(const csBox2& box);

    /// Shift projection for portal rendering to texture.
    void SetupProjectionShift (iCustomMatrixCamera* newCam,
                               iCamera* inewcam,
                               int sb_minX, int sb_minY,
                               int txt_h,
                               int real_w, int real_h,
                               int screenW, int screenH);
    /**
     * Fudge transformation of a camera used for portal contents rendering
     * to reduce cracks. Used when portal is rendered to a texture.
     */
    void FudgeTargetCamera (iCamera* inewcam, iCamera* cam,
                            iPortal* portal, const csFlags& portalFlags,
                            size_t count, const csVector2* portalVerts2d,
                            const csVector3* portalVerts3d,
                            int screenW, int screenH);
    
    /// Get render buffers for rendering portal as a mesh
    csPtr<csRenderBufferHolder> GetPortalBuffers (size_t count,
                                                  const csVector2* portalVerts2d,
                                                  const csVector3* portalVerts3d,
                                                  bool withTCs = false,
                                                  int txt_h = 0,
                                                  int real_w = 0, int real_h = 0,
                                                  int sb_minX = 0, int sb_minY = 0);
    
    /// Rendermesh setup for rendering portal as a mesh
    csRenderMesh* SetupPortalRM (csRenderMesh* rm,
                                 iPortal* portal, iSector* sector,
                                 size_t count, RenderView* rview);
  };

  /**
   * Standard setup functor for portals.
   * Iterates over all portals in a context and sets up new contexts to
   * render the part of the scene "behind" the portal. Depending on the
   * settings of a portal, it is either rendered to the same target as the
   * context or a new texture (in which case the original context is
   * augmented with a mesh rendering that texture).
   *
   * Usage: instiate. Application after the visible meshes were determined,
   * but before mesh sorting.
   * Example:
   * \code
   *  // Type for portal setup
   *  typedef CS::RenderManager::StandardPortalSetup<RenderTreeType,
   *    ContextSetupType> PortalSetupType;
   *
   * // Assumes there is an argument 'PortalSetupType::ContextSetupData& portalSetupData'
   * // to the current method, taking data from the previous portal (if any)
   *
   * // Keep track of the portal recursions to avoid infinite portal recursions
   * if (recurseCount > 30) return;
   *
   * // Set up all portals
   * {
   *   recurseCount++;
   *   // Data needed to be passed between portal setup steps
   *   PortalSetupType portalSetup (rmanager->portalPersistent, *this);
   *   // Actual setup
   *   portalSetup (context, portalSetupData);
   *   recurseCount--;
   * }
   * \endcode
   *
   * The template parameter \a RenderTree gives the render tree type.
   * The parameter \a ContextSetup gives a class used to set up the contexts
   * for the rendering of the scene behind a portal. It must provide an
   * implementation of operator() (RenderTree::ContextNode& context,
   *   PortalSetupType::ContextSetupData& portalSetupData).
   *
   * \par Internal workings
   * The standard setup will classify portals into simple and heavy portals
   * respectively where simple portals can be rendered directly without clipping
   * while heavy portals requires render-to-texture.
   */
  template<typename RenderTreeType, typename ContextSetup>
  class StandardPortalSetup : public StandardPortalSetup_Base
  {
  public:
    typedef StandardPortalSetup<RenderTreeType, ContextSetup> ThisType;

    /**
     * Data that needs to be passed between portal setup steps by the
     * context setup function.
     */
    struct ContextSetupData
    {
      typename RenderTreeType::ContextNode* lastSimplePortalCtx;

      /// Construct, defaulting to no previous portal been rendered.
      ContextSetupData (typename RenderTreeType::ContextNode* last = 0)
        : lastSimplePortalCtx (last)
      {}
    };

    /// Constructor.
    StandardPortalSetup (PersistentData& persistentData, ContextSetup& cfun)
      : StandardPortalSetup_Base (persistentData), contextFunction (cfun)
    {}

    /**
     * Operator doing the actual work. Goes over the portals in the given
     * context and sets up rendering of behind the portals.
     */
    void operator() (typename RenderTreeType::ContextNode& context,
      ContextSetupData& setupData)
    {
      RenderView* rview = context.renderView;
      RenderTreeType& renderTree = context.owner;
      int screenW, screenH;
      if (!context.GetTargetDimensions (screenW, screenH))
      {
        screenW = rview->GetGraphics3D()->GetWidth();
        screenH = rview->GetGraphics3D()->GetHeight();
      }

      bool debugDraw =
	renderTree.IsDebugFlagEnabled (persistentData.dbgDrawPortalOutlines)
	|| renderTree.IsDebugFlagEnabled (persistentData.dbgDrawPortalPlanes);

      csDirtyAccessArray<csVector2> allPortalVerts2d (64);
      csDirtyAccessArray<csVector3> allPortalVerts3d (64);
      csDirtyAccessArray<size_t> allPortalVertsNums;
      // Handle all portals
      for (size_t pc = 0; pc < context.allPortals.GetSize (); ++pc)
      {
        typename RenderTreeType::ContextNode::PortalHolder& holder = context.allPortals[pc];
        const size_t portalCount = holder.portalContainer->GetPortalCount ();

        size_t allPortalVertices = holder.portalContainer->GetTotalVertexCount ();
        allPortalVerts2d.SetSize (allPortalVertices * 3);
        allPortalVerts3d.SetSize (allPortalVertices * 3);
        allPortalVertsNums.SetSize (portalCount);

        csVector2* portalVerts2d = allPortalVerts2d.GetArray();
        csVector3* portalVerts3d = allPortalVerts3d.GetArray();
        /* Get clipped screen space and camera space vertices */
        holder.portalContainer->ComputeScreenPolygons (rview,
          portalVerts2d, portalVerts3d,
          allPortalVerts2d.GetSize(), allPortalVertsNums.GetArray(),
          screenW, screenH);
	
        for (size_t pi = 0; pi < portalCount; ++pi)
        {
          iPortal* portal = holder.portalContainer->GetPortal (int (pi));
          const csFlags portalFlags (portal->GetFlags());

          // Finish up the sector
          if (!portal->CompleteSector (rview))
            continue;
	  
          size_t count = allPortalVertsNums[pi];
          if (count == 0) continue;
	  
	  iSector* sector = portal->GetSector ();
	  bool skipRec = sector->GetRecLevel() >= portal->GetMaximumSectorVisit();

	  if (debugDraw)
	  {
	    bool isSimple = IsSimplePortal (portalFlags);
            PortalDebugDraw (renderTree, portal,
                             count, portalVerts2d, portalVerts3d,
                             screenH, isSimple, skipRec);
	  }
	  
	  if (!skipRec)
	  {
	    sector->IncRecLevel();
	    if (IsSimplePortal (portalFlags))
	    {
	      SetupSimplePortal (context, setupData, portal, sector,
		  portalVerts2d, portalVerts3d, count, screenW, screenH, holder);
	    }
	    else
	    {
	      SetupHeavyPortal (context, setupData, portal, sector,
		  portalVerts2d, portalVerts3d, count, screenW, screenH, holder);
	    }
	    sector->DecRecLevel();
	  }

	  portalVerts2d += count;
          portalVerts3d += count;
        }
      }
    }

  private:
    ContextSetup& contextFunction;

    bool IsSimplePortal (const csFlags& portalFlags)
    {
      return (portalFlags.Get() & (CS_PORTAL_CLIPDEST
        | CS_PORTAL_CLIPSTRADDLING
        | CS_PORTAL_ZFILL
	| CS_PORTAL_MIRROR
	| CS_PORTAL_FLOAT)) == 0;
    }

    void ComputeVector2BoundingBox (const csVector2* verts, size_t count,
                                    csBox2& box)
    {
      if (count == 0)
      {
        box.StartBoundingBox ();
        return;
      }
      box.StartBoundingBox (verts[0]);
      for (size_t i = 1; i < count; i++)
        box.AddBoundingVertexSmart (verts[i]);
    }

    void SetupWarp (iCamera* inewcam, iMovable* movable, iPortal* portal)
    {
      const csReversibleTransform& movtrans = movable->GetFullTransform();
      bool mirror = inewcam->IsMirrored ();
      csReversibleTransform warp_wor;
      portal->ObjectToWorld (movtrans, warp_wor);
      portal->WarpSpace (warp_wor, inewcam->GetTransform (), mirror);
      inewcam->SetMirrored (mirror);
    }

    void SetupSimplePortal (
      typename RenderTreeType::ContextNode& context,
      ContextSetupData& setupData, iPortal* portal, iSector* sector,
      const csVector2* portalVerts2d, const csVector3* portalVerts3d, size_t count,
      int screenW, int screenH,
      typename RenderTreeType::ContextNode::PortalHolder& holder)
    {
      RenderView* rview = context.renderView;
      RenderTreeType& renderTree = context.owner;
      const csFlags portalFlags (portal->GetFlags());

      // Setup simple portal
      rview->CreateRenderContext ();
      rview->SetLastPortal (portal);
      rview->SetPreviousSector (rview->GetThisSector ());
      rview->SetThisSector (sector);
      csPolygonClipper newView (portalVerts2d, count);
      rview->SetViewDimensions (screenW, screenH);
      rview->SetClipper (&newView);

      if (portalFlags.Check (CS_PORTAL_WARP))
      {
	iCamera *inewcam = rview->CreateNewCamera ();
        SetupWarp (inewcam, holder.meshWrapper->GetMovable(), portal);
      }
	
      typename RenderTreeType::ContextNode* portalCtx =
              renderTree.CreateContext (rview, setupData.lastSimplePortalCtx);
      setupData.lastSimplePortalCtx = portalCtx;

      // Copy the target from last portal
      for (int a = 0; a < rtaNumAttachments; a++)
        portalCtx->renderTargets[a] = context.renderTargets[a];
      portalCtx->perspectiveFixup = context.perspectiveFixup;

      // Setup the new context
      contextFunction (*portalCtx, setupData);

      rview->RestoreRenderContext ();
      
      /* Create render mesh for the simple portal. Required to so simple
       * portals in fogged sectors look right. */
      if (rview->GetThisSector()->HasFog())
      {
        // Synthesize a render mesh for the portal plane
        bool meshCreated;
        csRenderMesh* rm = renderTree.GetPersistentData().rmHolder.GetUnusedMesh (
                    meshCreated, rview->GetCurrentFrameNumber());
        SetupPortalRM (rm, portal, sector, count, rview);
        rm->buffers = GetPortalBuffers (count, portalVerts2d, portalVerts3d,
                                        false);
        rm->variablecontext.Invalidate();
          
        typename RenderTreeType::MeshNode::SingleMesh sm;
        sm.meshObjSVs = 0;

        CS::Graphics::RenderPriority renderPrio =
            holder.meshWrapper->GetRenderPriority ();
        context.AddRenderMesh (rm, renderPrio, sm);
      }
    }

    void SetupHeavyPortal (
      typename RenderTreeType::ContextNode& context,
      ContextSetupData& setupData, iPortal* portal, iSector* sector,
      csVector2* portalVerts2d, csVector3* portalVerts3d, size_t count,
      int screenW, int screenH,
      typename RenderTreeType::ContextNode::PortalHolder& holder)
    {
      RenderView* rview = context.renderView;
      RenderTreeType& renderTree = context.owner;
      const csFlags portalFlags (portal->GetFlags());

      // Setup a bounding box, in screen-space
      csBox2 screenBox;
      ComputeVector2BoundingBox (portalVerts2d, count, screenBox);
      
      // Obtain a texture handle for the portal to render to
      int sb_minX = int (screenBox.MinX());
      int sb_minY = int (screenBox.MinY());
      int txt_w = int (ceil (screenBox.MaxX() - screenBox.MinX()));
      int txt_h = int (ceil (screenBox.MaxY() - screenBox.MinY()));
      int real_w, real_h;
      csRef<iTextureHandle> tex = persistentData.QueryUnusedTexture (txt_w, txt_h,
		  real_w, real_h);
		  
      if (renderTree.IsDebugFlagEnabled (persistentData.dbgShowPortalTextures))
        renderTree.AddDebugTexture (tex, (float)real_w/(float)real_h);
		
      iCamera* cam = rview->GetCamera();
      // Create a new view
      csRef<CS::RenderManager::RenderView> newRenderView;
      csRef<iCustomMatrixCamera> newCam (rview->GetEngine()->CreateCustomMatrixCamera (cam));
      iCamera* inewcam = newCam->GetCamera();
      newRenderView = renderTree.GetPersistentData().renderViews.GetRenderView (rview, portal, inewcam);
      newRenderView->SetEngine (rview->GetEngine ());
      newRenderView->SetOriginalCamera (rview->GetOriginalCamera ());
	
      if (portalFlags.Check (CS_PORTAL_WARP))
      {
	SetupWarp (inewcam, holder.meshWrapper->GetMovable(), portal);
      }

      SetupProjectionShift (newCam, inewcam, sb_minX, sb_minY, txt_h,
                            real_w, real_h, screenW, screenH);
      FudgeTargetCamera (inewcam, cam,
                         portal, portalFlags, count, portalVerts2d, portalVerts3d,
                         screenW, screenH);
	
      // Add a new context with the texture as the target
      // Setup simple portal
      newRenderView->SetLastPortal (portal);
      newRenderView->SetPreviousSector (rview->GetThisSector ());
      newRenderView->SetThisSector (sector);
      newRenderView->SetViewDimensions (real_w, real_h);
      /* @@@ FIXME Without the +1 pixels of the portal stay unchanged upon
       * rendering */
      csBox2 clipBox (0, real_h - (txt_h+1), txt_w+1, real_h);
      csRef<iClipper2D> newView (CreateBoxClipper (clipBox));
      /* @@@ Consider PolyClipper?
	A box has an advantage when the portal tex is rendered
	distorted: texels from outside the portal area still have a
	good color. May not be the case with a (more exact) poly
	clipper. */
      newRenderView->SetClipper (newView);

      typename RenderTreeType::ContextNode* portalCtx =
		renderTree.CreateContext (newRenderView);
      portalCtx->renderTargets[rtaColor0].texHandle = tex;

      portalCtx->drawFlags |= CSDRAW_CLEARSCREEN;

      // Setup the new context
      ContextSetupData newSetup (portalCtx);
      contextFunction (*portalCtx, newSetup);

      // Synthesize a render mesh for the portal plane
      csRef<csShaderVariableContext> svc;
      svc.AttachNew (new csShaderVariableContext);
      csRef<csShaderVariable> svTexPortal =
		svc->GetVariableAdd (persistentData.svNameTexPortal);
      svTexPortal->SetValue (tex);

      bool meshCreated;
      csRenderMesh* rm = renderTree.GetPersistentData().rmHolder.GetUnusedMesh (
		  meshCreated, rview->GetCurrentFrameNumber());
      SetupPortalRM (rm, portal, sector, count, rview);
      rm->buffers = GetPortalBuffers (count, portalVerts2d, portalVerts3d,
                                      true, txt_h, real_w, real_h,
                                      sb_minX, sb_minY);
      rm->variablecontext = svc;
	
      typename RenderTreeType::MeshNode::SingleMesh sm;
      sm.meshObjSVs = 0;

      CS::Graphics::RenderPriority renderPrio =
          holder.meshWrapper->GetRenderPriority ();
      context.AddRenderMesh (rm, renderPrio, sm);
    }

  };

} // namespace RenderManager
} // namespace CS

#endif // __CS_CSPLUGINCOMMON_RENDERMANAGER_CONTEXT_H__

