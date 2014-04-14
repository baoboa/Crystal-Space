#ifndef __CS_BULLET_COLLISIONOBJECT_H__
#define __CS_BULLET_COLLISIONOBJECT_H__

#include "bullet2.h"
#include "common2.h"
#include "colliders2.h"

CS_PLUGIN_NAMESPACE_BEGIN(Bullet2)
{

//struct CS::Physics::iPhysicalBody;

class csBulletCollisionObject: public scfImplementationExt1<
  csBulletCollisionObject, csObject, CS::Collisions::iCollisionObject>
{
  friend class csBulletSector;
  friend class csBulletSystem;
  friend class csBulletMotionState;
  friend class csBulletKinematicMotionState;
  friend class csBulletJoint;
  friend class csBulletColliderTerrain;
  friend class csBulletCollisionActor;

protected:
  csRefArray<csBulletCollider> colliders;
  csRefArray<csBulletCollisionObject> contactObjects;
  csArray<csOrthoTransform> relaTransforms;
  csArray<CS::Physics::iJoint*> joints;
  CS::Collisions::CollisionGroup collGroup;
  csWeakRef<iMovable> movable;
  csWeakRef<iCamera> camera;
  csRef<CS::Collisions::iCollisionCallback> collCb;
  CS::Collisions::CollisionObjectType type;

  btTransform transform;
  btTransform invPricipalAxis;
  btQuaternion portalWarp;

  csBulletSector* sector;
  csBulletSystem* system;
  btCollisionObject* btObject;
  csBulletMotionState* motionState;
  btCompoundShape* compoundShape;
  csBulletCollisionObject* objectOrigin;
  csBulletCollisionObject* objectCopy;

  short haveStaticColliders;
  bool insideWorld;
  bool shapeChanged;
  bool isTerrain;

public:
  csBulletCollisionObject (csBulletSystem* sys);
  virtual ~csBulletCollisionObject ();

  virtual iObject* QueryObject (void) { return (iObject*) this; }
  virtual CS::Collisions::iCollisionObject* QueryCollisionObject () {
    return dynamic_cast<CS::Collisions::iCollisionObject*> (this);}
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

  virtual CS::Collisions::iCollider* GetCollider (size_t index) ;
  virtual size_t GetColliderCount () {return colliders.GetSize ();}

  virtual void RebuildObject ();

  virtual void SetCollisionGroup (const char* name);
  virtual const char* GetCollisionGroup () const {return collGroup.name.GetData ();}

  virtual void SetCollisionCallback (CS::Collisions::iCollisionCallback* cb) {collCb = cb;}
  virtual CS::Collisions::iCollisionCallback* GetCollisionCallback () {return collCb;}

  virtual bool Collide (CS::Collisions::iCollisionObject* otherObject);
  virtual CS::Collisions::HitBeamResult HitBeam (const csVector3& start, const csVector3& end);

  virtual size_t GetContactObjectsCount ();
  virtual CS::Collisions::iCollisionObject* GetContactObject (size_t index);

  btCollisionObject* GetBulletCollisionPointer () {return btObject;}
  virtual bool RemoveBulletObject ();
  virtual bool AddBulletObject ();
  void RemoveObjectCopy () {
    csBulletSector* sec = objectCopy->sector;
    sec->RemoveCollisionObject (objectCopy);
    objectCopy = NULL;
  }
};
}
CS_PLUGIN_NAMESPACE_END (Bullet2)
#endif
