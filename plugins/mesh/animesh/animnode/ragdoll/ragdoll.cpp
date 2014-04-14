/*
  Copyright (C) 2009-12 Christian Van Brussel, Institute of Information
      and Communication Technologies, Electronics and Applied Mathematics
      at Universite catholique de Louvain, Belgium
      http://www.uclouvain.be/en-icteam.html

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
#include "csutil/scf.h"

#include "ragdoll.h"
#include "ivaria/reporter.h"
#include "csgeom/transfrm.h"
#include "csgeom/plane3.h"
#include "iengine/scenenode.h"
#include "iengine/movable.h"
#include "iengine/mesh.h"
#include "imesh/object.h"
#include "imesh/animesh.h"

CS_PLUGIN_NAMESPACE_BEGIN(Ragdoll)
{
  SCF_IMPLEMENT_FACTORY(RagdollNodeManager);

  void RagdollNodeManager::Report (int severity, const char* msg, ...) const
  {
    va_list arg;
    va_start (arg, msg);
    csReportV (object_reg, severity, 
	       "crystalspace.mesh.animesh.animnode.ragdoll",
	       msg, arg);
    va_end (arg);
  }

  // ------------------------------------------------------------------------

  size_t GetChainIndex (csArray<ChainData>& chains, CS::Animation::iBodyChain*& chain)
  {
    size_t index = 0;
    for (csArray<ChainData>::Iterator it = chains.GetIterator (); it.HasNext (); index++)
    {
      ChainData& chainData = it.Next ();
      if (chainData.chain == chain)
	return index;
    }
    return (size_t) ~0;
  }

  size_t GetChainIndex (const csArray<ChainData>& chains, CS::Animation::iBodyChain*& chain)
  {
    size_t index = 0;
    for (csArray<ChainData>::ConstIterator it = chains.GetIterator (); it.HasNext (); index++)
    {
      const ChainData& chainData = it.Next ();
      if (chainData.chain == chain)
	return index;
    }
    return (size_t) ~0;
  }

  // --------------------------  RagdollNodeFactory  --------------------------

  CS_LEAKGUARD_IMPLEMENT(RagdollNodeFactory);

  RagdollNodeFactory::RagdollNodeFactory (RagdollNodeManager* manager, const char *name)
    : scfImplementationType (this), CS::Animation::SkeletonAnimNodeFactorySingle (name),
    manager (manager)
  {
  }

  void RagdollNodeFactory::SetBodySkeleton (CS::Animation::iBodySkeleton* skeleton)
  {
    bodySkeleton = skeleton;
  }

  CS::Animation::iBodySkeleton* RagdollNodeFactory::GetBodySkeleton () const
  {
    return bodySkeleton;
  }

  void RagdollNodeFactory::AddBodyChain (CS::Animation::iBodyChain* chain,
					     CS::Animation::RagdollState state)
  {
    ChainData data;
    data.chain = chain;
    data.state = state;
    chains.Push (data);
  }

  void RagdollNodeFactory::RemoveBodyChain (CS::Animation::iBodyChain* chain)
  {
    size_t index = GetChainIndex (chains, chain);
    if (index != (size_t) ~0)
      chains.DeleteIndexFast (index);
  }

  void RagdollNodeFactory::SetDynamicSystem (iDynamicSystem* system)
  {
    dynamicSystem = system;
  }

  iDynamicSystem* RagdollNodeFactory::GetDynamicSystem () const
  {
    return dynamicSystem;
  }

  csPtr<CS::Animation::SkeletonAnimNodeSingleBase> RagdollNodeFactory::ActualCreateInstance (
    CS::Animation::iSkeletonAnimPacket* packet,
    CS::Animation::iSkeleton* skeleton)
  {
    return csPtr<CS::Animation::SkeletonAnimNodeSingleBase>
      (new RagdollNode (this, skeleton, dynamicSystem));
  }

  // --------------------------  RagdollNode  --------------------------

  CS_LEAKGUARD_IMPLEMENT(RagdollNode);

  RagdollNode::RagdollNode (RagdollNodeFactory* factory, 
			    CS::Animation::iSkeleton* skeleton,
			    iDynamicSystem* system)
    : scfImplementationType (this),
    CS::Animation::SkeletonAnimNodeSingle<RagdollNodeFactory> (factory, skeleton),
    sceneNode (nullptr), dynamicSystem (system), maxBoneID (0)
  {
    // Copy the body chains
    for (csArray<ChainData>::Iterator it = factory->chains.GetIterator (); it.HasNext (); )
    {
      ChainData& chainData = it.Next ();
      chains.Push (chainData);

      // Create an entry for each bones of the chain
      CreateBoneData (chainData.chain->GetRootNode (), chainData.state);
    }
  }

  RagdollNode::~RagdollNode ()
  {
    Stop ();
  }

  void RagdollNode::SetDynamicSystem (iDynamicSystem* system)
  {
    if (!dynamicSystem)
    {
      dynamicSystem = system;
      InitBoneStates ();
      return;
    }

    for (csHash<BoneData, CS::Animation::BoneID>::GlobalIterator it = bones.GetIterator ();
      it.HasNext(); )
    {
      BoneData& boneData = it.Next ();

      if (system)
      {
	if (boneData.rigidBody)
	  system->AddBody (boneData.rigidBody);
	if (boneData.joint)
	  system->AddJoint (boneData.joint);
      }

      else
      {
	if (boneData.rigidBody)
	  dynamicSystem->RemoveBody (boneData.rigidBody);
	if (boneData.joint)
	  dynamicSystem->RemoveJoint (boneData.joint);
      }
    }

    dynamicSystem = system;
  }

  iDynamicSystem* RagdollNode::GetDynamicSystem () const
  {
    return dynamicSystem;
  }

  void RagdollNode::CreateBoneData (CS::Animation::iBodyChainNode* chainNode,
				    CS::Animation::RagdollState state)
  {
    CS::Animation::iBodyBone* bodyBone =
      factory->bodySkeleton->FindBodyBone (chainNode->GetAnimeshBone ());
    CS_ASSERT (bodyBone);

    // Check if the bone is already defined
    if (!bones.Contains (bodyBone->GetAnimeshBone ()))
    {
      // Create the bone reference
      BoneData boneData;
      boneData.boneID = bodyBone->GetAnimeshBone ();
      boneData.state = state;
      bones.Put (boneData.boneID, boneData);

      // Update the maximum bone ID
      maxBoneID = csMax (maxBoneID, boneData.boneID);
    }

    // Update the state of the children nodes
    for (uint i = 0; i < chainNode->GetChildCount (); i++)
      CreateBoneData (chainNode->GetChild (i), state);
  }

  void RagdollNode::SetBodyChainState (CS::Animation::iBodyChain* chain,
				       CS::Animation::RagdollState state)
  {
    size_t index = GetChainIndex (chains, chain);
#ifdef CS_DEBUG
    // Check that the chain is registered
    if (index == (size_t) ~0)
    {
      factory->manager->Report (CS_REPORTER_SEVERITY_WARNING,
       "Chain %s was not registered in the ragdoll plugin while trying to set new state",
				chain->GetName ());
      return;
    }
#endif

    // TODO: if dynamic then check that there is sth to link to

    // Set the new chain state
    chains[index].state = state;
    SetChainNodeState (chain->GetRootNode (), state);
  }

  void RagdollNode::SetChainNodeState (CS::Animation::iBodyChainNode* node,
				       CS::Animation::RagdollState state)
  {
    // Find the associated bone data
    BoneData nullBone;
    BoneData& boneData = bones.Get (node->GetAnimeshBone (), nullBone);
    boneData.state = state;

    // Update the state of the bone if this node is playing
    if (isPlaying)
      UpdateBoneState (&boneData);

    // Update the state of the children nodes
    for (uint i = 0; i < node->GetChildCount (); i++)
      SetChainNodeState (node->GetChild (i), state);
  }

  CS::Animation::RagdollState RagdollNode::GetBodyChainState
    (CS::Animation::iBodyChain* chain) const
  {
    size_t index = GetChainIndex (chains, chain);
    if (index == (size_t) ~0)
      return CS::Animation::STATE_INACTIVE;

    return chains[index].state;
  }

  iRigidBody* RagdollNode::GetBoneRigidBody (CS::Animation::BoneID bone)
  {
    if (!bones.Contains (bone))
      return nullptr;

    return bones[bone]->rigidBody;
  }

  iJoint* RagdollNode::GetBoneJoint (const CS::Animation::BoneID bone)
  {
    if (!bones.Contains (bone))
      return nullptr;

    return bones[bone]->joint;
  }

  uint RagdollNode::GetBoneCount (CS::Animation::RagdollState state) const
  {
    uint count = 0;
    for (csHash<BoneData, CS::Animation::BoneID>::ConstGlobalIterator it = bones.GetIterator ();
	 it.HasNext (); )
    {
      const BoneData& boneData = it.Next ();

      if (boneData.state == state)
	count++;
    }

    return count;
  }

  CS::Animation::BoneID RagdollNode::GetBone (CS::Animation::RagdollState state, uint index) const
  {
    uint count = 0;
    for (csHash<BoneData, CS::Animation::BoneID>::ConstGlobalIterator it = bones.GetIterator ();
	 it.HasNext (); )
    {
      const BoneData& boneData = it.Next ();

      if (boneData.state == state)
      {
	if (count == index)
	  return boneData.boneID;

	count++;
      }
    }

    return CS::Animation::InvalidBoneID;
  }

  CS::Animation::BoneID RagdollNode::GetRigidBodyBone (iRigidBody* body) const
  {
    for (csHash<BoneData, CS::Animation::BoneID>::ConstGlobalIterator it = bones.GetIterator ();
	 it.HasNext (); )
    {
      const BoneData& boneData = it.Next ();

      if (boneData.rigidBody == body)
	return boneData.boneID;
    }

    return CS::Animation::InvalidBoneID;
  }

  void RagdollNode::ResetChainTransform (CS::Animation::iBodyChain* chain)
  {
#ifdef CS_DEBUG
    // Check that the chain is registered
    size_t index = GetChainIndex (chains, chain);
    if (index == (size_t) ~0)
    {
      factory->manager->Report (CS_REPORTER_SEVERITY_WARNING,
       "Chain %s was not registered in the ragdoll plugin while trying to reset the chain transform",
				chain->GetName ());
      return;
    }

    // Check that the chain is in dynamic state
    if (chains[index].state != CS::Animation::STATE_DYNAMIC)
    {
      factory->manager->Report (CS_REPORTER_SEVERITY_WARNING,
       "Chain %s was not in dynamic state while trying to reset the chain transform",
				chain->GetName ());
      return;
    }
#endif

    // Schedule for the reset (it cannot be made right now since the skeleton state
    // may not yet be in a new state, eg if the user has just changed a chain to dynamic
    // state)
    ResetChainData chainData;
    chainData.chain = chain;
    chainData.frameCount = 2; // The skeleton may not be in a good state before 2 frames
    resetChains.Put (0, chainData);
  }

  void RagdollNode::Play ()
  {
    if (isPlaying)
      return;
    isPlaying = true;

    // Find the scene node
    if (!sceneNode)
      sceneNode = skeleton->GetSceneNode ();

    // Init the bone states
    if (dynamicSystem)
      InitBoneStates ();

    // Start the child node
    if (childNode)
      childNode->Play ();
  }

  void RagdollNode::Stop ()
  {
    if (!isPlaying)
      return;

    isPlaying = false;

    // Update state of all bones
    for (csHash<BoneData, CS::Animation::BoneID>::GlobalIterator it = bones.GetIterator ();
	 it.HasNext(); )
    {
      BoneData& bone = it.Next ();
      UpdateBoneState (&bone);
    }

    // Stop child node
    if (childNode)
      childNode->Stop ();
  }

  void RagdollNode::BlendState (CS::Animation::AnimatedMeshState* state, float baseWeight)
  {
    // TODO: use baseWeight

    // Check that this node is active
    if (!isPlaying || !dynamicSystem)
      return;

    // Make the child node blend the state
    if (childNode)
      childNode->BlendState (state, baseWeight);

    // Reset the chains that have been asked for
    for (int i = ((int) resetChains.GetSize ()) - 1; i >= 0; i--)
    {
      ResetChainData &chainData = resetChains.Get (i);
      chainData.frameCount--;

      if (chainData.frameCount == 0)
      {
        ResetChainNodeTransform (chainData.chain->GetRootNode ());
        resetChains.DeleteIndex (i);
      }
    }

    // Update each bones
    bool hasNaN = false;
    for (csHash<BoneData, CS::Animation::BoneID>::GlobalIterator it = bones.GetIterator ();
      it.HasNext(); )
    {
      BoneData& boneData = it.Next ();

      // TODO: test for deactivation of rigid body

      // Check if the bone is in dynamic state
      if (boneData.state != CS::Animation::STATE_DYNAMIC)
        continue;

      // Get the bind transform of the bone
      csQuaternion skeletonRotation;
      csVector3 skeletonOffset;
      skeleton->GetFactory ()->GetTransformBoneSpace (boneData.boneID, skeletonRotation,
						      skeletonOffset);

      csOrthoTransform bodyTransform = boneData.rigidBody->GetTransform ();
      if (CS::IsNaN (bodyTransform.GetOrigin ()[0]))
      {
	hasNaN = true;
	continue;
      }

      CS::Animation::BoneID parentBoneID = skeleton->GetFactory ()->GetBoneParent (boneData.boneID);

      // If this bone is the root of the skeleton
      if (parentBoneID == CS::Animation::InvalidBoneID)
      {
	// Compute the new bone transform
	csReversibleTransform relativeTransform =
	  bodyTransform * sceneNode->GetMovable ()->GetFullTransform ().GetInverse ();

	// Apply the new transform to the CS::Animation::AnimatedMeshState
	state->SetBoneUsed (boneData.boneID);
	state->GetVector (boneData.boneID) = relativeTransform.GetOrigin () - skeletonOffset;
	csQuaternion quaternion;
	quaternion.SetMatrix (relativeTransform.GetT2O ());
	state->GetQuaternion (boneData.boneID) = quaternion * skeletonRotation.GetConjugate ();

	continue;
      }
      
      // If this bone is inside the ragdoll chain
      if (bones.Contains (parentBoneID))
      {
	// Compute the new bone transform
	BoneData nullBone;
	csOrthoTransform parentTransform =
	  bones.Get (parentBoneID, nullBone).rigidBody->GetTransform ();
	csReversibleTransform relativeTransform =
	  bodyTransform * parentTransform.GetInverse ();

	// Apply the new transform to the CS::Animation::AnimatedMeshState
	state->SetBoneUsed (boneData.boneID);
	state->GetVector (boneData.boneID) = relativeTransform.GetOrigin () - skeletonOffset;
	csQuaternion quaternion;
	quaternion.SetMatrix (relativeTransform.GetT2O ());
	state->GetQuaternion (boneData.boneID) = quaternion * skeletonRotation.GetConjugate ();

	continue;
      }

      // Else this bone is the root of the ragdoll chain, but not of the skeleton
      else
      {
	// TODO: the transform should be read from the AnimatedMeshState
	csQuaternion boneRotation;
	csVector3 boneOffset;
	skeleton->GetTransformAbsSpace (parentBoneID, boneRotation, boneOffset);
	csOrthoTransform parentTransform (csMatrix3 (boneRotation.GetConjugate ()), boneOffset);
	parentTransform = parentTransform * sceneNode->GetMovable ()->GetFullTransform ();

	// Compute the new bone transform
	csReversibleTransform relativeTransform =
	  bodyTransform * parentTransform.GetInverse ();

	// Apply the new transform to the CS::Animation::AnimatedMeshState
	state->SetBoneUsed (boneData.boneID);
	state->GetVector (boneData.boneID) = relativeTransform.GetOrigin () - skeletonOffset;
	csQuaternion quaternion;
	quaternion.SetMatrix (relativeTransform.GetT2O ());
	state->GetQuaternion (boneData.boneID) = quaternion * skeletonRotation.GetConjugate ();

	continue;
      }
    }

    // Report any NaN values found
    if (hasNaN)
      factory->manager->Report
	(CS_REPORTER_SEVERITY_WARNING,
	 "NaN values found while updating the ragdoll's rigid bodies");
  }

  void RagdollNode::TickAnimation (float dt)
  {
    // Tick the child node
    if (childNode)
      childNode->TickAnimation (dt);

    // Update of the position of the mesh

    // Search for the list of bones that have a direct influence on the position
    // of the mesh (ie the bones that have no colliders or bounding boxes in any of
    // their parents).
    csArray<BoneData*> rootBones;
    for (size_t i = 0; i < chains.GetSize (); i++)
    {
      const ChainData& chainData = chains.Get (i);
      if (chainData.state != CS::Animation::STATE_DYNAMIC)
	continue;

      // Check if there is any collider or bounding box in one of the parent
      CS::Animation::BoneID boneID = skeleton->GetFactory ()->GetBoneParent
	(chainData.chain->GetRootNode ()->GetAnimeshBone ());
      while (boneID != CS::Animation::InvalidBoneID)
      {
	if (!skeleton->GetAnimatedMesh ()->GetBoneBoundingBox (boneID).Empty ()
	    || bones.Contains (boneID))
	  break;

	boneID = skeleton->GetFactory ()->GetBoneParent (boneID);
      }

      // Add this bone to the list
      if (boneID == CS::Animation::InvalidBoneID)
	rootBones.Push (bones.GetElementPointer
			(chainData.chain->GetRootNode ()->GetAnimeshBone ()));
    }

    // If no bone influence the position of the mesh then don't do anything
    if (!rootBones.GetSize ())
      return;

    // If there is only one bone influencing the position of the mesh
    if (rootBones.GetSize () == 1)
    {
      // Compute the new transform of the mesh
      csOrthoTransform bodyTransform = rootBones[0]->rigidBody->GetTransform ();
      csQuaternion boneRotation;
      csVector3 boneOffset;
      skeleton->GetTransformAbsSpace (rootBones[0]->boneID, boneRotation, boneOffset);
      csOrthoTransform boneTransform (csMatrix3 (boneRotation.GetConjugate ()), boneOffset);
      csOrthoTransform newTransform = boneTransform.GetInverse () * bodyTransform;
      
      // Apply the new transform to the iMovable of the animesh
      iMovable* movable = sceneNode->GetMovable ();
      movable->SetFullTransform (newTransform);
      movable->UpdateMove ();
    }

    // If there is only one bone influencing the position of the mesh
    else
    {
      // Compute the mean origin of all bones
      csVector3 origin (0.0f);
      for (size_t i = 0; i < rootBones.GetSize (); i++)
      {
	csOrthoTransform bodyTransform = rootBones[i]->rigidBody->GetTransform ();
	csQuaternion boneRotation;
	csVector3 boneOffset;
	skeleton->GetTransformAbsSpace (rootBones[i]->boneID, boneRotation, boneOffset);
	csOrthoTransform boneTransform (csMatrix3 (boneRotation.GetConjugate ()), boneOffset);
	csOrthoTransform newTransform = boneTransform.GetInverse () * bodyTransform;
	origin += newTransform.GetOrigin ();
      }
      origin /= rootBones.GetSize ();

      // Apply the new origin to the iMovable of the animesh
      iMovable* movable = sceneNode->GetMovable ();
      csTransform transform (csMatrix3 (), origin);
      movable->SetFullTransform (transform);
      movable->UpdateMove ();
    }
  }

  void RagdollNode::UpdateBoneState (BoneData* boneData)
  {
    // Check if this node has been stopped or if the bone is inactive
    if (!isPlaying
	|| boneData->state == CS::Animation::STATE_INACTIVE)
    {
      if (boneData->joint)
      {
	dynamicSystem->RemoveJoint (boneData->joint);
	boneData->joint = 0;
      }

      if (boneData->rigidBody)
      {
	dynamicSystem->RemoveBody (boneData->rigidBody);
	boneData->rigidBody = 0;
      }

      return;
    }

    CS::Animation::iBodyBone* bodyBone = factory->bodySkeleton->FindBodyBone (boneData->boneID);

    // Create the rigid body if not yet done
    bool previousBody = true;
    if (!boneData->rigidBody)
    {
      previousBody = false;

      // Check availability of the collider data
      if (!bodyBone->GetBoneColliderCount ())
      {
	factory->manager->Report
	  (CS_REPORTER_SEVERITY_ERROR,
	   "No colliders defined for bone %i while creating rigid body.",
	   bodyBone->GetAnimeshBone ());
	return;
      }

      // Create the rigid body
      boneData->rigidBody = dynamicSystem->CreateBody ();

      // Set the transform of the body
      csQuaternion rotation;
      csVector3 offset;
      skeleton->GetTransformAbsSpace (boneData->boneID, rotation, offset);
      // TODO: we shouldn't have to use the conjugate of the quaternion, isn't it?
      csOrthoTransform boneTransform (csMatrix3 (rotation.GetConjugate ()), offset);
      csOrthoTransform animeshTransform = sceneNode->GetMovable ()->GetFullTransform ();
      csOrthoTransform bodyTransform = boneTransform * animeshTransform;
      boneData->rigidBody->SetTransform (bodyTransform);

      // Set the properties of the body if they are defined
      // (with the Bullet plugin, it is more efficient to define it before the colliders)
      CS::Animation::iBodyBoneProperties* properties = bodyBone->GetBoneProperties ();
      if (properties)
	boneData->rigidBody->SetProperties (properties->GetMass (),
				  properties->GetCenter (),
				  properties->GetInertia ());

      // Attach the bone colliders
      for (uint index = 0; index < bodyBone->GetBoneColliderCount (); index++)
      {
	CS::Animation::iBodyBoneCollider* collider = bodyBone->GetBoneCollider (index);

	switch (collider->GetGeometryType ())
	{
	case BOX_COLLIDER_GEOMETRY:
	  {
	    csVector3 boxSize;
	    collider->GetBoxGeometry (boxSize);
	    boneData->rigidBody->AttachColliderBox
	      (boxSize, collider->GetTransform (), collider->GetFriction (),
	       collider->GetDensity (), collider->GetElasticity (),
	       collider->GetSoftness ());
	    break;
	  }

	case CYLINDER_COLLIDER_GEOMETRY:
	  {
	    float length, radius;
	    collider->GetCylinderGeometry (length, radius);
	    boneData->rigidBody->AttachColliderCylinder
	      (length, radius, collider->GetTransform (), collider->GetFriction (),
	       collider->GetDensity (), collider->GetElasticity (),
	       collider->GetSoftness ());
	    break;
	  }

	case CAPSULE_COLLIDER_GEOMETRY:
	  {
	    float length, radius;
	    collider->GetCapsuleGeometry (length, radius);
	    boneData->rigidBody->AttachColliderCapsule
	      (length, radius, collider->GetTransform (), collider->GetFriction (),
	       collider->GetDensity (), collider->GetElasticity (),
	       collider->GetSoftness ());
	    break;
	  }

	case CONVEXMESH_COLLIDER_GEOMETRY:
	  {
	    iMeshWrapper* mesh;
	    collider->GetConvexMeshGeometry (mesh);
	    boneData->rigidBody->AttachColliderConvexMesh
	      (mesh, collider->GetTransform (), collider->GetFriction (),
	       collider->GetDensity (), collider->GetElasticity (),
	       collider->GetSoftness ());
	    break;
	  }

	case TRIMESH_COLLIDER_GEOMETRY:
	  {
	    iMeshWrapper* mesh;
	    collider->GetMeshGeometry (mesh);
	    boneData->rigidBody->AttachColliderMesh
	      (mesh, collider->GetTransform (), collider->GetFriction (),
	       collider->GetDensity (), collider->GetElasticity (),
	       collider->GetSoftness ());
	    break;
	  }

	case PLANE_COLLIDER_GEOMETRY:
	  {
	    csPlane3 plane;
	    collider->GetPlaneGeometry (plane);
	    // TODO: add transform
	    boneData->rigidBody->AttachColliderPlane
	      (plane, collider->GetFriction (),
	       collider->GetDensity (), collider->GetElasticity (),
	       collider->GetSoftness ());
	    break;
	  }

	case SPHERE_COLLIDER_GEOMETRY:
	  {
	    float radius;
	    collider->GetSphereGeometry (radius);
	    boneData->rigidBody->AttachColliderSphere
	      (radius, collider->GetTransform ().GetOrigin (),
	       collider->GetFriction (), collider->GetDensity (),
	       collider->GetElasticity (), collider->GetSoftness ());
	    break;
	  }

	default:
	  factory->manager->Report (CS_REPORTER_SEVERITY_WARNING,
	    "No supported geometry for collider in bone %i while creating rigid body.",
				    bodyBone->GetAnimeshBone ());

	  dynamicSystem->RemoveBody (boneData->rigidBody);
	  boneData->rigidBody = 0;
	}
      }
    }

    // If the bone is in dynamic state
    if (boneData->state == CS::Animation::STATE_DYNAMIC)
    {
      // Set the rigid body in dynamic state
      boneData->rigidBody->MakeDynamic ();

      // If there was already a rigid body then update its position
      if (previousBody)
      {
	csQuaternion rotation;
	csVector3 offset;
	skeleton->GetTransformAbsSpace (boneData->boneID, rotation, offset);
	csOrthoTransform boneTransform (csMatrix3 (rotation.GetConjugate ()), offset);
	csOrthoTransform animeshTransform = sceneNode->GetMovable ()->GetFullTransform ();
	csOrthoTransform bodyTransform = boneTransform * animeshTransform;
	boneData->rigidBody->SetTransform (bodyTransform);
      }

      // Prepare for adding a joint
      // Check if the joint has already been defined
      if (boneData->joint)
	return;

      // Check that a joint definition is present
      if (!bodyBone->GetBoneJoint ())
	return;

      // Check if there is a parent bone to attach a joint to
      CS::Animation::BoneID parentBoneID = skeleton->GetFactory ()->GetBoneParent (boneData->boneID);
      if (parentBoneID == CS::Animation::InvalidBoneID)
	return;

      // Check if a CS::Animation::iBodyBone has been defined for the parent bone
      const BoneData* parentBoneData = bones.GetElementPointer (parentBoneID);
      if (!parentBoneData)
	return;

      // Create the dynamic joint
      boneData->joint = dynamicSystem->CreateJoint ();
      boneData->joint->SetBounce (bodyBone->GetBoneJoint ()->GetBounce (), false);
      boneData->joint->SetRotConstraints (bodyBone->GetBoneJoint ()->IsXRotConstrained (),
					  bodyBone->GetBoneJoint ()->IsYRotConstrained (),
					  bodyBone->GetBoneJoint ()->IsZRotConstrained (),
					  false);
      boneData->joint->SetTransConstraints (bodyBone->GetBoneJoint ()->IsXTransConstrained (),
					    bodyBone->GetBoneJoint ()->IsYTransConstrained (),
					    bodyBone->GetBoneJoint ()->IsZTransConstrained (),
					    false);

      boneData->joint->SetMaximumAngle (bodyBone->GetBoneJoint ()->GetMaximumAngle (), false);
      boneData->joint->SetMaximumDistance (bodyBone->GetBoneJoint ()->GetMaximumDistance (),
					   false);
      boneData->joint->SetMinimumAngle (bodyBone->GetBoneJoint ()->GetMinimumAngle (), false);
      boneData->joint->SetMinimumDistance (bodyBone->GetBoneJoint ()->GetMinimumDistance (),
					   false);

      // Setup the transform of the joint 
      csQuaternion rotation; 
      csVector3 offset; 

      // TODO: GetTransformBindSpace seems to return a wrong data 
      //skeleton->GetTransformBindSpace (boneData->boneID, rotation, offset); 
      //csOrthoTransform jointTransform (csMatrix3 (rotation.GetConjugate ()), offset); 

      skeleton->GetTransformBoneSpace (boneData->boneID, rotation, offset); 
      csOrthoTransform boneTransform (csMatrix3 (rotation.GetConjugate ()), offset); 
      skeleton->GetFactory ()->GetTransformBoneSpace (boneData->boneID, rotation, offset); 
      csOrthoTransform boneSTransform (csMatrix3 (rotation.GetConjugate ()), offset); 
      boneData->joint->SetTransform (bodyBone->GetBoneJoint ()->GetTransform () * 
				     boneSTransform * boneTransform.GetInverse()); 

      // Attach the rigid bodies to the joint
      boneData->joint->Attach (parentBoneData->rigidBody, boneData->rigidBody, false);
      boneData->joint->RebuildJoint ();

      return;
    }

    // If the bone is in kinematic state
    else if (boneData->state == CS::Animation::STATE_KINEMATIC)
    {
      // Find the bullet interface of the rigid body
      csRef<CS::Physics::Bullet::iRigidBody> bulletBody =
	scfQueryInterface<CS::Physics::Bullet::iRigidBody> (boneData->rigidBody);

      if (!bulletBody)
      {
	factory->manager->Report
	  (CS_REPORTER_SEVERITY_WARNING,
	   "No Bullet plugin while setting bone %i kinematic.",
	   bodyBone->GetAnimeshBone ());
	return;
      }

      // Set a bone kinematic callback
      csRef<BoneKinematicCallback> ref;
      ref.AttachNew (new BoneKinematicCallback (this, boneData->boneID));
      bulletBody->SetKinematicCallback (ref);

      // Set the rigid body in kinematic state
      bulletBody->MakeKinematic ();

      // Remove the joint
      if (boneData->joint)
      {
	dynamicSystem->RemoveJoint (boneData->joint);
	boneData->joint = 0;
      }
    }
  }

  void RagdollNode::InitBoneStates ()
  {
    // Update the state of all bones (iterate in increasing order of the bone ID's
    // so that the parent bones are always updated before their children)
    for (size_t i = 0; i <= maxBoneID; i++)
    {
      if (!bones.Contains ((CS::Animation::BoneID)i))
        continue;

      BoneData nullBone;
      BoneData& boneData = bones.Get ((CS::Animation::BoneID)i, nullBone);
      UpdateBoneState (&boneData);
    }
  }

  void RagdollNode::ResetChainNodeTransform (CS::Animation::iBodyChainNode* node)
  {
    // TODO: simply re-call UpdateSkeleton()?

    // Find the associated bone data
    BoneData nullBone;
    BoneData& boneData = bones.Get (node->GetAnimeshBone (), nullBone);
    
    // Compute the bind transform of the bone
    csQuaternion boneRotation;
    csVector3 boneOffset;
    skeleton->GetFactory ()->GetTransformBoneSpace (boneData.boneID, boneRotation,
						   boneOffset);
    csOrthoTransform bodyTransform (csMatrix3 (boneRotation.GetConjugate ()),
				    boneOffset);

    CS::Animation::BoneID parentBoneID = skeleton->GetFactory ()->GetBoneParent (boneData.boneID);

    // If the parent bone is a rigid body then take the parent transform from it
    if (bones.Contains (parentBoneID))
    {
      csOrthoTransform parentTransform =
      bones.Get (parentBoneID, nullBone).rigidBody->GetTransform ();
      bodyTransform = bodyTransform * parentTransform;
    }

    // Else take the parent transform from the skeleton state
    else if (parentBoneID != CS::Animation::InvalidBoneID)
    {
      skeleton->GetTransformAbsSpace (parentBoneID, boneRotation,
				      boneOffset);
      csOrthoTransform parentTransform (csMatrix3 (boneRotation.GetConjugate ()),
					boneOffset);

      bodyTransform = bodyTransform * parentTransform
	* sceneNode->GetMovable ()->GetFullTransform ();
    }

    // Apply the transform to the rigid body
    boneData.rigidBody->SetTransform (bodyTransform);
    boneData.rigidBody->SetLinearVelocity (csVector3 (0.0f));
    boneData.rigidBody->SetAngularVelocity (csVector3 (0.0f));

    // Update the transform of the children nodes
    for (uint i = 0; i < node->GetChildCount (); i++)
      ResetChainNodeTransform (node->GetChild (i));
  }

  /********************
   *  BoneKinematicCallback
   ********************/

  BoneKinematicCallback::BoneKinematicCallback (RagdollNode* ragdollNode,
						CS::Animation::BoneID boneID)
    : scfImplementationType (this), ragdollNode (ragdollNode), boneID (boneID)
  {
  }

  BoneKinematicCallback::~BoneKinematicCallback ()
  {
  }

  void BoneKinematicCallback::GetBodyTransform
    (iRigidBody* body, csOrthoTransform& transform) const
  {
    // Read the position of the kinematic body from the skeleton state
    csQuaternion boneRotation;
    csVector3 boneOffset;
    ragdollNode->skeleton->GetTransformAbsSpace (boneID, boneRotation, boneOffset);
    transform.SetO2T (csMatrix3 (boneRotation.GetConjugate ()));
    transform.SetOrigin (boneOffset);
    csOrthoTransform animeshTransform =
      ragdollNode->sceneNode->GetMovable ()->GetFullTransform ();
    transform = transform * animeshTransform;
  }

}
CS_PLUGIN_NAMESPACE_END(Ragdoll)
