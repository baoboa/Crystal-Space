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
#ifndef __CS_NOISENODE_H__
#define __CS_NOISENODE_H__

#include "csutil/scf_implementation.h"
#include "cstool/animnodetmpl.h"
#include "cstool/noise/noise.h"
#include "cstool/noise/noisegen.h"
#include "csutil/leakguard.h"
#include "csutil/weakref.h"
#include "csutil/refarr.h"
#include "iutil/comp.h"
#include "imesh/animnode/noise.h"

CS_PLUGIN_NAMESPACE_BEGIN(NoiseNode)
{
  class NoiseNodeManager;

  struct BoneNoise
  {
    CS::Animation::BoneID bone;
    size_t noiseIndex;
    CS::Animation::SkeletonNoiseChannel channel;
    float x;
    float y;
    csVector3 weight;
  };

  class NoiseNodeFactory
    : public scfImplementation2<NoiseNodeFactory, 
    scfFakeInterface<CS::Animation::iSkeletonAnimNodeFactory>,
    CS::Animation::iSkeletonNoiseNodeFactory>,
    public CS::Animation::SkeletonAnimNodeFactorySingle
  {
  public:
    CS_LEAKGUARD_DECLARE(NoiseAnimNodeFactory);

    NoiseNodeFactory (NoiseNodeManager* manager, const char *name);
    ~NoiseNodeFactory () {}

    //-- CS::Animation::iSkeletonNoiseNodeFactory
    virtual size_t AddSkeletonNoise (CS::Animation::SkeletonNoise* noise);
    virtual void AddBoneNoise (CS::Animation::BoneID bone, size_t noiseIndex,
			       CS::Animation::SkeletonNoiseChannel channel,
			       float x, float y, csVector3 weight = csVector3 (1.0f));

    inline virtual void SetChildNode (CS::Animation::iSkeletonAnimNodeFactory* factory)
    { CS::Animation::SkeletonAnimNodeFactorySingle::SetChildNode (factory); }
    inline virtual iSkeletonAnimNodeFactory* GetChildNode () const
    { return CS::Animation::SkeletonAnimNodeFactorySingle::GetChildNode (); }

    //-- CS::Animation::SkeletonAnimNodeFactorySingle
    csPtr<CS::Animation::SkeletonAnimNodeSingleBase> ActualCreateInstance (
      CS::Animation::iSkeletonAnimPacket* packet, CS::Animation::iSkeleton* skeleton);

  private:
    NoiseNodeManager* manager;
    csRefArray<CS::Animation::SkeletonNoise> skeletonNoises;
    csArray<BoneNoise> boneNoises;

    friend class NoiseNode;
  };

  class NoiseNode
    : public scfImplementation2<NoiseNode, 
				scfFakeInterface<CS::Animation::iSkeletonAnimNode>,
				CS::Animation::iSkeletonNoiseNode>,
      public CS::Animation::SkeletonAnimNodeSingle<NoiseNodeFactory>
  {
  public:
    CS_LEAKGUARD_DECLARE(NoiseNode);

    NoiseNode (NoiseNodeFactory* factory, CS::Animation::iSkeleton* skeleton);
    ~NoiseNode () {}

    //-- CS::Animation::iSkeletonNoiseNode

    //-- CS::Animation::iSkeletonAnimNode
    virtual void Play ();

    virtual void BlendState (CS::Animation::AnimatedMeshState* state,
			     float baseWeight = 1.0f);
    virtual void TickAnimation (float dt);

  private:
    float ticks;

    friend class NoiseNodeFactory;
  };

  class NoiseNodeManager
    : public CS::Animation::AnimNodeManagerCommon<NoiseNodeManager,
						  CS::Animation::iSkeletonNoiseNodeManager,
						  NoiseNodeFactory>
  {
  public:
    NoiseNodeManager (iBase* parent)
     : AnimNodeManagerCommonType (parent) {}
  };

}
CS_PLUGIN_NAMESPACE_END(NoiseNode)

#endif // __CS_NOISENODE_H__
