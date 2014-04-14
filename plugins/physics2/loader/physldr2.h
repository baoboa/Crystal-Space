/*
    Copyright (C) 2011 by Liu Lu

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


#ifndef __CS_PHYSLDR2_H__
#define __CS_PHYSLDR2_H__

#include "imap/reader.h"
#include "iutil/eventh.h"
#include "iutil/comp.h"
#include "csutil/strhash.h"
#include "csutil/scf_implementation.h"

struct iObjectRegistry;
struct iReporter;
struct iSyntaxService;
struct iEngine;

namespace CS
{
namespace Physics
{
struct iPhysicalSystem;
struct iPhysicalSector;
struct iRigidBody;
struct iSoftBody;
struct iJoint;
}
}

namespace CS
{
namespace Collisions
{
struct iCollisionSystem;
struct iCollisionSector;
struct iCollisionObject;
struct iCollider;
}
}

class csPhysicsLoader2 :
  public scfImplementation2<csPhysicsLoader2, iLoaderPlugin, iComponent>
{
public:
  csPhysicsLoader2 (iBase*);
  virtual ~csPhysicsLoader2 ();

  bool Initialize (iObjectRegistry*);

  /// Parse the physics node and setup the environment
  virtual csPtr<iBase> Parse (iDocumentNode *node,
    iStreamSource*, iLoaderContext* ldr_context, iBase* context);

  virtual bool IsThreadSafe() { return true; }
  ///// Parse the system specific sub section
  //virtual bool ParseSystem (iDocumentNode *node, CS::Collisions::iCollisionSystem* system, iLoaderContext* ldr_context);
  /// Parse the collision sector specific sub section
  virtual bool ParseCollisionSector (iDocumentNode *node, CS::Collisions::iCollisionSector* collSector, iLoaderContext* ldr_context);
  /// Parse the collision object specific sub section
  virtual bool ParseCollisionObject (iDocumentNode *node, CS::Collisions::iCollisionObject* object, 
    CS::Collisions::iCollisionSector* collSector, iLoaderContext* ldr_context);
  /// Parse the rigid body specific sub section
  virtual bool ParseRigidBody (iDocumentNode *node, CS::Physics::iRigidBody* body, 
    CS::Collisions::iCollisionSector* collSector, iLoaderContext* ldr_context);
  /// Parse the soft body specific sub section
  virtual bool ParseSoftBody (iDocumentNode *node, 
    CS::Physics::iPhysicalSector* physSector, iLoaderContext* ldr_context);
  /// Parse an anonymous mesh collider of the collision object.
  virtual bool ParseColliderConcaveMesh (iDocumentNode *node,
    CS::Collisions::iCollisionObject* object, iLoaderContext* ldr_context);
  /// Parse an anonymous mesh collider of the collision object.
  virtual bool ParseColliderConvexMesh (iDocumentNode *node,
    CS::Collisions::iCollisionObject* object, iLoaderContext* ldr_context);
  /// Parse an anonymous sphere collider of the collision object.
  virtual bool ParseColliderSphere (iDocumentNode *node,
  	CS::Collisions::iCollisionObject* object);
  /// Parse an anonymous cylinder collider of the collision object.
  virtual bool ParseColliderCylinder (iDocumentNode *node,
  	CS::Collisions::iCollisionObject* object);
  /// Parse an anonymous capsule collider of the collision object.
  virtual bool ParseColliderCapsule (iDocumentNode *node,
  	CS::Collisions::iCollisionObject* object);
  /// Parse an anonymous cone collider of the collision object.
  virtual bool ParseColliderCone (iDocumentNode *node,
    CS::Collisions::iCollisionObject* object);
  /// Parse an anonymous box collider of the collision object.
  virtual bool ParseColliderBox (iDocumentNode *node,
  	CS::Collisions::iCollisionObject* object);
  /// Parse an anonymous plane collider of the collision object.
  virtual bool ParseColliderPlane (iDocumentNode *node,
  	CS::Collisions::iCollisionObject* object);
  /// Parse an anonymous terrain collider of the collision object.
  virtual bool ParseColliderTerrain (iDocumentNode *node,
    CS::Collisions::iCollisionObject* object, iLoaderContext* ldr_context);
  /// Parse the joint specific sub section
  virtual bool ParseJoint (iDocumentNode *node, CS::Physics::iJoint* joint,
  	CS::Physics::iPhysicalSector* sector);
  /// Parse a transform
  virtual bool ParseTransform (iDocumentNode *node, csOrthoTransform &t);
  /// Parse a constraint definition
  virtual bool ParseConstraint (iDocumentNode *node,
  	bool &, bool &, bool &, csVector3 &, csVector3 &);

private:
  iObjectRegistry* object_reg;
  csRef<iReporter> reporter;
  csRef<iSyntaxService> synldr;
  csRef<iEngine> engine;
  csRef<CS::Collisions::iCollisionSystem> collisionSystem;
  csRef<CS::Physics::iPhysicalSystem> physicalSystem;
  csStringHash xmltokens;
};

#endif // __CS_PHYSLDR_H__
