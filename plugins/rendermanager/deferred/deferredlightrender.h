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

#ifndef __DEFERREDLIGHTRENDER_H__
#define __DEFERREDLIGHTRENDER_H__

#include "cssysdef.h"

#include "ivideo/graph3d.h"

#include "cstool/genmeshbuilder.h"
#include "cstool/normalcalc.h"

#include "csgfx/normalmaptools.h"

#include "csutil/cfgacc.h"

#include "igeom/trimesh.h"

#include "gbuffer.h"

CS_PLUGIN_NAMESPACE_BEGIN(RMDeferred)
{

  /**
   * Returns the given lights direction (only valid for spot and directional lights).
   */
  inline csVector3 GetLightDir(iLight* light)
  {
    csReversibleTransform T(light->GetMovable()->GetFullTransform());
    csVector3 d(T.This2OtherRelative(csVector3(0.0f, 0.0f, 1.0f)));

    return d.Unit();
  }

  /**
   * Creates a transform that will transform a 1x1x1 cube centered at the origin
   * to match the given bounding box (assumed to be in world space).
   */
  inline csReversibleTransform CreateLightTransform(iLight* light)
  {
    // get maximum of local light bbox - it has exactly the scales we need
    const csVector3& scales = light->GetLocalBBox().Max();

    // create scaling matrix
    csMatrix3 scale(
      scales.x, 0, 0,
      0, scales.y, 0,
      0, 0, scales.z);

    // create light2object transform
    csReversibleTransform trans(scale, csVector3(0));

    // assemble final object2world transform
    return trans.GetInverse() * light->GetMovable()->GetFullTransform();
  }

  /**
   * Returns true if the given point is inside the volume of the given spot light.
   */
  inline bool IsPointInsideSpotLight(const csVector3& p, iLight* light)
  {
    csVector3 pos(light->GetMovable()->GetFullPosition());
    float range = light->GetCutoffDistance();

    float inner, outer;
    light->GetSpotLightFalloff(inner, outer);

    // Gets the spot light direction.
    csVector3 d(GetLightDir(light));
    csVector3 u(p - pos);

    float dot_ud = u * d;
    if (dot_ud <= 0.0f || dot_ud >= range)
      return false;

    float cang = dot_ud / u.Norm();
    if (cang < outer)
      return true;

    return false;
  }

  /**
   * Returns true if the given point is inside the volume of the given light.
   */
  inline bool IsPointInsideLight(const csVector3& p, iLight* light, const csReversibleTransform& trans)
  {
    // get light space position
    csVector3 pos(trans * p);

    static const csBox3 unitBox(-1,-1,0,1,1,1);

    // different checks for different light types
    switch(light->GetType())
    {
    case CS_LIGHT_POINTLIGHT:
      return csSquaredDist::PointPoint(csVector3(0), pos) <= 1;
    case CS_LIGHT_DIRECTIONAL:
      return unitBox.In(pos);
    case CS_LIGHT_SPOTLIGHT:
      {
	// cutoff check
	if(pos.z < 0.0f || pos.z > 1.0f)
	  return false;

	// get outer falloff angle
	float inner, outer;
	light->GetSpotLightFalloff(inner, outer);

	// angle check
	return pos.z*pos.z/pos.SquaredNorm() <= outer;
      }
    default:
      CS_ASSERT(false);
    };

    return false;
  }

  // @@@TODO: add new comment explaining how it works
  template<typename ShadowHandler>
  class DeferredLightRenderer
  {
  public:

    /**
     * Data used by the light renderer that needs to persist over multiple 
     * frames. Render managers must store an instance of this class and 
     * provide it to the helper upon instantiation.
     */
    struct PersistentData : public scfImplementation1<PersistentData,
						      iLightCallback>
    {
      // typedefs
      struct ClipVolume
      {
	bool init;
	bool valid;
	long shapeNumber;
	csRenderMesh mesh;

	ClipVolume() : init(false)
	{
	}
      };

      typedef csHash<ClipVolume, iLight*> ClipVolumeHash;

      // object registry
      iObjectRegistry* objReg;

      // shader manager - only used during initialization
      csRef<iShaderManager> shaderManager;

      // loader - only used during initialization
      csRef<iLoader> loader;

      // our message ID
      const char* msgID;

      /* Mesh used for drawing point lights. Assumed to be center at the 
       * origin with a radius of 1. */
      csRenderMesh sphereMesh; 

      /* Mesh used for drawing spot lights. Assumed to be a cone with a 
       * height and radius of 1, aligned with the positive y-axis, and its
       * base centered at the origin. */
      csRenderMesh coneMesh;

      /* Mesh and material used for drawing directional lights. Assumed to be
       * a 1x1x1 box centered at the origin. */
      csRenderMesh boxMesh;

      /* Mesh and material used for drawing ambient light. Assumed to be in the
       * xy plane with the bottom left corner at the origin with a width and 
       * height equal to the screens width and height. */
      csRenderMesh quadMesh;

      // shaders used for lighting computations
      csRef<iShader> pointShader;
      csRef<iShader> spotShader;
      csRef<iShader> directionalShader;
      csRef<iShader> depthShader;
      csRef<iShader> outputShader;
      csRef<iShader> zOnlyShader;

      // shader variables used for lighting computations
      csRef<csShaderVariable> lightPos;
      csRef<csShaderVariable> lightDir;
      csRef<csShaderVariable> scale;

      // shader for drawing light volumes
      csRef<iShader> lightVolumeShader;

      // shader variable for volume color
      csRef<csShaderVariable> lightVolumeColor;

      // object model ID
      csStringID clipID;

      // cached clip volumes
      ClipVolumeHash clipVolumes;

      // persistent shadow data - we query deferred shadow data from it
      typename ShadowHandler::PersistentData* shadowPersist;
      csRef<csShaderVariable> shadowSpread;
      bool doShadows;

      PersistentData() : scfImplementation1<PersistentData, iLightCallback>(this)
      {
      }

      // loads a shader and issues a warning if it fails
      iShader* LoadShader(const char* path, const char* name = nullptr)
      {
	// shader we want to get
	iShader* shader = name ? shaderManager->GetShader(name) : nullptr;

	// not present, yet
	if(!shader)
	{
	  // try to load it
	  shader = loader->LoadShader(path);
	  if(!shader)
	  {
	    // failed - report error
	    csReport(objReg, CS_REPORTER_SEVERITY_ERROR, msgID, "Failed to load shader from %s", path);
	  }
	}

	// bail out if we didn't get any in debug mode
	CS_ASSERT(shader);

	// return shader
	return shader;
      }

      // builds a rendermesh from a trimesh given via dirty access arrays
      csRenderMesh CreateRenderMesh(csDirtyAccessArray<csVector3>& vertices,
				    csDirtyAccessArray<csVector3>& normals,
				    csDirtyAccessArray<csTriangle>& triangles)
      {
	// allocate rendermesh
	csRenderMesh mesh;

	// create buffer holder
	mesh.buffers.AttachNew(new csRenderBufferHolder);

	// copy position buffer
	{
	  // create buffer
	  csRef<iRenderBuffer> buffer = csRenderBuffer::CreateRenderBuffer(
	    vertices.GetSize(), CS_BUF_STATIC, CS_BUFCOMP_FLOAT, 3);

	  // populate it
	  buffer->CopyInto(vertices.GetArray(), vertices.GetSize());

	  // attach it
	  mesh.buffers->SetRenderBuffer(CS_BUFFER_POSITION, buffer);
	}

	// copy normal buffer
	{
	  // calculate normals if not present
	  if(normals.IsEmpty())
	  {
	    csNormalCalculator::CalculateNormals(vertices, triangles, normals, false);
	  }

	  // create buffer
	  csRef<iRenderBuffer> buffer = csRenderBuffer::CreateRenderBuffer(
	    normals.GetSize(), CS_BUF_STATIC, CS_BUFCOMP_FLOAT, 3);

	  // populate it
	  buffer->CopyInto(normals.GetArray(), normals.GetSize());

	  // attach it
	  mesh.buffers->SetRenderBuffer(CS_BUFFER_NORMAL, buffer);
	}

	// copy index buffer
	{
	  // go over triMesh and find min/max indices
	  size_t rangeMin = (size_t)~0;
	  size_t rangeMax = 0;
	  csTriangle* tris = triangles.GetArray();
	  size_t numTris = triangles.GetSize();
	  size_t numIndex = 3 * numTris;
	  for(size_t t = 0; t < numTris; ++t)
	  {
	    for(size_t i = 0; i < 3; ++i)
	    {
	      size_t index = (size_t)tris[t][i];
	      rangeMin = csMin(index, rangeMin);
	      rangeMax = csMax(index, rangeMax);
	    }
	  }

	  // create buffer
	  csRef<iRenderBuffer> buffer = csRenderBuffer::CreateIndexRenderBuffer(
	    numIndex, CS_BUF_STATIC, CS_BUFCOMP_UNSIGNED_INT, rangeMin, rangeMax);

	  // populate it
	  buffer->CopyInto(tris, numIndex);

	  // attach it
	  mesh.buffers->SetRenderBuffer(CS_BUFFER_INDEX, buffer);

	  // set mesh type
	  mesh.meshtype = CS_MESHTYPE_TRIANGLES;

	  // set index range
	  mesh.indexstart = 0;
	  mesh.indexend = (uint)numIndex;
	}

	// set clipping options
	mesh.clip_plane = CS_CLIP_NOT;
	mesh.clip_portal = CS_CLIP_NOT;
	mesh.clip_z_plane = CS_CLIP_NOT;

	// no material
	mesh.material = nullptr;

	return mesh;
      }

      csRenderMesh* GetClipVolume(iLight* light)
      {
	// get object model
	iObjectModel* objectModel = light->QuerySceneNode()->GetObjectModel();

	// check whether it's valid - we can't work without an object model
	if(objectModel)
	{
	  // try to get a cached result
	  ClipVolume& cached = clipVolumes.GetOrCreate(light);
	  const long shapeNumber = objectModel->GetShapeNumber();
	  if(cached.init && cached.shapeNumber == shapeNumber)
	  {
	    // got one
	    return cached.valid ? &cached.mesh : nullptr;
	  }
	  else
	  {
	    // new record
	    cached.init = true;
	    light->SetLightCallback(this);
	  }

	  // no cached result - update
	  cached.shapeNumber = shapeNumber;

	  // get triangle data
	  iTriangleMesh* triMesh = objectModel->GetTriangleData(clipID);

	  // no triangle data means no clip volume
	  if(triMesh)
	  {
	    // cosntruct temporary arrays
	    csDirtyAccessArray<csVector3> vertices;
	    csDirtyAccessArray<csVector3> normals;
	    csDirtyAccessArray<csTriangle> triangles;

	    size_t triCount = triMesh->GetTriangleCount();
	    size_t vertexCount = triMesh->GetVertexCount();

	    // allocate big enough buffer
	    vertices.SetSize(vertexCount);
	    triangles.SetSize(triCount);

	    // copy data
	    memcpy(vertices.GetArray(), triMesh->GetVertices(), vertexCount * sizeof(csVector3));
	    memcpy(triangles.GetArray(), triMesh->GetTriangles(), triCount * sizeof(csTriangle));

	    // normals are calculated by CreateRenderMesh

	    // create render mesh from tri mesh
	    cached.mesh = CreateRenderMesh(vertices, normals, triangles);
	    cached.valid = true;

	    return &cached.mesh;
	  }
	  else
	  {
	    cached.valid = false;
	  }

	  // fallthrough
	}

	// no clip volume
	return nullptr;
      }

      void UpdateNewFrame()
      {
      }

      /**
       * Initialize persistent data, must be called once before using the
       * light renderer.
       */
      bool Initialize(iObjectRegistry* objRegistry, typename ShadowHandler::PersistentData& shadowPersistent, bool useShadows, bool deferredFull)
      {
	// set our mesasge id
        msgID = "crystalspace.rendermanager.deferred.lightrender";

	// save object registry
	objReg = objRegistry;

	// set shadow data
	shadowPersist = &shadowPersistent;
	doShadows = useShadows;

	// query some plugins we'll need
	shaderManager = csQueryRegistry<iShaderManager>(objRegistry);
        loader = csQueryRegistry<iLoader>(objRegistry);
        csRef<iGraphics3D> graphics3D = csQueryRegistry<iGraphics3D>(objRegistry);
        csRef<iStringSet> stringSet = csQueryRegistryTagInterface<iStringSet>(objRegistry, "crystalspace.shared.stringset");
        iShaderVarStringSet* svStringSet = shaderManager->GetSVNameStringset();

	// setup config accessor
        csConfigAccess cfg(objRegistry);

	// get clip ID
	clipID = stringSet->Request("clip");

	// get shaders - the paths should really be configurable
	csString basePath(deferredFull ? "/shader/deferred/full/" : "/shader/deferred/lighting/");
	// @@@TODO: make the shaders changeable via config
	pointShader = LoadShader(basePath + "point_light.xml");
	spotShader = LoadShader(basePath + "spot_light.xml");
	directionalShader = LoadShader(basePath + "directional_light.xml");
	depthShader = LoadShader(basePath + "depth.xml");
	outputShader = deferredFull ? LoadShader("/shader/deferred/full/use_buffer.xml") : nullptr;
	lightVolumeShader = LoadShader("/shader/deferred/dbg_light_volume.xml");
	zOnlyShader = LoadShader("/shader/early_z/z_only.xml", "z_only");

	// populate shader variables
	scale = shaderManager->GetVariableAdd(svStringSet->Request("gbuffer scaleoffset"));
        lightVolumeColor = lightVolumeShader->GetVariableAdd(svStringSet->Request("static color"));
	lightPos.AttachNew(new csShaderVariable(svStringSet->Request("light position view")));
	lightDir.AttachNew(new csShaderVariable(svStringSet->Request("light direction view")));
	shadowSpread.AttachNew(new csShaderVariable(svStringSet->Request("light shadow spread")));

	// arrays for mesh building
	csDirtyAccessArray<csVector2> texels;
	csDirtyAccessArray<csVector3> vertices;
	csDirtyAccessArray<csVector3> normals;
	csDirtyAccessArray<csTriangle> triangles;

        // build box
	CS::Geometry::Primitives::GenerateBox(csBox3(-1,-1,0,1,1,1), vertices, texels, normals, triangles);

	// create box mesh
	boxMesh = CreateRenderMesh(vertices, normals, triangles);
	boxMesh.db_mesh_name = "crystalspace.rendermanager.deferred.lightrender.box";

        // build sphere
        csEllipsoid ellipsoid(csVector3(0.0f, 0.0f, 0.0f), csVector3(1.0f, 1.0f, 1.0f));
	int sphereDetail = cfg->GetInt("RenderManager.Deferred.SphereDetail", 16);
	CS::Geometry::Primitives::GenerateSphere(ellipsoid, sphereDetail, vertices, texels, normals, triangles);

	// create sphere mesh
	sphereMesh = CreateRenderMesh(vertices, normals, triangles);
	sphereMesh.db_mesh_name = "crystalspace.rendermanager.deferred.lightrender.sphere";

        // build cone
        int coneDetail = cfg->GetInt("RenderManager.Deferred.ConeDetail", 16);
	CS::Geometry::Primitives::GenerateCone(1.0f, 1.0f, coneDetail, vertices, texels, normals, triangles);

	// hard transform cone to have have base at z=1 and top at z=0
	// (GenerateCone has base at y=0 and top at y=1)
	{
	  csMatrix3 t2o(
	    1,  0,  0,
	    0,  0, -1,
	    0, -1,  0);
	  csVector3 v_t2o(0,0,1);
	  csOrthoTransform trans(t2o, v_t2o);

	  for(size_t i = 0; i < vertices.GetSize(); ++i)
	  {
	    vertices[i] /= trans;
	    normals[i] = trans.This2OtherRelative(normals[i]);
	  }
	}

	// create cone mesh
	coneMesh = CreateRenderMesh(vertices, normals, triangles);
	coneMesh.db_mesh_name = "crystalspace.rendermanager.deferred.lightrender.cone";

        // build quad
	CS::Geometry::Primitives::GenerateQuad(csVector3(-1,-1,0), csVector3(-1,1,0), csVector3(1,1,0), csVector3(1,-1,0),
					       vertices, texels, normals, triangles);

	// create quad mesh
	quadMesh = CreateRenderMesh(vertices, normals, triangles);
	quadMesh.db_mesh_name = "crystalspace.rendermanager.deferred.lightrender.quad";

	// set rendermodes for quad mesh as they're constant
	quadMesh.mixmode = CS_FX_COPY;
	quadMesh.alphaType = csAlphaMode::alphaNone;
	quadMesh.do_mirror = false;
	quadMesh.cullMode = CS::Graphics::cullDisabled;

	// release plugins we don't need anymore
	loader.Invalidate();

	return true;
      }

      // iLightCallback
      void OnColorChange(iLight* light, const csColor& newcolor) { }
      void OnPositionChange(iLight* light, const csVector3& newpos) { }
      void OnSectorChange(iLight* light, iSector* newsector) { }
      void OnRadiusChange(iLight* light, float newradius) { }
      void OnDestroy(iLight* light)
      {
        clipVolumes.DeleteAll(light);
      }
      void OnAttenuationChange(iLight* light, int newatt) { }
    };

    DeferredLightRenderer(iGraphics3D* g3d, 
                          iShaderManager* shaderMgr,
                          CS::RenderManager::RenderView* rview,
                          GBuffer& gbuffer,
                          PersistentData& persistent)
      : 
    graphics3D(g3d),
    shaderMgr(shaderMgr),
    rview(rview),
    persistentData(persistent),
    shadowHandler(*persistent.shadowPersist, rview)
    {
      gbuffer.UpdateShaderVars(shaderMgr);
    }

    ~DeferredLightRenderer() {}

    /**
     * Outputs the depth from the gbuffer.
     */
    void OutputDepth()
    {
      iShader* shader = persistentData.depthShader;

      DrawFullscreenQuad(shader, CS_ZBUF_FILL);
    }

    /**
     * Outputs the final results using the accumulation buffers and the gbuffer
     */
    void OutputResults()
    {
      iShader* shader = persistentData.outputShader;

      DrawFullscreenQuad(shader, CS_ZBUF_FILL);
    }

    /**
     * Renders a single light volume.
     */
    void OutputLightVolume(iLight* light, bool wireframe = true, float alpha = 0.5f)
    {
      csRenderMesh* obj = nullptr;

      switch(light->GetType())
      {
      case CS_LIGHT_POINTLIGHT:
        obj = &persistentData.sphereMesh;
        break;
      case CS_LIGHT_DIRECTIONAL:
        obj = &persistentData.boxMesh;
        break;
      case CS_LIGHT_SPOTLIGHT:
        obj = &persistentData.coneMesh;
        break;
      default:
        CS_ASSERT(false);
      };

      csColor4 color(light->GetColor());
      color.alpha = alpha;

      if(wireframe)
        graphics3D->SetEdgeDrawing(true);

      RenderLightVolume(light, obj, color);

      if(wireframe)
        graphics3D->SetEdgeDrawing(false);
    }

    /**
     * Renders a single light.
     */
    void operator()(iLight* light)
    {
      csRenderMesh* mesh = nullptr;
      iShader* shader = nullptr;

      switch(light->GetType())
      {
      case CS_LIGHT_POINTLIGHT:
	mesh = &persistentData.sphereMesh;
	shader = persistentData.pointShader;
        break;
      case CS_LIGHT_DIRECTIONAL:
	mesh = &persistentData.boxMesh;
	shader = persistentData.directionalShader;
        break;
      case CS_LIGHT_SPOTLIGHT:
	mesh = &persistentData.coneMesh;
	shader = persistentData.spotShader;
        break;
      default:
        CS_ASSERT(false);
      };

      CS_ASSERT(mesh);
      CS_ASSERT(shader);

      RenderLight(light, mesh, shader);
    }

  private:

    /**
     * Sets shader variables specific to the given light.
     */
    void SetupLightShaderVars(iLight* light, csShaderVariableStack& svStack)
    {
      const csReversibleTransform& world2camera = graphics3D->GetWorldToCamera();

      csLightType type = light->GetType();

      // Transform light position to view space.
      if(type == CS_LIGHT_POINTLIGHT || type == CS_LIGHT_SPOTLIGHT)
      {
	iMovable* movable = light->GetMovable();

	csShaderVariable* lightPosSV = persistentData.lightPos;

        csVector3 lightPos(movable->GetFullPosition() / world2camera);

        lightPosSV->SetValue(lightPos);
	svStack[lightPosSV->GetName()] = lightPosSV;
      }

      // Transform light direction to view space.
      if(type == CS_LIGHT_DIRECTIONAL || type == CS_LIGHT_SPOTLIGHT)
      {
	csShaderVariable* lightDirSV = persistentData.lightDir;

        csVector3 lightDir(GetLightDir(light));
        lightDir = world2camera.This2OtherRelative(lightDir);

        lightDirSV->SetValue(lightDir.Unit());
	svStack[lightDirSV->GetName()] = lightDirSV;
      }
    }

    /**
     * Renders a light volume.
     */
    void RenderLightVolume(iLight* light, csRenderMesh* obj, const csColor4& color)
    {
      // set light color
      persistentData.lightVolumeColor->SetValue(color);

      // get shader stack and shader
      csShaderVariableStack svStack = shaderMgr->GetShaderVariableStack();
      iShader *shader = persistentData.lightVolumeShader;

      // update shader stack
      svStack.Clear();
      shaderMgr->PushVariables(svStack);
      shader->PushVariables(svStack);

      // set mesh modes
      obj->cullMode = CS::Graphics::cullDisabled;
      obj->object2world = CreateLightTransform(light);
      obj->do_mirror = rview->GetCamera()->IsMirrored();
      obj->mixmode = CS_FX_ALPHA;

      // Draw the light mesh.
      DrawMesh(obj, shader, svStack, CS_ZBUF_NONE);
    }

    /**
     * Renders a light mesh using the given shader.
     */
    void RenderLight(iLight* light, csRenderMesh* obj, iShader* shader)
    {
      // Update shader stack.
      csShaderVariableStack svStack = shaderMgr->GetShaderVariableStack();
      iShaderVariableContext* lightSVContext = light->GetSVContext();

      // push shader variables
      svStack.Clear();
      shader->PushVariables(svStack);
      shaderMgr->PushVariables(svStack);
      lightSVContext->PushVariables(svStack);
      SetupLightShaderVars(light, svStack);

      // push shadow spread variable
      csShaderVariable* shadowSpreadSV = persistentData.shadowSpread;
      svStack[shadowSpreadSV->GetName()] = shadowSpreadSV;

      // get camera and mirroring
      iCamera* cam = rview->GetCamera();
      bool camMirrored = cam->IsMirrored();

      // set transform and mirror mode
      obj->object2world = CreateLightTransform(light);
      obj->do_mirror = camMirrored;

      csVector3 camPos(csVector3(0.0f, 0.0f, 1.0f) / cam->GetTransform());
      bool insideLight = IsPointInsideLight(camPos, light, obj->object2world);

      csRenderMesh* clipVolume = persistentData.GetClipVolume(light);

      bool maskStencil = clipVolume || !insideLight;

      if(maskStencil)
      {
	// use stencil masking
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_BEGIN);
      }

      if(clipVolume)
      {
	// start with all masked out
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_PASS1);
	obj->cullMode = CS::Graphics::cullFlipped;
	obj->mixmode = CS_FX_TRANSPARENT;
	DrawMesh(obj, persistentData.zOnlyShader, svStack);

	// set clip volume transform and mesh modes
	clipVolume->object2world = light->GetMovable()->GetFullTransform();
	clipVolume->do_mirror = camMirrored;
	clipVolume->cullMode = CS::Graphics::cullNormal;
	clipVolume->mixmode = CS_FX_TRANSPARENT;

        // mask out front faces that are behind geometry
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_PASS1);
	DrawMesh(clipVolume, persistentData.zOnlyShader, svStack);

	// mask in back faces that are behind geometry
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_PASS2);
	DrawMesh(clipVolume, persistentData.zOnlyShader, svStack);
      }

      if(!insideLight)
      {
	// mask out front faces that are behind geometry
	obj->cullMode = CS::Graphics::cullNormal;
	obj->mixmode = CS_FX_TRANSPARENT;
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_PASS1);
        DrawMesh(obj, persistentData.zOnlyShader, svStack);
      }

      if(maskStencil)
      {
	// use stencil test
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_USE);
      }

      // set cull and mix mode
      obj->cullMode = CS::Graphics::cullFlipped;
      obj->mixmode = CS_FX_ADD;

      // light doesn't cast shadows - draw without shadowing
      if(!persistentData.doShadows || light->GetFlags().Check(CS_LIGHT_NOSHADOWS))
      {
	// let the shader know we don't use shadows
	shadowSpreadSV->SetValue(0);

	// draw the light without shadows
	DrawMesh(obj, shader, svStack);
      }
      // shadow does cast shadows - draw with shadowing
      else
      {
	// we may have to render multiple times if the shadow map uses multiple projectors
	for(uint i = 0; i < shadowHandler.GetSublightNum(light); ++i)
	{
	  // setup shadow variables
	  csShaderVariableStack localStack(svStack);
	  int spread = shadowHandler.PushVariables(light, i, localStack);

	  // check whether we need to draw anything for this projector
	  if(spread > 0)
	  {
	    // set spread shader variable accordingly...
	    shadowSpreadSV->SetValue(spread);

	    // draw light with shadows
	    DrawMesh(obj, shader, localStack);
	  }
	}
      }

      if(maskStencil)
      {
	// clear stencil mask
	graphics3D->SetShadowState(CS_SHADOW_VOLUME_FINISH);
      }
    }

    /**
     * Draws a fullscreen quad using the supplied shader.
     */
    void DrawFullscreenQuad(iShader* shader, csZBufMode zmode)
    {
      // backup old projection and transform
      csReversibleTransform oldView(graphics3D->GetWorldToCamera());
      CS::Math::Matrix4 oldProj(graphics3D->GetProjectionMatrix());

      // set identity projection and transform
      graphics3D->SetWorldToCamera(csOrthoTransform());
      graphics3D->SetProjectionMatrix(CS::Math::Matrix4());

      // get empty SV stack
      csShaderVariableStack svStack = shaderMgr->GetShaderVariableStack();
      svStack.Clear();
      shader->PushVariables(svStack);
      shaderMgr->PushVariables(svStack);

      // draw quad
      DrawMesh(&persistentData.quadMesh, shader, svStack, zmode);

      // restore old projection and transform
      graphics3D->SetWorldToCamera(oldView);
      graphics3D->SetProjectionMatrix(oldProj);
    }

    void DrawMesh(csRenderMesh* m, iShader* shader, csShaderVariableStack& svStack, csZBufMode zmode = CS_ZBUF_INVERT)
    {
      m->z_buf_mode = zmode;

      // copy mesh modes as they may be modified by shader setup
      // which we don't want
      CS::Graphics::RenderMeshModes modes = *m;

      const size_t ticket = shader->GetTicket(modes, svStack);
      const size_t numPasses = shader->GetNumberOfPasses(ticket);

      for(size_t p = 0; p < numPasses; p++)
      {
        if(!shader->ActivatePass(ticket, p)) 
          continue;

        if(!shader->SetupPass(ticket, m, modes, svStack))
          continue;

        graphics3D->DrawMesh(m, *m, svStack);

        shader->TeardownPass(ticket);

        shader->DeactivatePass(ticket);
      }
    }

    iGraphics3D* graphics3D;
    iShaderManager* shaderMgr;
    CS::RenderManager::RenderView* rview;

    PersistentData& persistentData;
    ShadowHandler shadowHandler;
  };

  /**
   * A helper functor that will draw the light volume for each given light.
   */
  template<typename LightRenderType>
  class LightVolumeRenderer
  {
  public:

    LightVolumeRenderer(LightRenderType& render, bool wireframe, float alpha)
      :
    render(render),
    wireframe(wireframe),
    alpha(alpha)
    {}

    void operator()(iLight* light)
    {
      render.OutputLightVolume(light, wireframe, alpha);
    }

    LightRenderType& render;
    bool wireframe;
    float alpha;
  };

}
CS_PLUGIN_NAMESPACE_END(RMDeferred)

#endif // __DEFERREDLIGHTRENDER_H__
