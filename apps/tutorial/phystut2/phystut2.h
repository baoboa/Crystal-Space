#ifndef __PHYSTUT2_H__
#define __PHYSTUT2_H__

#include "cstool/demoapplication.h"
#include "ivaria/physics.h"
#include "ivaria/bullet2.h"
#include "ivaria/collisions.h"
#include "imesh/animesh.h"
#include "imesh/animnode/ragdoll2.h"

class Simple : public CS::Utility::DemoApplication
{
private:
  csRef<CS::Collisions::iCollisionSystem> collisionSystem;
  csRef<CS::Physics::iPhysicalSystem> physicalSystem;
  csRef<CS::Collisions::iCollisionSector> collisionSector;
  csRef<CS::Physics::iPhysicalSector> physicalSector;
  csRef<CS::Physics::Bullet2::iPhysicalSector> bulletSector;
  csRef<CS::Physics::iSoftBodyAnimationControlFactory> softBodyAnimationFactory;
  csRefArray<CS::Physics::iRigidBody> dynamicBodies;
  bool isSoftBodyWorld;

  // Meshes
  csRef<iMeshFactoryWrapper> boxFact;
  csRef<iMeshFactoryWrapper> meshFact;
  csRef<CS::Collisions::iColliderConcaveMesh> mainCollider;


  // Environments
  int environment;
  csRef<iMeshWrapper> walls;

  // Configuration related
  int solver;
  bool autodisable;
  csString phys_engine_name;
  int phys_engine_id;
  bool do_bullet_debug;
  bool do_soft_debug;
  float remainingStepDuration;

  // Dynamic simulation related
  bool allStatic;
  bool pauseDynamic;
  float dynamicSpeed;

  // Camera related
  CS::Physics::Bullet2::DebugMode debugMode;
  int physicalCameraMode;
  csRef<CS::Physics::iRigidBody> cameraBody;
  csRef<CS::Collisions::iCollisionActor> cameraActor;

  // Ragdoll related
  csRef<CS::Animation::iSkeletonRagdollNodeManager2> ragdollManager;

  // Dragging related
  bool dragging;
  bool softDragging;
  csRef<CS::Physics::iJoint> dragJoint;
  csRef<CS::Physics::iSoftBody> draggedBody;
  
  size_t draggedVertex;
  float dragDistance;
  float linearDampening, angularDampening;

  // Cut & Paste related
  csRef<CS::Physics::iPhysicalBody> clipboardBody;
  csRef<iMovable> clipboardMovable;

  // Collider
  csOrthoTransform localTrans;

  // Ghost
  csRef<CS::Collisions::iCollisionObject> ghostObject;

private:
  void Frame ();
  bool OnKeyboard (iEvent &event);

  bool OnMouseDown (iEvent &event);
  bool OnMouseUp (iEvent &event);

  // Camera
  void UpdateCameraMode ();

  // Spawning objects
  bool SpawnStarCollider ();
  CS::Physics::iRigidBody* SpawnBox (bool setVelocity = true);
  CS::Physics::iRigidBody* SpawnSphere (bool setVelocity = true);
  CS::Physics::iRigidBody* SpawnCone (bool setVelocity = true);
  CS::Physics::iRigidBody* SpawnCylinder (bool setVelocity = true);
  CS::Physics::iRigidBody* SpawnCapsule (float length = rand() % 3 / 50.f + .7f,
    float radius = rand() % 10 / 50.f + .2f, bool setVelocity = true);
  CS::Collisions::iCollisionObject* SpawnConcaveMesh ();
  CS::Physics::iRigidBody* SpawnConvexMesh (bool setVelocity = true);
  CS::Physics::iRigidBody* SpawnCompound (bool setVelocity = true);
  CS::Physics::iJoint* SpawnJointed ();
  CS::Physics::iRigidBody* SpawnFilterBody (bool setVelocity = true);
  void SpawnChain ();
  void LoadFrankieRagdoll ();
  void LoadKrystalRagdoll ();
  void SpawnFrankieRagdoll ();
  void SpawnKrystalRagdoll ();
  void SpawnRope ();
  CS::Physics::iSoftBody* SpawnCloth ();
  CS::Physics::iSoftBody* SpawnSoftBody (bool setVelocity = true);

  void CreateBoxRoom ();
  void CreatePortalRoom ();
  void CreateTerrainRoom ();

  void CreateGhostCylinder ();
  void GripContactBodies ();

public:
  Simple ();
  virtual ~Simple ();

  void PrintHelp ();
  bool OnInitialize (int argc, char* argv[]);
  bool Application ();

  friend class MouseAnchorAnimationControl;
  csRef<CS::Physics::iAnchorAnimationControl> grabAnimationControl;
};

class MouseAnchorAnimationControl : public scfImplementation1
  <MouseAnchorAnimationControl, CS::Physics::iAnchorAnimationControl>
{
public:
  MouseAnchorAnimationControl (Simple* simple)
    : scfImplementationType (this), simple (simple) {}

  csVector3 GetAnchorPosition () const;

private:
  Simple* simple;
};

#endif
