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
#ifndef __CS_IMESH_ANIMNODE_NOISE_H__
#define __CS_IMESH_ANIMNODE_NOISE_H__

/**\file
 * Noise animation node for an animated mesh.
 */

#include "csutil/scf_interface.h"
#include "imesh/animnode/skeleton2anim.h"

namespace CS {
namespace Math {
namespace Noise {
namespace Module {

class Module;

} // namespace Module
} // namespace Noise
} // namespace Math
} // namespace CS

/**\addtogroup meshplugins
 * @{ */

namespace CS {
namespace Animation {

struct iSkeletonNoiseNodeFactory;

/**
 * A class to manage the creation and deletion of noise animation 
 * node factories.
 */
struct iSkeletonNoiseNodeManager
  : public virtual CS::Animation::iSkeletonAnimNodeManager<CS::Animation::iSkeletonNoiseNodeFactory>
{
  SCF_ISKELETONANIMNODEMANAGER_INTERFACE (CS::Animation::iSkeletonNoiseNodeManager, 1, 0, 0);
};

// ----------------------------- iSkeletonNoiseNode -----------------------------

/**
 * The noise channels to be used by the CS::Animation::iSkeletonNoiseNode.
 */
enum SkeletonNoiseChannel
{
  NOISE_ROTATION,   /*!< The noise functions affect the rotational channel of the
		      bone. Each component of the SkeletonNoise will be
		      interpreted respectively as the X, Y and Z Euler angles
		      of the rotation.*/
  NOISE_POSITION    /*!< The noise functions affect the positional channel of the
		      bone. Each component of the SkeletonNoise will be
		      interpreted respectively as the X, Y and Z components of the
		      positional vector.*/
};

/**
 * A structure holding the noise components for a iSkeletonNoiseNode
 * animation node. The structure can hold up to three noise components.
 * It is allowed to not define some of these components, in this case
 * the corresponding animation channel axis won't be animated.
 */
class SkeletonNoise : public csRefCount
{
public:

  /// Constructor
  SkeletonNoise ()
  {
    components[0] = nullptr;
    components[1] = nullptr;
    components[2] = nullptr;
  }

  /// Destructor
  virtual inline ~SkeletonNoise () {}

  /**
   * Set the noise component at the given index.
   * \param index The index of the noise component. It must be smaller than 3.
   * \param module The noise module controlling the given component.
   */
  inline void SetComponent (size_t index, CS::Math::Noise::Module::Module* module)
  {
    CS_ASSERT (index < 3);
    components[index] = module;
  }

  /**
   * Get the noise component at the given index.
   * \param index The index of the noise component. It must be smaller than 3.
   * \return The noise component at the given index, or \a nullptr if this
   * component is not defined.
   */
  inline CS::Math::Noise::Module::Module* GetComponent (size_t index)
  {
    CS_ASSERT (index < 3);
    return components[index];
  }

protected:
  CS::Math::Noise::Module::Module* components[3];
};

/**
 * Factory for the 'noise' animation node (see CS::Animation::iSkeletonNoiseNode).
 *
 * This animation node controls the animation of the bones through
 * pseudo-random noise functions from CS::Math::Noise.
 */
struct iSkeletonNoiseNodeFactory : public virtual iSkeletonAnimNodeFactory
{
  SCF_INTERFACE(CS::Animation::iSkeletonNoiseNodeFactory, 1, 0, 0);

  /**
   * Add a skeleton noise to be registered by this animation node.
   * Afterward, the noise module can be referenced through AddBoneNoise()
   * in order to control the animation of a given bone.
   * \return The index of the skeleton noise.
   */
  virtual size_t AddSkeletonNoise (SkeletonNoise* noise) = 0;

  /**
   * Add a bone noise in order to control the animation of the bone.
   * \param bone The ID of the bone to be animated.
   * \param noiseIndex The index of the skeleton noise controlling
   * the animation of the bone. This index is the value returned by
   * AddSkeletonNoise().
   * \param channel The channel of the bone that will be controlled
   * by the skeleton noise.
   * \param x The X coordinate that will be used to sample the noise
   * components of the given SkeletonNoise. The Z coordinate is the
   * current time stamp since the start of the animation node.
   * \param y The Y coordinate that will be used to sample the noise
   * components of the given SkeletonNoise. The Z coordinate is the
   * current time stamp since the start of the animation node.
   * \param weight The weight to apply on the values sampled from the
   * noise components of the given SkeletonNoise. Each component of
   * the vector is associated with each component of the skeleton noise.
   */
  virtual void AddBoneNoise (BoneID bone, size_t noiseIndex,
			     SkeletonNoiseChannel channel, float x, float y,
			     csVector3 weight = csVector3 (1.0f)) = 0;

  /**
   * Set the child animation node of this node. It is valid to set a null 
   * reference as chid node.
   */
  virtual void SetChildNode (CS::Animation::iSkeletonAnimNodeFactory* factory) = 0;

  /**
   * Get the child animation node of this node.
   */
  virtual iSkeletonAnimNodeFactory* GetChildNode () const = 0;
};

/**
 * An animation node that controls the animation of the bones through
 * pseudo-random noise functions from CS::Math::Noise.
 */
struct iSkeletonNoiseNode : public virtual iSkeletonAnimNode
{
  SCF_INTERFACE(iSkeletonNoiseNode, 2, 0, 0);
};

} // namespace Animation
} // namespace CS

/** @} */

#endif //__CS_IMESH_ANIMNODE_NOISE_H__
