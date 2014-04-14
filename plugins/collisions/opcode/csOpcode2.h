/*
  Copyright (C) 2011 by Liu Lu

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

#ifndef __CS_OPCODE_COLLISION_H__
#define __CS_OPCODE_COLLISION_H__

#include "iutil/comp.h"
#include "csutil/scf.h"
#include "csutil/nobjvec.h"
#include "csutil/scf_implementation.h"
#include "ivaria/collisions.h"
#include "iengine/sector.h"
#include "iengine/movable.h"
#include "csutil/csobject.h"
#include "Opcode.h"

CS_PLUGIN_NAMESPACE_BEGIN (Opcode2)
{
class csOpcodeCollisionSystem;
class csOpcodeCollisionObject;

class csOpcodeCollisionSector : public scfImplementationExt1<
  csOpcodeCollisionSector, csObject, CS::Collisions::iCollisionSector>
{

  friend class csOpcodeCollisionObject;
  Opcode::AABBTreeCollider TreeCollider;
  Opcode::RayCollider RayCol;
  Opcode::BVTCache ColCache;

  struct CollisionPortal
  {
    iPortal* portal;
    csOpcodeCollisionObject* ghostPortal1;
  };

  class CollisionGroupVector : public csArray<CS::Collisions::CollisionGroup>
  {
  public:
    CollisionGroupVector () : csArray<CS::Collisions::CollisionGroup> () {}
    static int CompareKey (CS::Collisions::CollisionGroup const& item,
      char const* const& key)
    {
      return strcmp (item.name.GetData (), key);
    }
    static csArrayCmp<CS::Collisions::CollisionGroup, char const*>
      KeyCmp(char const* k)
    {
      return csArrayCmp<CS::Collisions::CollisionGroup, char const*> (k,CompareKey);
    }
  };

  CollisionGroupVector collGroups;
  CS::Collisions::CollisionGroupMask allFilter; 
  size_t systemFilterCount;

  csRef<iSector> sector;
  csOpcodeCollisionSystem* sys;
  csVector3 gravity;
  csRefArrayObject<csOpcodeCollisionObject> collisionObjects;
  csArray<CollisionPortal> portals;
  csArray<int> collision_faces;
  CS::Collisions::CollisionData curCollisionData;

public:
  csOpcodeCollisionSector (csOpcodeCollisionSystem* sys);
  virtual ~csOpcodeCollisionSector ();

  virtual iObject* QueryObject () {return (iObject*) this;}
  
  //iCollisionSector
  virtual void SetGravity (const csVector3& v);
  virtual csVector3 GetGravity () const {return gravity;}

  virtual void AddCollisionObject(CS::Collisions::iCollisionObject* object);
  virtual void RemoveCollisionObject(CS::Collisions::iCollisionObject* object);

  virtual size_t GetCollisionObjectCount () {return collisionObjects.GetSize ();}
  virtual CS::Collisions::iCollisionObject* GetCollisionObject (size_t index);
  virtual CS::Collisions::iCollisionObject* FindCollisionObject (const char* name);

  virtual void AddPortal(iPortal* portal, const csOrthoTransform& meshTrans);
  virtual void RemovePortal(iPortal* portal);

  virtual void SetSector(iSector* sector) {this->sector = sector;}
  virtual iSector* GetSector(){return sector;}

  virtual CS::Collisions::HitBeamResult HitBeam(const csVector3& start, 
    const csVector3& end);

  virtual CS::Collisions::HitBeamResult HitBeamPortal(const csVector3& start, 
    const csVector3& end);

  virtual CS::Collisions::CollisionGroup& CreateCollisionGroup (const char* name);
  virtual CS::Collisions::CollisionGroup& FindCollisionGroup (const char* name);

  virtual void SetGroupCollision (const char* name1,
    const char* name2, bool collide);
  virtual bool GetGroupCollision (const char* name1,
    const char* name2);

  virtual bool CollisionTest(CS::Collisions::iCollisionObject* object, 
    csArray<CS::Collisions::CollisionData>& collisions);

  virtual void AddCollisionActor (CS::Collisions::iCollisionActor* actor);
  virtual void RemoveCollisionActor ();
  virtual CS::Collisions::iCollisionActor* GetCollisionActor ();

  void AddMovableToSector (CS::Collisions::iCollisionObject* obj);

  void RemoveMovableFromSector (CS::Collisions::iCollisionObject* obj);

  CS::Collisions::HitBeamResult HitBeamCollider (Opcode::Model* model, Point* vertholder, udword* indexholder,
    const csOrthoTransform& trans, const csVector3& start, const csVector3& end, float& depth);

  CS::Collisions::HitBeamResult HitBeamTerrain (csOpcodeCollisionObject* terrainObj, 
    const csVector3& start, const csVector3& end, float& depth);

  CS::Collisions::HitBeamResult HitBeamObject (csOpcodeCollisionObject* object,
    const csVector3& start, const csVector3& end, float& depth);

  bool CollideDetect (Opcode::Model* modelA, Opcode::Model* modelB,
    const csOrthoTransform& transA, const csOrthoTransform& transB);

  void GetCollisionData (Opcode::Model* modelA, Opcode::Model* modelB,
    Point* vertholderA, Point* vertholderB,
    udword* indexholderA, udword* indexholderB,
    const csOrthoTransform& transA, const csOrthoTransform& transB);

  bool CollideObject (csOpcodeCollisionObject* objA, csOpcodeCollisionObject* objB, 
    csArray<CS::Collisions::CollisionData>& collisions);

  bool CollideTerrain (csOpcodeCollisionObject* objA, csOpcodeCollisionObject* objB, 
    csArray<CS::Collisions::CollisionData>& collisions);
};

class csOpcodeCollisionSystem : public scfImplementation2<
  csOpcodeCollisionSystem, CS::Collisions::iCollisionSystem,
  iComponent>
{
friend class csOpcodeCollider;
private:
  iObjectRegistry* object_reg;
  csRefArrayObject<csOpcodeCollisionSector> collSectors;
  csStringID baseID;
  csStringID colldetID;

public:
  csOpcodeCollisionSystem (iBase* iParent);
  virtual ~csOpcodeCollisionSystem ();

  // iComponent
  virtual bool Initialize (iObjectRegistry* object_reg);

  // iCollisionSystem
  virtual void SetInternalScale (float scale);
  virtual csRef<CS::Collisions::iColliderConvexMesh> CreateColliderConvexMesh (
    iMeshWrapper* mesh, bool simplify = false);
  virtual csRef<CS::Collisions::iColliderConcaveMesh> CreateColliderConcaveMesh (iMeshWrapper* mesh);
  virtual csRef<CS::Collisions::iColliderConcaveMeshScaled> CreateColliderConcaveMeshScaled
    (CS::Collisions::iColliderConcaveMesh* collider, csVector3 scale);
  virtual csRef<CS::Collisions::iColliderCylinder> CreateColliderCylinder (float length, float radius);
  virtual csRef<CS::Collisions::iColliderBox> CreateColliderBox (const csVector3& size);
  virtual csRef<CS::Collisions::iColliderSphere> CreateColliderSphere (float radius);
  virtual csRef<CS::Collisions::iColliderCapsule> CreateColliderCapsule (float length, float radius);
  virtual csRef<CS::Collisions::iColliderCone> CreateColliderCone (float length, float radius);
  virtual csRef<CS::Collisions::iColliderPlane> CreateColliderPlane (const csPlane3& plane);
  virtual csRef<CS::Collisions::iColliderTerrain> CreateColliderTerrain (iTerrainSystem* terrain,
    float minHeight = 0, float maxHeight = 0);

  virtual csRef<CS::Collisions::iCollisionObject> CreateCollisionObject ();
  virtual csRef<CS::Collisions::iCollisionActor> CreateCollisionActor ();
  virtual csRef<CS::Collisions::iCollisionSector> CreateCollisionSector ();
  virtual CS::Collisions::iCollisionSector* FindCollisionSector (const char* name);

  virtual void DecomposeConcaveMesh (CS::Collisions::iCollisionObject* object,
    iMeshWrapper* mesh, bool simplify = false); 

  static void OpcodeReportV (int severity, const char* message, 
    va_list args);

public:
  static iObjectRegistry* rep_object_reg;
};
}
CS_PLUGIN_NAMESPACE_END (Opcode2)
#endif
