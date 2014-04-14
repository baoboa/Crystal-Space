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

#ifndef __CS_BULLET_RIGIDBODY_H__
#define __CS_BULLET_RIGIDBODY_H__

#include "bullet2.h"
#include "common2.h"
#include "collisionobject2.h"

CS_PLUGIN_NAMESPACE_BEGIN (Bullet2)
{
class csBulletPhysicalSystem;
class csBulletDefaultKinematicCallback;

using CS::Physics::iPhysicalBody;
using CS::Physics::iRigidBody;
using CS::Physics::iSoftBody;

class csBulletRigidBody : public scfImplementationExt1<
  csBulletRigidBody, csBulletCollisionObject, 
  CS::Physics::iRigidBody>
{
  friend class csBulletKinematicMotionState;
  friend class csBulletSoftBody;
  friend class csBulletJoint;
  friend class csBulletSector;
private:
  // TODO: remove as much as possible of these fields and store them in the btRigidBody instead
  btRigidBody* btBody;
  CS::Physics::RigidBodyState physicalState;
  float density;
  float totalMass;
  float friction;
  float softness;
  float elasticity;
  float linearDampening;
  float angularDampening;
  csVector3 linearVelocity;
  csVector3 angularVelocity;
  short anchorCount;
  csRef<CS::Physics::iKinematicCallback> kinematicCb;

public:
  csBulletRigidBody (csBulletSystem* phySys);
  virtual ~csBulletRigidBody ();

  virtual iObject* QueryObject (void) { return (iObject*) this; }
  //iCollisionObject
  virtual iCollisionObject* QueryCollisionObject () {return dynamic_cast<csBulletCollisionObject*> (this);}
  virtual iPhysicalBody* QueryPhysicalBody () {return this;}

  virtual void SetObjectType (CS::Collisions::CollisionObjectType type, bool forceRebuild = true) {}
  virtual CS::Collisions::CollisionObjectType GetObjectType () {return CS::Collisions::COLLISION_OBJECT_PHYSICAL;}

  virtual void SetAttachedMovable (iMovable* movable) {csBulletCollisionObject::SetAttachedMovable (movable);}
  virtual iMovable* GetAttachedMovable () {return csBulletCollisionObject::GetAttachedMovable ();}

  virtual void SetAttachedCamera (iCamera* camera) {csBulletCollisionObject::SetAttachedCamera (camera);}
  virtual iCamera* GetAttachedCamera () {return csBulletCollisionObject::GetAttachedCamera ();}

  virtual void SetTransform (const csOrthoTransform& trans) {csBulletCollisionObject::SetTransform (trans);}
  virtual csOrthoTransform GetTransform () {return csBulletCollisionObject::GetTransform ();}

  virtual void RebuildObject () {csBulletCollisionObject::RebuildObject ();}

  virtual void AddCollider (CS::Collisions::iCollider* collider, const csOrthoTransform& relaTrans
    = csOrthoTransform (csMatrix3 (), csVector3 (0)));
  virtual void RemoveCollider (CS::Collisions::iCollider* collider);
  virtual void RemoveCollider (size_t index);

  virtual CS::Collisions::iCollider* GetCollider (size_t index) {return csBulletCollisionObject::GetCollider (index);}
  virtual size_t GetColliderCount () {return colliders.GetSize ();}

  virtual void SetCollisionGroup (const char* name) {csBulletCollisionObject::SetCollisionGroup (name);}
  virtual const char* GetCollisionGroup () const {return csBulletCollisionObject::GetCollisionGroup ();}

  virtual void SetCollisionCallback (CS::Collisions::iCollisionCallback* cb) {collCb = cb;}
  virtual CS::Collisions::iCollisionCallback* GetCollisionCallback () {return collCb;}

  virtual bool Collide (iCollisionObject* otherObject) {return csBulletCollisionObject::Collide (otherObject);}
  virtual CS::Collisions::HitBeamResult HitBeam (const csVector3& start, const csVector3& end)
  { return csBulletCollisionObject::HitBeam (start, end);}

  virtual size_t GetContactObjectsCount () {return contactObjects.GetSize ();}
  virtual CS::Collisions::iCollisionObject* GetContactObject (size_t index) {
    return csBulletCollisionObject::GetContactObject (index);}

  btRigidBody* GetBulletRigidPointer () {return btBody;}
  virtual bool RemoveBulletObject ();
  virtual bool AddBulletObject ();

  //iPhysicalBody

  virtual CS::Physics::PhysicalBodyType GetBodyType () const {return CS::Physics::BODY_RIGID;}
  virtual iRigidBody* QueryRigidBody () {return dynamic_cast<iRigidBody*>(this);}
  virtual iSoftBody* QuerySoftBody () {return NULL;}

  virtual bool Disable ();
  virtual bool Enable ();
  virtual bool IsEnabled ();

  virtual void SetMass (float mass);
  virtual float GetMass ();

  virtual float GetDensity () const {return density;}
  virtual void SetDensity (float density);

  virtual float GetVolume ();

  virtual void AddForce (const csVector3& force);

  virtual csVector3 GetLinearVelocity (size_t index = 0) const;

  virtual void SetFriction (float friction);
  virtual float GetFriction () {return friction;}

  //iRigidBody
  virtual CS::Physics::RigidBodyState GetState () {return physicalState;}
  virtual bool SetState (CS::Physics::RigidBodyState state);

  virtual void SetElasticity (float elasticity);
  virtual float GetElasticity () {return elasticity;}

  virtual void SetLinearVelocity (const csVector3& vel);

  virtual void SetAngularVelocity (const csVector3& vel);
  virtual csVector3 GetAngularVelocity () const;

  virtual void AddTorque (const csVector3& torque);

  virtual void AddRelForce (const csVector3& force);
  virtual void AddRelTorque (const csVector3& torque);

  virtual void AddForceAtPos (const csVector3& force,
      const csVector3& pos);
  virtual void AddForceAtRelPos (const csVector3& force,
      const csVector3& pos);

  virtual void AddRelForceAtPos (const csVector3& force,
      const csVector3& pos);
  virtual void AddRelForceAtRelPos (const csVector3& force,
      const csVector3& pos);

  virtual csVector3 GetForce () const;
  virtual csVector3 GetTorque () const;

  virtual void SetKinematicCallback (CS::Physics::iKinematicCallback* cb) {kinematicCb = cb;}
  virtual CS::Physics::iKinematicCallback* GetKinematicCallback () {return kinematicCb;}

  virtual void SetLinearDampener (float d);
  virtual float GetLinearDampener () {return linearDampening;}

  virtual void SetRollingDampener (float d);
  virtual float GetRollingDampener () {return angularDampening;}
};

class csBulletDefaultKinematicCallback : public scfImplementation1<
  csBulletDefaultKinematicCallback, CS::Physics::iKinematicCallback>
{
public:
  csBulletDefaultKinematicCallback ();
  virtual ~csBulletDefaultKinematicCallback();
  virtual void GetBodyTransform (CS::Physics::iRigidBody* body, csOrthoTransform& transform) const;
};
}
CS_PLUGIN_NAMESPACE_END (Bullet2)

#endif
