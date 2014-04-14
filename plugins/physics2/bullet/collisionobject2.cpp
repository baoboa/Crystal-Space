#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "cssysdef.h"
#include "iengine/movable.h"
#include "collisionobject2.h"
#include "colliders2.h"

CS_PLUGIN_NAMESPACE_BEGIN (Bullet2)
{

csBulletCollisionObject::csBulletCollisionObject (csBulletSystem* sys)
: scfImplementationType (this), movable (NULL), camera (NULL), collCb (NULL),
  type (CS::Collisions::COLLISION_OBJECT_BASE), transform (btTransform::getIdentity ()),
  portalWarp (btQuaternion::getIdentity ()), sector (NULL), system (sys),
  btObject (NULL), compoundShape (NULL), objectOrigin (NULL), objectCopy (NULL),
  haveStaticColliders(0), insideWorld (false), shapeChanged (false), isTerrain (false)
{
  btTransform identity;
  identity.setIdentity ();
  motionState = new csBulletMotionState (this, identity, identity);
}

csBulletCollisionObject::~csBulletCollisionObject ()
{
  RemoveBulletObject ();
  colliders.DeleteAll ();
  if (btObject)
    delete btObject;
  if (compoundShape)
    delete compoundShape;
  if (motionState)
    delete motionState;
}

void csBulletCollisionObject::SetObjectType (CS::Collisions::CollisionObjectType type,
                                             bool forceRebuild /* = true */)
{
  //many many constraints.
  if (type == CS::Collisions::COLLISION_OBJECT_ACTOR || type == CS::Collisions::COLLISION_OBJECT_PHYSICAL)
  {
    csFPrintf  (stderr, "csBulletCollisionObject: Can't change from BASE/GHOST to ACTOR/PHYSICAL\n");
    return;
  }
  else if (isTerrain && type != CS::Collisions::COLLISION_OBJECT_BASE)
  {
    csFPrintf  (stderr, "csBulletCollisionObject: Can't change terrain object from BASE to other type\n");
    return;
  }
  else if (haveStaticColliders > 0)
  {
    csFPrintf  (stderr, "csBulletCollisionObject: Can't change static object from BASE to other type\n");
    return;
  }
  else
    this->type = type; // You can change type between base and ghost.
  if (forceRebuild)
    RebuildObject ();
}

void csBulletCollisionObject::SetTransform (const csOrthoTransform& trans)
{

  //Lulu: I don't understand why remove the body from the world then set MotionState,
  //      then add it back to the world. 

  transform = CSToBullet (trans, system->getInternalScale ());

  if (type == CS::Collisions::COLLISION_OBJECT_BASE || type == CS::Collisions::COLLISION_OBJECT_PHYSICAL)
  {
    if (!isTerrain)
    {
      if (insideWorld)
        sector->bulletWorld->removeRigidBody (btRigidBody::upcast (btObject));

      btTransform principalAxis = motionState->inversePrincipalAxis.inverse ();

      delete motionState;
      motionState = new csBulletMotionState (this, transform * principalAxis, principalAxis);

      if (btObject)
      {
        dynamic_cast<btRigidBody*>(btObject)->setMotionState (motionState);
        dynamic_cast<btRigidBody*>(btObject)->setCenterOfMassTransform (motionState->m_graphicsWorldTrans);
      }

      if (insideWorld)
        sector->bulletWorld->addRigidBody (btRigidBody::upcast (btObject), collGroup.value, collGroup.mask);
    }
    else
    {
      //Do not support change transform of terrain?
      //Currently in CS it's not supported.
    }
  }
  else // ghost or actor
  {
    if (movable)
      movable->SetFullTransform (BulletToCS (transform * invPricipalAxis, system->getInverseInternalScale ()));
    if (camera)
      camera->SetTransform (BulletToCS (transform * invPricipalAxis, system->getInverseInternalScale ()));
    if (btObject)
      btObject->setWorldTransform(transform);
  }
}

csOrthoTransform csBulletCollisionObject::GetTransform ()
{
  float inverseScale = system->getInverseInternalScale ();

  if (type == CS::Collisions::COLLISION_OBJECT_BASE || type == CS::Collisions::COLLISION_OBJECT_PHYSICAL)
  {
    if (!isTerrain)
    {
      btTransform trans;
      motionState->getWorldTransform (trans);
      return BulletToCS (trans * motionState->inversePrincipalAxis,
        inverseScale);
    }
    else
    {
      csBulletColliderTerrain* terrainCollider = dynamic_cast<csBulletColliderTerrain*> (colliders[0]);
      return terrainCollider->terrainTransform;      
    }
  }
  if (isTerrain)
  {
    csBulletColliderTerrain* terrainCollider = dynamic_cast<csBulletColliderTerrain*> (colliders[0]);
    return terrainCollider->terrainTransform;   
  }
  else // ghost or actor
    if (btObject)
      return BulletToCS (btObject->getWorldTransform(), system->getInverseInternalScale ());
    else
      return BulletToCS (transform, system->getInverseInternalScale ());
}

void csBulletCollisionObject::AddCollider (CS::Collisions::iCollider* collider,
                                           const csOrthoTransform& relaTrans)
{
  csRef<csBulletCollider> coll (dynamic_cast<csBulletCollider*>(collider));

  CS::Collisions::ColliderType type = collider->GetGeometryType ();
  if (type == CS::Collisions::COLLIDER_CONCAVE_MESH
    ||type == CS::Collisions::COLLIDER_CONCAVE_MESH_SCALED
    ||type == CS::Collisions::COLLIDER_PLANE)
    haveStaticColliders ++;
  
  if(type == CS::Collisions::COLLIDER_TERRAIN)
  {
    colliders.Empty ();
    relaTransforms.Empty ();
    colliders.Push (coll);
    relaTransforms.Push (relaTrans);
    isTerrain = true;
  }
  else if (!isTerrain)
  {
    // If a collision object has a terrain collider. Then it is not allowed to add other colliders.
    colliders.Push (coll);
    relaTransforms.Push (relaTrans);
  }
  shapeChanged = true;
  //User must call RebuildObject() after this.
}

void csBulletCollisionObject::RemoveCollider (CS::Collisions::iCollider* collider)
{
  for (size_t i =0; i < colliders.GetSize(); i++)
  {
    if (colliders[i] == collider)
    {
      RemoveCollider (i);
      return;
    }
  }
  //User must call RebuildObject() after this.
}

void csBulletCollisionObject::RemoveCollider (size_t index)
{
  if (index < colliders.GetSize ())
  {  
    if (isTerrain && index == 0)
      isTerrain = false;

    CS::Collisions::ColliderType type = colliders[index]->GetGeometryType ();
    if (type == CS::Collisions::COLLIDER_CONCAVE_MESH
      ||type == CS::Collisions::COLLIDER_CONCAVE_MESH_SCALED
      ||type == CS::Collisions::COLLIDER_PLANE)
      haveStaticColliders --;

    colliders.DeleteIndex (index);
    relaTransforms.DeleteIndex (index);
  }
  //User must call RebuildObject() after this.
}

CS::Collisions::iCollider* csBulletCollisionObject::GetCollider (size_t index)
{
  if (index < colliders.GetSize ())
    return colliders[index];
  return NULL;
}

void csBulletCollisionObject::RebuildObject ()
{
  size_t colliderCount = colliders.GetSize ();
  if (colliderCount == 0)
  {  
    csFPrintf (stderr, "csBulletCollisionObject: Haven't add any collider to the object.\nRebuild failed.\n");
    return;
  }

  if (shapeChanged)
  {
    if(compoundShape)
      delete compoundShape;

    if (isTerrain)
    {
      csBulletColliderTerrain* terrainColl = dynamic_cast<csBulletColliderTerrain*> (colliders[0]);
      for (size_t i = 0; i < terrainColl->bodies.GetSize (); i++)
      {
        btRigidBody* body = terrainColl->GetBulletObject (i);
        body->setUserPointer (this);
      }
    }

    else if(colliderCount >= 2)
    {  
      compoundShape = new btCompoundShape();
      for (size_t i = 0; i < colliderCount; i++)
      {
        btTransform relaTrans = CSToBullet (relaTransforms[i], system->getInternalScale ());
        compoundShape->addChildShape (relaTrans, colliders[i]->shape);
      }
      //Shift children shape?
    }
  }

  bool wasInWorld = false;
  if (insideWorld)
  {
    wasInWorld = true;
    RemoveBulletObject ();
  }

  if (wasInWorld)
    AddBulletObject ();
}

void csBulletCollisionObject::SetCollisionGroup (const char* name)
{
  if (!sector)
    return;

  CS::Collisions::CollisionGroup& group = sector->FindCollisionGroup (name);
  this->collGroup = group;

  if (btObject && insideWorld)
  {
    btObject->getBroadphaseHandle ()->m_collisionFilterGroup = collGroup.value;
    btObject->getBroadphaseHandle ()->m_collisionFilterMask = collGroup.mask;
  }
}

bool csBulletCollisionObject::Collide (CS::Collisions::iCollisionObject* otherObject)
{
  csArray<CS::Collisions::CollisionData> data;
  csBulletCollisionObject* otherObj = dynamic_cast<csBulletCollisionObject*> (otherObject);
  if (isTerrain)
  {
    //Terrain VS Terrain???
    if (otherObj->isTerrain == true)
      return false;

    //Terrain VS object/Body.
    btCollisionObject* otherBtObject = otherObj->GetBulletCollisionPointer ();
    csBulletColliderTerrain* terrainColl = dynamic_cast<csBulletColliderTerrain*> (colliders[0]);
    for (size_t i = 0; i < terrainColl->bodies.GetSize (); i++)
    {
      btRigidBody* body = terrainColl->GetBulletObject (i);
      if (sector->BulletCollide (body, otherBtObject, data))
      {
        if (otherObj->collCb)
          otherObj->collCb->OnCollision (otherObj, this, data);
        return true;
      }
    }
    return false;
  }
  else
  {
    //Object/Body CS terrain.
    if (otherObj->isTerrain == true)
      return otherObject->Collide (this);

    //Object/Body VS object.
    btCollisionObject* otherBtObject = dynamic_cast<csBulletCollisionObject*> (otherObject)->GetBulletCollisionPointer ();
    bool result = sector->BulletCollide (btObject, otherBtObject, data);
    if (result)
      if (collCb)
        collCb->OnCollision (this, otherObj, data);

    return result;
  }
  return false;
}

CS::Collisions::HitBeamResult csBulletCollisionObject::HitBeam (const csVector3& start,
                                                const csVector3& end)
{
  //Terrain part
  if (isTerrain)
  {
    csBulletColliderTerrain* terrainColl = dynamic_cast<csBulletColliderTerrain*> (colliders[0]);
    for (size_t i = 0; i < terrainColl->bodies.GetSize (); i++)
    {
      btRigidBody* body = terrainColl->GetBulletObject (i);
      CS::Collisions::HitBeamResult result = sector->RigidHitBeam (body, start, end);
      if (result.hasHit)
        return result;
    }
    return CS::Collisions::HitBeamResult();
  }
  //Others part
  else
    return sector->RigidHitBeam (btObject, start, end);
}

size_t csBulletCollisionObject::GetContactObjectsCount ()
{
  size_t result = 0;
  if (type == CS::Collisions::COLLISION_OBJECT_BASE 
    || type == CS::Collisions::COLLISION_OBJECT_PHYSICAL)
    result = contactObjects.GetSize ();
  else
  {
    btGhostObject* ghost = btGhostObject::upcast (btObject);
    if (ghost)
      result = ghost->getNumOverlappingObjects ();
    else 
      return 0;
  }

  if (objectCopy)
    result += objectCopy->GetContactObjectsCount ();

  return result;
}

CS::Collisions::iCollisionObject* csBulletCollisionObject::GetContactObject (size_t index)
{
  if (type == CS::Collisions::COLLISION_OBJECT_BASE 
    || type == CS::Collisions::COLLISION_OBJECT_PHYSICAL)
  {
    if (index < contactObjects.GetSize () && index >= 0)
      return contactObjects[index];
    else
    {
      if (objectCopy)
      {
        index -= contactObjects.GetSize ();
        return objectCopy->GetContactObject (index);
      }
      else
        return NULL;
    }
  }
  else
  {
    btGhostObject* ghost = btGhostObject::upcast (btObject);
    if (ghost)
    {
      if (index < (size_t) ghost->getNumOverlappingObjects () && index >= 0)
      {
        btCollisionObject* obj = ghost->getOverlappingObject (index);
        if (obj)
          return static_cast<CS::Collisions::iCollisionObject*> (obj->getUserPointer ());
        else 
          return NULL;
      }
      else
      {
        if (objectCopy)
        {
          index -= ghost->getNumOverlappingObjects ();
          return objectCopy->GetContactObject (index);
        }
        else
          return NULL;
      }
    }
    else 
      return NULL;
  }
}

bool csBulletCollisionObject::RemoveBulletObject ()
{
  if (insideWorld)
  {
    if (type == CS::Collisions::COLLISION_OBJECT_BASE)
    {
      if (isTerrain)
      {
        csBulletColliderTerrain* terrainColl = dynamic_cast<csBulletColliderTerrain*> (colliders[0]);
        terrainColl->RemoveRigidBodies ();
      }
      else
      {
        sector->bulletWorld->removeRigidBody (btRigidBody::upcast(btObject));
        delete btObject;
        btObject = NULL;
      }
    }
    else
    {
      sector->bulletWorld->removeCollisionObject (btObject);
      delete btObject;
      btObject = NULL;
    }
    insideWorld = false;

    if (objectCopy)
      objectCopy->sector->RemoveCollisionObject (objectCopy);
    if (objectOrigin)
      objectOrigin->objectCopy = NULL;

    objectCopy = NULL;
    objectOrigin = NULL;
    return true;
  }
  return false;
}

bool csBulletCollisionObject::AddBulletObject ()
{
  //Add to world in this function...
  if (insideWorld)
    RemoveBulletObject ();

  btCollisionShape* shape;
  if (compoundShape)
    shape = compoundShape;
  else
    shape = colliders[0]->shape;

  btTransform pricipalAxis;
  if (compoundShape)
    pricipalAxis.setIdentity ();
  else
    pricipalAxis = CSToBullet (relaTransforms[0], system->getInternalScale ());

  invPricipalAxis = pricipalAxis.inverse ();

  if (type == CS::Collisions::COLLISION_OBJECT_BASE)
  {
    if (!isTerrain)
    {
      btTransform trans;
      motionState->getWorldTransform (trans);
      trans = trans * motionState->inversePrincipalAxis;
      delete motionState;
      motionState = new csBulletMotionState (this, trans * pricipalAxis, pricipalAxis);

      btVector3 localInertia (0.0f, 0.0f, 0.0f);
      btRigidBody::btRigidBodyConstructionInfo infos (0.0, motionState,
        shape, localInertia);
      btObject = new btRigidBody (infos);
      btObject->setUserPointer (dynamic_cast<iCollisionObject*> (this));
      sector->bulletWorld->addRigidBody (btRigidBody::upcast(btObject), collGroup.value, collGroup.mask);

    }
    else
      dynamic_cast<csBulletColliderTerrain*> (colliders[0])->AddRigidBodies (sector, this);
  }
  else if (type == CS::Collisions::COLLISION_OBJECT_GHOST 
    || type == CS::Collisions::COLLISION_OBJECT_ACTOR)
  {

    btObject = new btPairCachingGhostObject ();
    btObject->setWorldTransform (transform);
    if (movable)
      movable->SetFullTransform (BulletToCS(transform * invPricipalAxis, system->getInverseInternalScale ()));
    if (camera)
      camera->SetTransform (BulletToCS(transform * invPricipalAxis, system->getInverseInternalScale ()));
    btObject->setUserPointer (dynamic_cast<CS::Collisions::iCollisionObject*> (this));
    btObject->setCollisionShape (shape);
    sector->broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
    sector->bulletWorld->addCollisionObject (btObject, collGroup.value, collGroup.mask);
  }
  insideWorld = true;
  return true;
}
}
CS_PLUGIN_NAMESPACE_END (Bullet2)
