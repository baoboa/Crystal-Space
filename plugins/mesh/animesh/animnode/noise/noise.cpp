/*
  Copyright (C) 2011 Christian Van Brussel, Institute of Information
      and Communication Technologies, Electronics and Applied Mathematics
      at Universite catholique de Louvain, Belgium
      http://www.uclouvain.be/en-icteam.html

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

#include "cssysdef.h"
#include "csutil/scf.h"

#include "csgeom/math.h"
#include "iutil/objreg.h"
#include "noise.h"

CS_PLUGIN_NAMESPACE_BEGIN(NoiseNode)
{
  SCF_IMPLEMENT_FACTORY(NoiseNodeManager);

  // --------------------------  NoiseNodeFactory  --------------------------

  CS_LEAKGUARD_IMPLEMENT(NoiseNodeFactory);

  NoiseNodeFactory::NoiseNodeFactory (NoiseNodeManager* manager, const char *name)
    : scfImplementationType (this), CS::Animation::SkeletonAnimNodeFactorySingle (name),
    manager (manager)
  {
  }

  size_t NoiseNodeFactory::AddSkeletonNoise (CS::Animation::SkeletonNoise* noise)
  {
    return skeletonNoises.Push (noise);
  }

  void NoiseNodeFactory::AddBoneNoise (CS::Animation::BoneID bone, size_t noiseIndex,
				       CS::Animation::SkeletonNoiseChannel channel,
				       float x, float y, csVector3 weight)
  {
    CS_ASSERT (noiseIndex < skeletonNoises.GetSize ());
    BoneNoise noise;
    noise.bone = bone;
    noise.noiseIndex = noiseIndex;
    noise.channel = channel;
    noise.x = x;
    noise.y = y;
    noise.weight = weight;
    boneNoises.Push (noise);
  }

  csPtr<CS::Animation::SkeletonAnimNodeSingleBase> NoiseNodeFactory::ActualCreateInstance (
    CS::Animation::iSkeletonAnimPacket* packet,
    CS::Animation::iSkeleton* skeleton)
  {
    return csPtr<CS::Animation::SkeletonAnimNodeSingleBase> (new NoiseNode (this, skeleton));
  }

  // --------------------------  NoiseNode  --------------------------

  CS_LEAKGUARD_IMPLEMENT(NoiseNode);

  NoiseNode::NoiseNode (NoiseNodeFactory* factory, CS::Animation::iSkeleton* skeleton)
    : scfImplementationType (this),
    CS::Animation::SkeletonAnimNodeSingle<NoiseNodeFactory> (factory, skeleton)
  {
  }

  void NoiseNode::Play ()
  {
    SkeletonAnimNodeSingleBase::Play ();

    ticks = 0.0f;
  }

  void NoiseNode::BlendState (CS::Animation::AnimatedMeshState* state, float baseWeight)
  {
    // Check that this node is active
    if (!isPlaying)
      return;

    // Make the child node blend the state
    if (childNode)
      childNode->BlendState (state, baseWeight);

    for (size_t i = 0; i < factory->boneNoises.GetSize () - 1; i++)
    {
      BoneNoise& boneNoise = factory->boneNoises[i];
      CS_ASSERT (skeleton->GetFactory ()->HasBone (boneNoise.bone));

      CS::Animation::SkeletonNoise* skeletonNoise =
	factory->skeletonNoises[boneNoise.noiseIndex];

      // Compute the noise vector
      csVector3 noiseVector (0.0f);

      CS::Math::Noise::Module::Module* module = skeletonNoise->GetComponent (0);
      if (module) noiseVector[0] = module->GetValue (boneNoise.x, boneNoise.y, ticks)
		    * boneNoise.weight[0];

      module = skeletonNoise->GetComponent (1);
      if (module) noiseVector[1] = module->GetValue (boneNoise.x, boneNoise.y, ticks)
		    * boneNoise.weight[1];

      module = skeletonNoise->GetComponent (2);
      if (module) noiseVector[2] = module->GetValue (boneNoise.x, boneNoise.y, ticks)
		    * boneNoise.weight[2];

      // Initialize the bone data if needed
      if (!state->IsBoneUsed (boneNoise.bone))
      {
	state->SetBoneUsed (boneNoise.bone);
	state->GetQuaternion (boneNoise.bone).SetIdentity ();
	state->GetVector (boneNoise.bone) = csVector3 (0.0f);
      }

      // Apply the noise vector
      if (boneNoise.channel == CS::Animation::NOISE_ROTATION)
      {
	csQuaternion rotation;
	rotation.SetEulerAngles (noiseVector);
	csQuaternion& quaternion = state->GetQuaternion (boneNoise.bone);
	quaternion = quaternion.SLerp (rotation, baseWeight);
      }

      else
      {
	csVector3& v = state->GetVector (boneNoise.bone);
	v = csLerp (v, noiseVector, baseWeight);
      }
    }
  }

  void NoiseNode::TickAnimation (float dt)
  {
    ticks += dt * playbackSpeed;

    if (childNode)
      childNode->TickAnimation (dt);
  }

}
CS_PLUGIN_NAMESPACE_END(NoiseNode)
