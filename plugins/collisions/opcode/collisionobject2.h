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

#ifndef __CS_OPCODE_COLLISIONOBJECT_H__
#define __CS_OPCODE_COLLISIONOBJECT_H__

#include "iengine/camera.h"
#include "csOpcode2.h"
#include "colliders2.h"

CS_PLUGIN_NAMESPACE_BEGIN (Opcode2)
{
class csOpcodeCollisionSector;
class csOpcodeCollisionSystem;
class csOpcodeCollider;

class csOpcodeCollisionObject: public scfImplementationExt1<
  csOpcodeCollisionObject, csObject, CS::Collisions::iCollisionObject>
{
  friend class csOpcodeCollisionSector;
private:
  csOpcodeCollisionSystem* system;
  csOpcodeCollisionSector* sector;
  csWeakRef<iMovable> movable;
  csWeakRef<iCamera> camera;
  csRef<CS::Collisions::iCollider> collider;
  CS::Collisions::CollisionObjectType type;
  CS::Collisions::CollisionGroup collGroup;
  csRef<CS::Collisions::iCollisionCallback> collCb;
  csRefArray<csOpcodeCollisionObject> contactObjects;
  csOrthoTransform transform;

  bool insideWorld;
  bool isTerrain;

public:
  csOpcodeCollisionObject (csOpcodeCollisionSystem* sys);
  virtual ~csOpcodeCollisionObject ();

  virtual iObject* QueryObject (void) { return (iObject*) this; }
  virtual CS::Collisions::iCollisionObject* QueryCollisionObject () {return dynamic_cast<iCollisionObject*> (this);}
  virtual CS::Physics::iPhysicalBody* QueryPhysicalBody () {return NULL;}

  virtual void SetObjectType (CS::Collisions::CollisionObjectType type, bool forceRebuild = true);
  virtual CS::Collisions::CollisionObjectType GetObjectType () {return type;}

  virtual void SetAttachedMovable (iMovable* movable){this->movable = movable;}
  virtual iMovable* GetAttachedMovable (){return movable;}

  virtual void SetAttachedCamera (iCamera* camera){this->camera = camera;}
  virtual iCamera* GetAttachedCamera (){return camera;}

  virtual void SetTransform (const csOrthoTransform& trans);
  virtual csOrthoTransform GetTransform ();

  virtual void AddCollider (CS::Collisions::iCollider* collider, const csOrthoTransform& relaTrans
    = csOrthoTransform (csMatrix3 (), csVector3 (0)));
  virtual void RemoveCollider (CS::Collisions::iCollider* collider);
  virtual void RemoveCollider (size_t index);

  virtual CS::Collisions::iCollider* GetCollider (size_t index);
  virtual size_t GetColliderCount ();

  virtual void RebuildObject ();

  virtual void SetCollisionGroup (const char* name);
  virtual const char* GetCollisionGroup () const {return collGroup.name.GetData ();}

  virtual void SetCollisionCallback (CS::Collisions::iCollisionCallback* cb) {collCb = cb;}
  virtual CS::Collisions::iCollisionCallback* GetCollisionCallback () {return collCb;}

  virtual bool Collide (iCollisionObject* otherObject);
  virtual CS::Collisions::HitBeamResult HitBeam (const csVector3& start, const csVector3& end);

  virtual size_t GetContactObjectsCount ();
  virtual CS::Collisions::iCollisionObject* GetContactObject (size_t index);
};
}
CS_PLUGIN_NAMESPACE_END (Opcode2)
#endif