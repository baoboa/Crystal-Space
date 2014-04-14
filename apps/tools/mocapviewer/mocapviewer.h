/*
  Copyright (C) 2010-2012 Christian Van Brussel, Institute of Information
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

#ifndef __MOCAPVIEWER_H__
#define __MOCAPVIEWER_H__

#include "cstool/demoapplication.h"
#include "cstool/mocapparser.h"
#include "cstool/noise/noise.h"
#include "cstool/noise/noisegen.h"
#include "imesh/bodymesh.h"
#include "imesh/animnode/debug.h"
#include "imesh/animnode/noise.h"
#include "imesh/animnode/retarget.h"
#include "imesh/animnode/skeleton2anim.h"

#define SEED_COUNT 10

struct iMovieRecorder;
class csPixmap;

class MocapViewer : public CS::Utility::DemoApplication,
  public scfImplementation1<MocapViewer, CS::Animation::iSkeletonAnimCallback>
{
  enum DisplayMode
  {
    DISPLAY_WIRE = 0,
    DISPLAY_PLD,
    DISPLAY_ELLIPSE
  };

 private:
  bool CreateScene ();

  // References to animesh objects
  csRef<iMovieRecorder> movieRecorder;
  csRef<CS::Animation::iBodyManager> bodyManager;
  csRef<CS::Animation::iSkeletonDebugNodeManager> debugNodeManager;
  csRef<CS::Animation::iSkeletonNoiseNodeManager> noiseNodeManager;
  csRef<CS::Animation::iSkeletonRetargetNodeManager> retargetNodeManager;
  csRef<CS::Animation::iSkeletonDebugNode> debugNode;
  csRef<CS::Animation::iSkeletonAnimNode> animNode;
  csRef<CS::Mesh::iAnimatedMesh> animesh;
  csRef<iMeshWrapper> meshWrapper;
  CS::Animation::MocapParserResult parsingResult;
  CS::Animation::BoneID targetBoneID;

  // Display of information
  csPixmap* debugImage;
  bool printInfo;

  // Random number generation
  unsigned int seeds[SEED_COUNT];

  // Noise points
  CS::Math::Noise::Module::Perlin noiseX;
  CS::Math::Noise::Module::Perlin noiseY;
  csArray<csVector3> noisePoints;
  float noiseScale;

  // Skeleton noise
  CS::Math::Noise::Module::Perlin snoiseX;
  CS::Math::Noise::Module::Perlin snoiseY;
  CS::Math::Noise::Module::Perlin snoiseZ;

  // Scene duration
  csTicks duration;

 public:
  MocapViewer ();
  ~MocapViewer ();

  //-- CS::Utility::DemoApplication
  void PrintHelp ();
  void Frame ();

  //-- csApplicationFramework
  bool OnInitialize (int argc, char* argv[]);
  bool Application ();

  //-- CS::Animation::iSkeletonAnimCallback
  void AnimationFinished (CS::Animation::iSkeletonAnimNode* node);
  void AnimationCycled (CS::Animation::iSkeletonAnimNode* node) {}
  void PlayStateChanged (CS::Animation::iSkeletonAnimNode* node, bool isPlaying) {}
  void DurationChanged (CS::Animation::iSkeletonAnimNode* node) {}
};

#endif // __MOCAPVIEWER_H__
