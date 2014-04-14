/*
    Copyright (C) 2007-2008 by Marten Svanfeldt
              (C) 2008 by Frank Richter

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

#include "csplugincommon/rendermanager/portalsetup.h"

namespace CS
{
  namespace RenderManager
  {
#define SPSBPD  StandardPortalSetup_Base::PersistentData
  
    bool SPSBPD::PortalBufferConstraint::IsEqual (
      const PortalBuffers& b1, const PortalBuffers& b2)
    {
      size_t s1 = b1.coordBuf->GetElementCount ();
      size_t s2 = b2.coordBuf->GetElementCount ();

      if (s1 == s2) return true;
      return false;
    }

    bool SPSBPD::PortalBufferConstraint::IsLargerEqual (
      const PortalBuffers& b1, const PortalBuffers& b2)
    {
      size_t s1 = b1.coordBuf->GetElementCount ();
      size_t s2 = b2.coordBuf->GetElementCount ();

      if (s1 >= s2) return true;
      return false;
    }

    bool SPSBPD::PortalBufferConstraint::IsEqual (
      const PortalBuffers& b1, const KeyType& s2)
    {
      size_t s1 = b1.coordBuf->GetElementCount ();

      if (s1 == s2) return true;
      return false;
    }

    bool SPSBPD::PortalBufferConstraint::IsLargerEqual (
      const PortalBuffers& b1, const KeyType& s2)
    {
      size_t s1 = b1.coordBuf->GetElementCount ();

      if (s1 >= s2) return true;
      return false;
    }

    bool SPSBPD::PortalBufferConstraint::IsLargerEqual (
      const KeyType& s1, const PortalBuffers& b2)
    {
      size_t s2 = b2.coordBuf->GetElementCount ();

      if (s1 >= s2) return true;
      return false;
    }
    
    //-----------------------------------------------------------------------

    void SPSBPD::csBoxClipperCached::operator delete (void* p, void* q)
    {
      csBoxClipperCached* bcc = reinterpret_cast<csBoxClipperCached*> (p);
      BoxClipperCacheRefCounted* bccCache (bcc->owningCache);
      bccCache->FreeCachedClipper (bcc);
      bccCache->DecRef();
    }
    void SPSBPD::csBoxClipperCached::operator delete (void* p)
    {
      csBoxClipperCached* bcc = reinterpret_cast<csBoxClipperCached*> (p);
      BoxClipperCacheRefCounted* bccCache (bcc->owningCache);
      bccCache->FreeCachedClipper (bcc);
      bccCache->DecRef();
    }
  
    //-----------------------------------------------------------------------

    void SPSBPD::BoxClipperCacheRefCounted::FreeCachedClipper (csBoxClipperCached* bcc)
    {
      CS::Utility::ResourceCache::ReuseConditionFlagged::StoredAuxiliaryInfo*
	reuseAux = GetReuseAuxiliary (
	  reinterpret_cast<csBoxClipperCachedStore*> (bcc));
      reuseAux->reusable = true;
    }
    
    SPSBPD::PersistentData(int textCachOptions) :
      bufCache (CS::Utility::ResourceCache::ReuseConditionAfterTime<uint> (),
	CS::Utility::ResourceCache::PurgeConditionAfterTime<uint> (10000)),
      texCache (csimg2D, "rgb8", // @@@ FIXME: Use same format as main view ...
	CS_TEXTURE_3D | CS_TEXTURE_NOMIPMAPS | CS_TEXTURE_CLAMP,
	"target", textCachOptions), 
      fixedTexCacheWidth (0), fixedTexCacheHeight (0)
    {
      bufCache.agedPurgeInterval = 5000;
      boxClipperCache.AttachNew (new BoxClipperCacheRefCounted (
        CS::Utility::ResourceCache::ReuseConditionFlagged (),
	CS::Utility::ResourceCache::PurgeConditionAfterTime<uint> (10000)));
      boxClipperCache->agedPurgeInterval = 5000;
    }

    void SPSBPD::Initialize (iShaderManager* shmgr, iGraphics3D* g3d,
                             RenderTreeBase::DebugPersistent& dbgPersist)
    {
      svNameTexPortal =
	shmgr->GetSVNameStringset()->Request ("tex portal");
      texCache.SetG3D (g3d);
      
      dbgDrawPortalOutlines =
        dbgPersist.RegisterDebugFlag ("draw.portals.outline");
      dbgDrawPortalPlanes =
        dbgPersist.RegisterDebugFlag ("draw.portals.planes");
      dbgShowPortalTextures =
        dbgPersist.RegisterDebugFlag ("textures.portals");
    }
    
    //-----------------------------------------------------------------------

    void StandardPortalSetup_Base::PortalDebugDraw (RenderTreeBase& renderTree,
                                                    iPortal* portal,
                                                    size_t count, const csVector2* portalVerts2d,
                                                    const csVector3* portalVerts3d,
                                                    int screenH,
                                                    bool isSimple, int skipRec)
    {
      if (renderTree.IsDebugFlagEnabled (persistentData.dbgDrawPortalOutlines))
      {
        for (size_t i = 0; i < count; i++)
        {
          size_t next = (i+1)%count;
          csVector2 v1 (portalVerts2d[i]);
          csVector2 v2 (portalVerts2d[next]);
          v1.y = screenH - v1.y;
          v2.y = screenH - v2.y;
          renderTree.AddDebugLineScreen (v1, v2,
            isSimple ? csRGBcolor (0, 255, int(skipRec) * 255)
                          : csRGBcolor (255, 0, int (skipRec) * 255));
        }
      }
      if (renderTree.IsDebugFlagEnabled (persistentData.dbgDrawPortalPlanes))
      {
        csVector3 guessedCenter (0);
        const csVector3* pv = portal->GetWorldVertices();
        const int* pvi = portal->GetVertexIndices();
        size_t numOrgVerts = portal->GetVertexIndicesCount ();
        for (size_t n = 0; n < numOrgVerts; n++)
        {
          guessedCenter += pv[pvi[n]];
        }
        guessedCenter /= numOrgVerts;
        csTransform identity;
        renderTree.AddDebugPlane (portal->GetWorldPlane(), identity,
          isSimple ? csColor (0, 1, int (skipRec)) : csColor (1, 0, int (skipRec)),
          guessedCenter);
      }
    }

    csPtr<iClipper2D> StandardPortalSetup_Base::CreateBoxClipper(const csBox2& box)
    {
      csRef<iClipper2D> clipper;
      PersistentData::csBoxClipperCachedStore* bccstore;
      bccstore = persistentData.boxClipperCache->Query ();
      if (bccstore == 0)
      {
        PersistentData::csBoxClipperCachedStore dummy;
        bccstore = persistentData.boxClipperCache->AddActive (dummy);
      }
#include "csutil/custom_new_disable.h"
      clipper.AttachNew (new (bccstore) PersistentData::csBoxClipperCached (
        persistentData.boxClipperCache, box));
#include "csutil/custom_new_enable.h"
      persistentData.boxClipperCache->IncRef ();
      return csPtr<iClipper2D> (clipper);
    }

    void StandardPortalSetup_Base::SetupProjectionShift (iCustomMatrixCamera* newCam,
                                                         iCamera* inewcam,
                                                         int sb_minX, int sb_minY,
                                                         int txt_h,
                                                         int real_w, int real_h,
                                                         int screenW, int screenH)
    {
      /* Shift projection.
      
          What we want is to render the sector behind the portal, as seen
          from the current camera's viewpoint. The portal has a bounding
          rectangle. For reasons of efficiency we only want to render this
          rectangular region of the view. Also, the texture is only large
          enough to cover this rectangle, so the view needs to be offset
          to align the upper left corner of the portal rectangle with the
          upper left corner of the render target texture. This can be done
          by manipulating the projection matrix.
          
          The matrix 'projShift' below does:
          - Transform projection matrix from normalized space to screen
            space.
          - Translate the post-projection space so the upper left corner
            of the rectangle covering the the portal is in the upper left
            corner of the render target.
          - Transform projection matrix from (now) render target space to
            normalized space.
          (Each step can be represented as a matrix, and the matrix below
          is just the result of pre-concatenating these matrices.)
        */
    
      float irw = 1.0f/real_w;
      float irh = 1.0f/real_h;
      CS::Math::Matrix4 projShift (
        screenW*irw, 0, 0, irw * (screenW-2*sb_minX) - 1,
        0, screenH*irh, 0, irh * (screenH+2*((real_h - txt_h) - sb_minY)) - 1,
        0, 0, 1, 0,
        0, 0, 0, 1);
      
      newCam->SetProjectionMatrix (projShift * inewcam->GetProjectionMatrix());
    }
    
    void StandardPortalSetup_Base::FudgeTargetCamera (iCamera* inewcam, iCamera* cam,
                                                      iPortal* portal, const csFlags& portalFlags,
                                                      size_t count, const csVector2* portalVerts2d,
                                                      const csVector3* portalVerts3d,
                                                      int screenW, int screenH)
    {
      /* Visible cracks can occur on portal borders when the geometry
         behind the portal is supposed to fit seamlessly into geometry
         before the portal since the rendering of the target geometry
         may not exactly line up with the portal area on the portal
         texture.
         To reduce that effect the camera position in the target sector
         is somewhat fudged to move slightly into the target so that
         the rendered target sector geometry extends beyond the portal
         texture area. */
      {
        // - Find portal point with largest Z (pMZ)
        float maxz = 0;
        size_t maxc = 0;
        for (size_t c = 0; c < count; c++)
        {
          float z = portalVerts3d[c].z;
          if (z > maxz)
          {
            maxz = z;
            maxc = c;
          }
        }
        // - Find inverse perspective point of pMZ plus some offset (pMZ2)
        csVector4 zToPostProject (0, 0, maxz, 1.0f);
        zToPostProject = cam->GetProjectionMatrix() * zToPostProject;
        
        const CS::Math::Matrix4& inverseProj (cam->GetInvProjectionMatrix());
        csVector2 p (portalVerts2d[maxc]);
        p.x += 1.5f;
        p.y += 1.5f;
        p.x /= 0.5f*screenW;
        p.y /= 0.5f*screenH;
        p.x -= 1;
        p.y -= 1;
        csVector4 p_proj (p.x*zToPostProject.w, p.y*zToPostProject.w,
          zToPostProject.z, zToPostProject.w);
        csVector4 p_proj_inv = inverseProj * p_proj;
        csVector3 pMZ2 (p_proj_inv[0], p_proj_inv[1], p_proj_inv[2]);
        // - d = distance pMZ, pMZ2
        float d = sqrtf (csSquaredDist::PointPoint (portalVerts3d[maxc], pMZ2));
        // - Get portal target plane in target world space
        csVector3 portalDir;
        if (portalFlags.Check (CS_PORTAL_WARP))
        {
          portalDir = portal->GetWarp().Other2ThisRelative (
            portal->GetWorldPlane().Normal());
        }
        else
          portalDir = portal->GetWorldPlane().Normal();
        /* - Offset target camera into portal direction in target sector,
             amount of offset 'd' */
        csVector3 camorg (inewcam->GetTransform().GetOrigin());
        camorg += d * portalDir;
        inewcam->GetTransform().SetOrigin (camorg);
      }
    }
    
    csPtr<csRenderBufferHolder> StandardPortalSetup_Base::GetPortalBuffers (size_t count,
                                                                            const csVector2* portalVerts2d,
                                                                            const csVector3* portalVerts3d,
                                                                            bool withTCs,
                                                                            int txt_h,
                                                                            int real_w, int real_h,
                                                                            int sb_minX, int sb_minY)
    {
      PersistentData::PortalBuffers* bufs = persistentData.bufCache.Query (count);
        
      if (bufs == 0)
      {
        PersistentData::PortalBuffers newBufs;
        newBufs.coordBuf = csRenderBuffer::CreateRenderBuffer (count, CS_BUF_STREAM, CS_BUFCOMP_FLOAT, 3);
        newBufs.tcBuf = csRenderBuffer::CreateRenderBuffer (count, CS_BUF_STREAM, CS_BUFCOMP_FLOAT, 4);
        newBufs.indexBuf = csRenderBuffer::CreateIndexRenderBuffer (count, CS_BUF_STREAM,
                    CS_BUFCOMP_UNSIGNED_INT, 0, count-1);
        newBufs.holder.AttachNew (new csRenderBufferHolder);
        newBufs.holder->SetRenderBuffer (CS_BUFFER_INDEX, newBufs.indexBuf);
        newBufs.holder->SetRenderBuffer (CS_BUFFER_POSITION, newBufs.coordBuf);
        newBufs.holder->SetRenderBuffer (CS_BUFFER_TEXCOORD0, newBufs.tcBuf);
        bufs = persistentData.bufCache.AddActive (newBufs);
      }

      {
        csRenderBufferLock<csVector3> coords (bufs->coordBuf);
        for (size_t c = 0; c < count; c++)
        {
          coords[c].Set (portalVerts3d[c]);
        }
      }
      if (withTCs)
      {
        float xscale = (float)1.0f/(float)real_w;
        float yscale = (float)1.0f/(float)real_h;
        csRenderBufferLock<csVector4> tcoords (bufs->tcBuf);
        for (size_t c = 0; c < count; c++)
        {
          float z = portalVerts3d[c].z;
          const csVector2& p2 = portalVerts2d[c];
          tcoords[c].Set ((p2.x - sb_minX) * xscale * z,
            (txt_h - (p2.y - sb_minY)) * yscale * z, 0, z);
        }
      }
      {
        csRenderBufferLock<uint> indices (bufs->indexBuf);
        for (size_t c = 0; c < count; c++)
          *indices++ = uint (c);
      }
      
      return csPtr<csRenderBufferHolder> (bufs->holder);
    }
    
    csRenderMesh* StandardPortalSetup_Base::SetupPortalRM (csRenderMesh* rm,
                                                           iPortal* portal, iSector* sector,
                                                           size_t count, RenderView* rview)
    {
#ifdef CS_DEBUG
      bool created;
      csStringBase& nameStr = persistentData.stringHolder.GetUnusedData (
                created,  rview->GetCurrentFrameNumber());
      nameStr.Format ("[portal from %s to %s]",
                rview->GetThisSector()->QueryObject()->GetName(),
                sector->QueryObject()->GetName());
      rm->db_mesh_name = nameStr;
#else
      rm->db_mesh_name = "[portal]";
#endif
      iMaterialWrapper* mat = portal->GetMaterial ();
      rm->material = mat;
      rm->meshtype = CS_MESHTYPE_TRIANGLEFAN;
      rm->z_buf_mode = CS_ZBUF_USE;
      rm->mixmode = CS_MIXMODE_BLEND(ONE, ZERO);
      rm->indexstart = 0;
      rm->indexend = uint (count);
      rm->object2world = rview->GetCamera()->GetTransform();
      
      return rm;
    }
  } // namespace RenderManager
} // namespace CS

