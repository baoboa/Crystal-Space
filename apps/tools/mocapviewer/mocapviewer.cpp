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

#include "cssysdef.h"
#include "mocapviewer.h"
#include "csutil/floatrand.h"
#include "csutil/randomgen.h"
#include "cstool/animeshtools.h"
#include "cstool/cspixmap.h"
#include "imesh/animesh.h"
#include "iutil/cfgmgr.h"
#include "ivaria/movierecorder.h"

MocapViewer::MocapViewer ()
  : DemoApplication ("CrystalSpace.MocapViewer"),
    scfImplementationType (this), targetBoneID (CS::Animation::InvalidBoneID),
    debugImage (nullptr), noiseScale (0.5f), duration (0)
{
}

MocapViewer::~MocapViewer ()
{
  delete debugImage;
}

void MocapViewer::PrintHelp ()
{
  csCommandLineHelper commandLineHelper;

  // Usage examples
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer /lib/krystal/mocap/idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer C:\\CS\\data\\krystal\\mocap\\idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -start=20 -end=60 -scale=0.1 idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -rootmask=Hips idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -rootmask=Hips -childmask=Head -childmask=LeftHand idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -rootmask=Hips -childall idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh -display=pld -ncount=200 -nfrequency=0.4 -poscamera=0.7");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -display=pld -record -recordfile=mocap.nuv idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -rotcamera=-90 idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -rotskeleton=180 -lookat=Head idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh -offxcamera=1.0f -offycamera=1.0f");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh -boneswitch=Neck:LeftArm -boneswitch=Head:RightUpLeg");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh -rootmask=Hips -childall -randomseed=9372109 -boneswitchall");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh -timeshift=Head:1 -timeshift=Hips:-1.5 -duration=5.5");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer -targetfile=data/krystal/krystal.xml -targetname=krystal -nobone idle01.bvh");
  commandLineHelper.AddCommandLineExample
    ("csmocapviewer idle01.bvh -rootmask=Hips -childall -snoise -snfrequency=0.2 -snoctaves=2 -snratio=1.0 -display=pld -sntranslation=0.8");

  // Command line options
  size_t section = commandLineHelper.AddCommandLineSection ("Animation playback");
  commandLineHelper.AddCommandLineOption
    ("info", "Parse the file, print out the mocap data information, then exit", csVariant (), section);
  commandLineHelper.AddCommandLineOption
    ("noanim", "Don't play the animation, only display the skeleton in rest pose", csVariant (), section);
  commandLineHelper.AddCommandLineOption
    ("start", "Set the index of the start frame", csVariant (0), section);
  commandLineHelper.AddCommandLineOption
    ("end", "Set the index of the end frame", csVariant (0), section);
  commandLineHelper.AddCommandLineOption
    ("scale", "Set the global scale to apply to the distances", csVariant (0.01f), section);
  commandLineHelper.AddCommandLineOption
    ("speed", "Set the speed to play the animation", csVariant (1.0f), section);
  commandLineHelper.AddCommandLineOption
    ("duration", "Set the time length to play the animation, in seconds. The application will quit afterward.",
     csVariant (0.0f), section);

  section = commandLineHelper.AddCommandLineSection ("Display");
  commandLineHelper.AddCommandLineOption
    ("display", csString ().Format ("Set the display mode of the skeleton. Allowed values are %s, %s, and %s",
				    CS::Quote::Single ("wire"), CS::Quote::Single ("pld"), CS::Quote::Single ("ellipse")),
     csVariant ("wire"), section);
  commandLineHelper.AddCommandLineOption
    ("rotskeleton", "Rotate the skeleton of a given angle around the Z axis, in degree", csVariant (0.0f), section);
  commandLineHelper.AddCommandLineOption
    ("rotcamera", "Rotate the camera of a given angle around the Y axis, in degree", csVariant (0.0f), section);
  commandLineHelper.AddCommandLineOption
    ("poscamera", "Scale the distance between the camera and the target", csVariant (1.0f), section);
  commandLineHelper.AddCommandLineOption
    ("offxcamera", "Have the camera look originally at the given X offset", csVariant (0.0f), section);
  commandLineHelper.AddCommandLineOption
    ("offycamera", "Have the camera look originally at the given Y offset", csVariant (0.0f), section);
  commandLineHelper.AddCommandLineOption
    ("lookat", "Keep the camera looking at the given bone", csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("randomseed", "Global seed for the random number generation", csVariant (0), section);

  section = commandLineHelper.AddCommandLineSection ("Animesh retargeting");
  commandLineHelper.AddCommandLineOption
    ("targetfile", "Set the file of the animesh to retarget the animation to", csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("targetname", "Set the name of the animesh factory to retarget the animation to", csVariant (""), section);

  section = commandLineHelper.AddCommandLineSection ("Bone filtering");
  commandLineHelper.AddCommandLineOption
    ("rootmask", "Set the bone name of the root of the bone chain that will be used as a mask", csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("childmask", "Add a child to the bone chain that will be used as a mask", csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("childall", "Add all sub-children of the root bone to the bone chain that will be used as a mask", csVariant (), section);
  commandLineHelper.AddCommandLineOption
    ("bone", "Add a bone to be displayed by its name", csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("nobone", "Don't display any bone at all", csVariant (), section);
  commandLineHelper.AddCommandLineOption
    ("nobone", "Remove a bone to be displayed by its name", csVariant (""), section);

  section = commandLineHelper.AddCommandLineSection ("Animation data manipulation");
  commandLineHelper.AddCommandLineOption
    ("nomove", "Don't apply the horizontal motion on the given bone", csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("boneswitch", csString ().Format
     ("Define two bones that will have their animation channels switched. The entry must be of the format %s", "boneName:boneName"),
     csVariant (""), section);
  commandLineHelper.AddCommandLineOption
    ("boneswitchall", "Switch randomly the animations of all the bones of the skeleton", csVariant (), section);
  commandLineHelper.AddCommandLineOption
    ("scramble", "Scramble ratio to be applied on the position of the bones. Value must be positive", csVariant (0.0f), section);
  commandLineHelper.AddCommandLineOption
    ("timeshift", csString ().Format
     ("Define a time shift to be applied on the animation of a given bone. The entry must be of the format %s", "boneName:timeShift"),
     csVariant (""), section);

  section = commandLineHelper.AddCommandLineSection ("Noise cloud generation");
  commandLineHelper.AddCommandLineOption
    ("ncount", "Set the number of noise points added", csVariant (0), section);
  commandLineHelper.AddCommandLineOption
    ("nscale", "Scale to apply on the position of the noise points", csVariant (0.5f), section);
  commandLineHelper.AddCommandLineOption
    ("noctaves", "Set the number of octaves of the noise. Value must be between 1 and 30", csVariant (6), section);
  commandLineHelper.AddCommandLineOption
    ("nfrequency", "Set the frequency of the noise. Value must be positive", csVariant (1.0f), section);
  commandLineHelper.AddCommandLineOption
    ("nlacunarity", "Set the lacunarity of the noise. Value is suggested to be between 1.5 and 3.5", csVariant (2.0f), section);
  commandLineHelper.AddCommandLineOption
    ("npersistence", "Set the persistence of the noise. Value is suggested to be between 0.0 and 1.0", csVariant (0.5f), section);

  section = commandLineHelper.AddCommandLineSection ("Skeleton noise generation");
  commandLineHelper.AddCommandLineOption
    ("snoise", "Use a noise animation of the skeleton instead of the motion captured data",
     csVariant (false), section);
  commandLineHelper.AddCommandLineOption
    ("snratio", "Ratio to apply on the noise skeleton animation versus the motion captured data. Value is suggested to be between 0.0 and 1.0", csVariant (1.0f), section);
  commandLineHelper.AddCommandLineOption
    ("snoctaves", "Set the number of octaves of the skeleton noise. Value must be between 1 and 30", csVariant (6), section);
  commandLineHelper.AddCommandLineOption
    ("snfrequency", "Set the frequency of the skeleton noise. Value must be positive", csVariant (1.0f), section);
  commandLineHelper.AddCommandLineOption
    ("snlacunarity", "Set the lacunarity of the skeleton noise. Value is suggested to be between 1.5 and 3.5", csVariant (2.0f), section);
  commandLineHelper.AddCommandLineOption
    ("snpersistence", "Set the persistence of the skeleton noise. Value is suggested to be between 0.0 and 1.0", csVariant (0.5f), section);
  commandLineHelper.AddCommandLineOption
    ("sntranslation", "Set the noise ratio to be applied on the translation of the bones. Value must be positive", csVariant (1.0f), section);

  section = commandLineHelper.AddCommandLineSection ("Video recording");
  commandLineHelper.AddCommandLineOption
    ("record", "Record the session in a video file, then exit", csVariant (), section);
  commandLineHelper.AddCommandLineOption
    ("recordfile", "Force the name of the video file to be created", csVariant (""), section);

  // Printing help
  commandLineHelper.PrintApplicationHelp
    (GetObjectRegistry (),
     "csmocapviewer", "csmocapviewer [OPTIONS] [filename]", csString ().Format
     ("Crystal Space's viewer for motion captured data. This viewer supports currently"
      " only the Biovision Hierarchical data file format (BVH).\n\n"
      "The animation can be retargeted automatically to an animesh. Use either the -targetfile and"
      " -targetname options for that. The results will depend on the actual similitudes between the"
      " two skeletons.\n\n"
      "A mask can be defined to select the bones that are displayed. A simple way to populate the "
      "bone mask is by defining a bone chain. A bone chain is defined by a root bone (option "
      "-rootmask), then the user can add either all children and sub-children of the root bone (option"
      " -childall), either all bones on the way to a given child bone (option -childmask). Only one bone"
      " chain can be defined, but the -childmask option can be used additively. Some"
      " specific bones can be added and removed by using the -bone and -nobone options. If the only bone"
      " option provided is the empty -nobone option, then no bones at all will be displayed.\n\n"
      "This viewer can also be used as a Point Light Display system, and is able to record automatically"
      " videos with these data. This system has been designed as a base tool for a psychology study on "
      "the human perception of the motion.\n\n"
      "The Point Light Display can be perturbated by adding noise points animated by a Perlin"
      " noise. The behavior of the motion of these noise points can be tweaked by several"
      " parameters. See http://libnoise.sourceforge.net/docs/classnoise_1_1module_1_1Perlin.html"
      " for more information on these parameters.\n\n"
      "Instead of using the animation data from the motion capture file, the skeleton itself can be"
      " animated using a Perlin noise. In this case, the rotation of each bone joint will be animated"
      " randomly, while the distance between the joints will be kept constant. If you use this mode,"
      " you still have to provide a motion captured data file in order to define the skeleton to be"
      " used.\n\n"
      "The animation data can also be manipulated by either switching some bone animation channels,"
      " or by adding a time offset on the motion of a given bone.\n\n"
      "Finally, this application uses a configuration file. See %s"
      " for more information", CS::Quote::Double ("/config/csmocapviewer.cfg")));
}

void MocapViewer::Frame ()
{
  // Check for the end of the session's time length
  if (duration > 0 && duration < vc->GetCurrentTicks ())
  {
    csRef<iEventQueue> q (csQueryRegistry<iEventQueue> (GetObjectRegistry ()));
    if (q) q->GetEventOutlet()->Broadcast (csevQuit (GetObjectRegistry ()));
    return;
  }

  // Update manually the animation of the animesh since it is not in a sector
  // TODO: use engine flag ALWAYS_ANIMATE
  csVector3 position (0.0f);
  meshWrapper->GetMeshObject ()->NextFrame (vc->GetCurrentTicks (), position, 0);

  // Update the position of the camera target
  if (targetBoneID != CS::Animation::InvalidBoneID)
  {
    csQuaternion boneRotation;
    csVector3 boneOffset;
    animesh->GetSkeleton ()->GetTransformAbsSpace (targetBoneID, boneRotation, boneOffset);

    csOrthoTransform transform (csMatrix3 (boneRotation.GetConjugate ()),
				boneOffset);
    csVector3 position = (transform * meshWrapper->QuerySceneNode ()
			  ->GetMovable ()->GetTransform ()).GetOrigin ();

    cameraManager->SetCameraTarget (position);
  }

  // Default behavior from DemoApplication
  DemoApplication::Frame ();

  // Ask the debug node to display the data
  csColor color (0.0f, 8.0f, 0.0f);
  debugNode->Draw (view->GetCamera (), color);

  // Display the noise points
  int colorI = g2d->FindRGB (255.0f * color[0],
			     255.0f * color[1],
			     255.0f * color[2]);
  float seed0 = ((float) vc->GetCurrentTicks ()) / 10000.0f;
  for (csArray<csVector3>::Iterator it = noisePoints.GetIterator (); it.HasNext (); )
  {
    csVector3& point = it.Next ();

    float px = noiseX.GetValue (seed0 + point[0], point[1], point[2])
      * noiseScale * ((float) g2d->GetWidth ()) + ((float) g2d->GetWidth ()) * 0.5f;
    float py = noiseY.GetValue (seed0 + point[0], point[1], point[2])
      * noiseScale * ((float) g2d->GetHeight ()) + ((float) g2d->GetHeight ()) * 0.5f;

    // Display the debug image if available
    if (debugImage)
      debugImage->Draw (g3d, px - debugImage->Width () / 2,
			py - debugImage->Height () / 2);

    // Else display a square
    else
    {
      size_t size = 5;
      for (size_t i = 0; i < size; i++)
	for (size_t j = 0; j < size; j++)
	  g2d->DrawPixel (((int) px) - size / 2 + i,
			  ((int) py) - size / 2 + j,
			  colorI);
    }
  }

  // Update the HUD
  if (hudManager->GetEnabled ())
  {
    csString txt;
    txt.Format ("Frame: %i on %u",
		(int) (animNode->GetPlaybackPosition () / parsingResult.frameDuration),
		(int) (animNode->GetDuration () / parsingResult.frameDuration));
    hudManager->GetStateDescriptions ()->Put (0, txt);
  }
}

bool MocapViewer::OnInitialize (int argc, char* argv[])
{
  if (!csInitializer::RequestPlugins (GetObjectRegistry (),
    CS_REQUEST_PLUGIN ("crystalspace.utilities.movierecorder", iMovieRecorder),
    CS_REQUEST_END))
    return ReportError ("Failed to initialize plugins!");

  // Default behavior from DemoApplication
  if (!DemoApplication::OnInitialize (argc, argv))
    return false;

  // Load the needed plugins
  if (!csInitializer::RequestPlugins (GetObjectRegistry (),
    CS_REQUEST_PLUGIN ("crystalspace.mesh.animesh.body",
		       CS::Animation::iBodyManager),
    CS_REQUEST_PLUGIN ("crystalspace.mesh.animesh.animnode.debug",
		       CS::Animation::iSkeletonDebugNodeManager),
    CS_REQUEST_PLUGIN ("crystalspace.mesh.animesh.animnode.noise",
		       CS::Animation::iSkeletonNoiseNodeManager),
    CS_REQUEST_PLUGIN ("crystalspace.mesh.animesh.animnode.retarget",
		       CS::Animation::iSkeletonRetargetNodeManager),
    CS_REQUEST_END))
    return ReportError ("Failed to initialize plugins!");

  return true;
}

bool MocapViewer::Application ()
{
  // Default behavior from DemoApplication
  if (!DemoApplication::Application ())
    return false;

  // Find a reference to the video recorder plugin
  movieRecorder = csQueryRegistry<iMovieRecorder> (GetObjectRegistry ());
  if (!movieRecorder) 
    return ReportError("Failed to locate the movie recorder plugin!");

  // Find a reference to the bodymesh plugin
  bodyManager = csQueryRegistry<CS::Animation::iBodyManager> (GetObjectRegistry ());
  if (!bodyManager) 
    return ReportError("Failed to locate CS::Animation::iBodyManager plugin!");

  // Find references to the plugins of the animation nodes
  debugNodeManager =
    csQueryRegistry<CS::Animation::iSkeletonDebugNodeManager> (GetObjectRegistry ());
  if (!debugNodeManager)
    return ReportError("Failed to locate CS::Animation::iSkeletonDebugNodeManager plugin!");

  noiseNodeManager =
    csQueryRegistry<CS::Animation::iSkeletonNoiseNodeManager> (GetObjectRegistry ());
  if (!noiseNodeManager)
    return ReportError("Failed to locate CS::Animation::iSkeletonNoiseNodeManager plugin!");

  retargetNodeManager =
    csQueryRegistry<CS::Animation::iSkeletonRetargetNodeManager> (GetObjectRegistry ());
  if (!retargetNodeManager)
    return ReportError("Failed to locate CS::Animation::iSkeletonRetargetNodeManager plugin!");

  // Set the camera mode
  cameraManager->SetCameraMode (CS::Utility::CAMERA_MOVE_FREE);

  // Default behavior from DemoApplication for the creation of the scene
  if (!CreateRoom ())
    return false;

  // Create the scene
  if (!CreateScene ())
    return false;

  // Run the application
  if (!printInfo)
    Run();

  return true;
}

bool MocapViewer::CreateScene ()
{
  csString txt;
  int ivalue;
  float fvalue;

  // ------------------------------
  // Acquisition of the parameters of the scene
  // ------------------------------

  // Load the configuration file
  csRef<iConfigManager> cfg = csQueryRegistry<iConfigManager> (GetObjectRegistry());
  cfg->AddDomain ("/config/csmocapviewer.cfg", vfs, iConfigManager::ConfigPriorityPlugin);
  csString resourcePath = cfg->GetStr ("MocapViewer.Settings.ResourcePath", "");
  csString videoFormat = cfg->GetStr ("MocapViewer.Settings.VideoFormat", "");
  csString pldImage = cfg->GetStr ("MocapViewer.Settings.PLDImage", "");
  csString displayConfig = cfg->GetStr ("MocapViewer.Settings.Display", "");
  csString targetFile = cfg->GetStr ("MocapViewer.Settings.TargetFile", "");
  csString targetName = cfg->GetStr ("MocapViewer.Settings.TargetName", "");

  // Read the command line options
  // Read the file name
  csString mocapFilename = clp->GetName (0);
  if (mocapFilename == "")
  {
    ReportError ("No BVH file provided");
    PrintHelp ();
    return false;
  }

  // Check if we need to add the default mocap data path to the filename
  if (!vfs->Exists (mocapFilename.GetData ()) 
      && resourcePath != "")
  {
    // Check if there is a slash in the mocapFilename
    size_t index = mocapFilename.FindLast ('\\');
    if (index == (size_t) -1)
      index = mocapFilename.FindLast ('/');

    if (index == (size_t) -1)
      mocapFilename = resourcePath + mocapFilename;
  }

  // Read the start and end frames
  txt = clp->GetOption ("start", 0);
  int startFrame = 0;
  int frame;
  if (txt && sscanf (txt.GetData (), "%i", &frame) == 1)
    startFrame = frame;

  txt = clp->GetOption ("end", 0);
  int endFrame = 0;
  if (txt && sscanf (txt.GetData (), "%i", &frame) == 1)
    endFrame = frame;

  // Read the global scale
  txt = clp->GetOption ("scale", 0);
  float globalScale = 0.01f;
  if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    globalScale = fvalue;

  // ------------------------------
  // Parsing of the motion capture data file
  // ------------------------------

  // Parse the BVH file
  CS::Animation::BVHMocapParser mocapParser (GetObjectRegistry ());
  if (!mocapParser.SetResourceFile (mocapFilename.GetData ()))
    return false;

  if (startFrame > 0)
    mocapParser.SetStartFrame (startFrame);
  if (endFrame > 0)
    mocapParser.SetEndFrame (endFrame);
  mocapParser.SetGlobalScale (globalScale);

  parsingResult = mocapParser.ParseData ();
  if (!parsingResult.result)
    return false;

  // ------------------------------
  // Display of the summary of the motion capture data file
  // ------------------------------

  // Check if we simply need to print the mocap information then exit
  printInfo = clp->GetBoolOption ("info", false);
  if (printInfo)
  {
    printf ("=================================================\n");
    printf ("=== Mocap file: %s ===\n", mocapFilename.GetData ());
    printf ("=================================================\n");
    printf ("=== Frame count: %u ===\n", (unsigned int) parsingResult.frameCount);
    printf ("=== Frames per second: %.4f ===\n", 1.0f / parsingResult.frameDuration);
    printf ("=== Total duration: %.4f seconds ===\n",
	    parsingResult.frameCount * parsingResult.frameDuration);
    printf ("=================================================\n");
    printf ("=== Skeleton structure: ===\n");
    printf ("=================================================\n");
    printf ("%s", parsingResult.skeletonFactory->Description ().GetData ());
    printf ("=================================================\n");

    return true;
  }

  // ------------------------------
  // Removal of the horizontal motion of a given bone
  // ------------------------------

  csString nomove = clp->GetOption ("nomove", 0);
  if (nomove)
  {
    CS::Animation::BoneID boneID = parsingResult.skeletonFactory->FindBone (nomove);
    if (boneID == CS::Animation::InvalidBoneID)
      ReportWarning ("Could not find bone %s!", CS::Quote::Single (nomove.GetData ()));

    else
    {
      CS::Animation::iSkeletonAnimation* animation = parsingResult.animPacketFactory->GetAnimation (0);
      CS::Animation::ChannelID channel = animation->FindChannel (boneID);
      if (channel != CS::Animation::InvalidChannelID)
      {
	float time;
	csQuaternion rotation;
	csVector3 offset;
	size_t count = animation->GetKeyFrameCount (channel);
	for (size_t i = 0; i < count; i++)
	{
	  animation->GetKeyFrame (channel, i, boneID, time, rotation, offset);
	  offset[0] = offset[2] = 0.0f;
	  animation->SetKeyFrame (channel, i, rotation, offset);
	}
      }
    }
  }

  // ------------------------------
  // Initialialization of the random number generation seeds
  // ------------------------------

  txt = clp->GetOption ("randomseed", 0);
  unsigned int globalSeed = 93427923;
  if (txt && sscanf (txt.GetData (), "%i", &ivalue) == 1)  
    globalSeed = ivalue;

  csRandomGen globalRandomGenerator (globalSeed);
  int seedCount = 0;
  for (int i = 0; i < SEED_COUNT; i++)
    seeds[i] = globalRandomGenerator.Get (942438977);

  // ------------------------------
  // Switching of the bone animation channels
  // ------------------------------

  bool boneSwitchAll = clp->GetBoolOption ("boneswitchall", false);
  if (boneSwitchAll)
  {
    csRandomGen irandomGenerator (seeds[seedCount++]);

    const csArray<CS::Animation::BoneID>& bones = parsingResult.skeletonFactory->GetBoneOrderList ();
    csArray<CS::Animation::BoneID> boneSwitches;
    for (size_t i = 0; i < bones.GetSize (); i++)
      boneSwitches.Push (bones[i]);

    for (size_t i = 0; i < bones.GetSize () - 1; i++)
    {
      CS::Animation::BoneID boneID1 = bones[i];

      // Find a random other bone
      CS::Animation::BoneID boneID2 = CS::Animation::InvalidBoneID;
      while (true)
      {
	size_t index = irandomGenerator.Get (boneSwitches.GetSize () - 1);
	boneID2 = boneSwitches.Get (index);
	if (boneID1 != boneID2)
	{
	  boneSwitches.DeleteIndex (index);
	  break;
	}
      }

      // Find the animation channels of the corresponding bones
      CS::Animation::iSkeletonAnimation* animation =
	parsingResult.animPacketFactory->GetAnimation (0);

      CS::Animation::ChannelID channel1 = animation->FindChannel (boneID1);
      if (channel1 == CS::Animation::InvalidChannelID)
	continue;

      CS::Animation::ChannelID channel2 = animation->FindChannel (boneID2);
      if (channel2 == CS::Animation::InvalidChannelID)
	continue;

      // Switch the animation channels
      animation->SetChannelBone (channel1, boneID2);
      animation->SetChannelBone (channel2, boneID1);
    }
  }

  size_t switchIndex = 0;
  while (!boneSwitchAll)
  {
    // Read the next switch entry
    csString boneSwitch = clp->GetOption ("boneswitch", switchIndex++);
    if (!boneSwitch || boneSwitch.Trim ().IsEmpty ()) break;

    size_t index = boneSwitch.FindFirst (':');
    if (index == (size_t) -1)
    {
      ReportWarning ("Invalid bone animation switch %s. The entry must be of the format %s.",
		     CS::Quote::Single (boneSwitch), CS::Quote::Single ("bone1:bone2"));
      continue;
    }

    else
    {
      // Find the bone ID's
      csString bone1 = boneSwitch.Slice (0, index);
      csString bone2 = boneSwitch.Slice (index + 1);

      CS::Animation::BoneID boneID1 = parsingResult.skeletonFactory->FindBone (bone1);
      if (boneID1 == CS::Animation::InvalidBoneID)
      {
	ReportWarning ("Could not find the bone %s for the bone animation switch %s.",
		       CS::Quote::Single (bone1), CS::Quote::Single (boneSwitch));
	continue;
      }

      CS::Animation::BoneID boneID2 = parsingResult.skeletonFactory->FindBone (bone2);
      if (boneID2 == CS::Animation::InvalidBoneID)
      {
	ReportWarning ("Could not find the bone %s for the bone animation switch %s.",
		       CS::Quote::Single (bone2), CS::Quote::Single (boneSwitch));
	continue;
      }

      // Find the animation channels of the corresponding bones
      CS::Animation::iSkeletonAnimation* animation =
	parsingResult.animPacketFactory->GetAnimation (0);

      CS::Animation::ChannelID channel1 = animation->FindChannel (boneID1);
      if (channel1 == CS::Animation::InvalidChannelID)
      {
	ReportWarning ("The bone %s has no animation defined. The animation switch %s will therefore have no effect.",
		       CS::Quote::Single (bone1), CS::Quote::Single (boneSwitch));
	continue;
      }

      CS::Animation::ChannelID channel2 = animation->FindChannel (boneID2);
      if (channel2 == CS::Animation::InvalidChannelID)
      {
	ReportWarning ("The bone %s has no animation defined. The animation switch %s will therefore have no effect.",
		       CS::Quote::Single (bone2), CS::Quote::Single (boneSwitch));
	continue;
      }

      // Switch the animation channels
      animation->SetChannelBone (channel1, boneID2);
      animation->SetChannelBone (channel2, boneID1);
    }
  }

  // ------------------------------
  // Time-shifting the animation channels
  // ------------------------------

  size_t shiftIndex = 0;
  while (true)
  {
    // Read the next shift entry
    csString timeShift = clp->GetOption ("timeshift", shiftIndex++);
    if (!timeShift || timeShift.Trim ().IsEmpty ()) break;

    size_t index = timeShift.FindFirst (':');
    if (index == (size_t) -1)
    {
      ReportWarning ("Invalid animation time shift %s. The entry must be of the format %s.",
		     CS::Quote::Single (timeShift), CS::Quote::Single ("bone:offset"));
      continue;
    }

    else
    {
      // Find the bone ID and time offset
      csString bone = timeShift.Slice (0, index);
      csString offsetS = timeShift.Slice (index + 1);

      CS::Animation::BoneID boneID = parsingResult.skeletonFactory->FindBone (bone);
      if (boneID == CS::Animation::InvalidBoneID)
      {
	ReportWarning ("Could not find the bone %s for the animation animation time shift %s.",
		       CS::Quote::Single (bone), CS::Quote::Single (timeShift));
	continue;
      }

      float offset;
      if (sscanf (offsetS.GetData (), "%f", &offset) != 1)
      {
	ReportWarning ("The time offset in the animation animation time shift %s is not a valid float value.",
		       CS::Quote::Single (timeShift));
	continue;
      }

      // Find the animation channels of the corresponding bones
      CS::Animation::iSkeletonAnimation* animation =
	parsingResult.animPacketFactory->GetAnimation (0);

      CS::Animation::ChannelID channel = animation->FindChannel (boneID);
      if (channel == CS::Animation::InvalidChannelID)
      {
	ReportWarning ("The bone %s has no animation defined. The animation time shift %s will therefore have no effect.",
		       CS::Quote::Single (bone), CS::Quote::Single (timeShift));
	continue;
      }

      // Apply the time shift
      animation->ApplyTimeShift (channel, offset);
    }
  }

  // ------------------------------
  // Acquisition of the main remaining parameters of the scene
  // ------------------------------

  // Read if we are in PLD display mode
  DisplayMode displayMode = DISPLAY_WIRE;
  if (displayConfig == "pld")
    displayMode = DISPLAY_PLD;
  else if (displayConfig == "ellipse")
    displayMode = DISPLAY_ELLIPSE;

  csString displayOption = clp->GetOption ("display", 0);
  if (displayOption)
  {
    if (displayOption == "wire")
      displayMode = DISPLAY_WIRE;
    if (displayOption == "pld")
      displayMode = DISPLAY_PLD;
    else if (displayOption == "ellipse")
      displayMode = DISPLAY_ELLIPSE;
  }

  // Read the remaining configuration data
  bool recordVideo = clp->GetBoolOption ("record", false);
  csString recordFile = clp->GetOption ("recordfile", 0);
  bool noAnimation = clp->GetBoolOption ("noanim", false);

  float playbackSpeed = 1.0f;
  txt = clp->GetOption ("speed", 0);
  if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    playbackSpeed = fvalue;

  txt = clp->GetOption ("duration", 0);
  if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    duration = fvalue * 1000.0f;
  
  // ------------------------------
  // Creation of the animated mesh
  // ------------------------------

  // Load the animesh plugin
  csRef<iMeshObjectType> meshType = csLoadPluginCheck<iMeshObjectType>
    (GetObjectRegistry (), "crystalspace.mesh.object.animesh", false);

  if (!meshType)
    return ReportError ("Could not load the animesh object plugin!");

  // Create a new animesh factory
  csRef<iMeshObjectFactory> meshFactory = meshType->NewFactory ();
  csRef<CS::Mesh::iAnimatedMeshFactory> animeshFactory = 
    scfQueryInterfaceSafe<CS::Mesh::iAnimatedMeshFactory> (meshFactory);

  if (!animeshFactory)
    return ReportError ("Could not create an animesh factory!");

  animeshFactory->SetSkeletonFactory (parsingResult.skeletonFactory);

    // Check if the automatic animation has to be disabled
  if (noAnimation)
    parsingResult.skeletonFactory->SetAutoStart (false);

  // Load the iSkeletonManager plugin
  csRef<iPluginManager> plugmgr = csQueryRegistry<iPluginManager> (GetObjectRegistry ());
  csRef<CS::Animation::iSkeletonManager> skeletonManager =
    csLoadPlugin<CS::Animation::iSkeletonManager> (plugmgr, "crystalspace.skeletalanimation");
  if (!skeletonManager)
    return ReportError ("Could not load the skeleton plugin");

  const csArray<CS::Animation::BoneID>& boneList =
    parsingResult.skeletonFactory->GetBoneOrderList ();

  // Create a new animation tree. The structure of the tree is:
  //   + Debug node
  //     + Noise animation node if the skeleton noise is activated
  //       + Animation node with the mocap data,
  csRef<CS::Animation::iSkeletonAnimPacketFactory> animPacketFactory =
    parsingResult.skeletonFactory->GetAnimationPacket ();

  // ------------------------------
  // Setup the bone chain mask to select the bones that are displayed
  // ------------------------------

  bool hasMask = clp->GetOption ("rootmask", 0)
    || clp->GetOption ("bone", 0) || clp->GetOption ("nobone", 0);
  csBitArray boneMask;
  boneMask.SetSize (parsingResult.skeletonFactory->GetTopBoneID () + 1);

  // Check if there is only one empty "-nobone" option provided
  txt = clp->GetOption ("nobone", 0);
  if (txt && txt == ""
      && !clp->GetOption ("rootmask", 0) && !clp->GetOption ("bone", 0))
    boneMask.Clear ();

  else
    boneMask.SetAll ();

  // Check for a definition of a bone chain
  txt = clp->GetOption ("rootmask", 0);
  if (txt)
  {
    boneMask.Clear ();

    CS::Animation::BoneID boneID = parsingResult.skeletonFactory->FindBone (txt);
    if (boneID == CS::Animation::InvalidBoneID)
      ReportWarning ("Could not find root bone %s!", CS::Quote::Single (txt));

    else
    {
      // Create the body chain
      CS::Animation::iBodySkeleton* bodySkeleton =
	bodyManager->CreateBodySkeleton ("mocap_body", parsingResult.skeletonFactory);
      CS::Animation::iBodyChain* bodyChain = bodySkeleton->CreateBodyChain ("body_chain", boneID);

      // Check if we need to add all children
      if (clp->GetBoolOption ("childall", false))
	bodyChain->AddAllSubChains ();

      // Add all user defined sub-chains
      size_t index = 0;
      txt = clp->GetOption ("childmask", index);
      while (txt)
      {
	boneID = parsingResult.skeletonFactory->FindBone (txt);
	if (boneID == CS::Animation::InvalidBoneID)
	  ReportWarning ("Could not find child bone %s!", CS::Quote::Single (txt));

	else
	  bodyChain->AddSubChain (boneID);

	index++;
	txt = clp->GetOption ("childmask", index);
      }

      bodyChain->PopulateBoneMask (boneMask);
    }
  }

  // Setup the mask for the bones that are explicitely given on command line
  size_t index = 0;
  txt = clp->GetOption ("bone", index);
  while (txt)
  {
    CS::Animation::BoneID boneID = parsingResult.skeletonFactory->FindBone (txt);
    if (boneID == CS::Animation::InvalidBoneID)
      ReportWarning ("Could not find user specified bone %s!", CS::Quote::Single (txt));

    else boneMask.SetBit (boneID);

    txt = clp->GetOption ("bone", ++index);
  }

  index = 0;
  txt = clp->GetOption ("nobone", index);
  while (txt)
  {
    if (txt != "")
    {
      CS::Animation::BoneID boneID = parsingResult.skeletonFactory->FindBone (txt);
      if (boneID == CS::Animation::InvalidBoneID)
	ReportWarning ("Could not find user specified bone %s!", CS::Quote::Single (txt));

      else boneMask.ClearBit (boneID);
    }

    txt = clp->GetOption ("nobone", ++index);
  }

  // ------------------------------
  // Setup of the 'debug' animation node
  // ------------------------------

  // Create the 'debug' animation node
  csRef<CS::Animation::iSkeletonDebugNodeFactory> debugNodeFactory =
    debugNodeManager->CreateAnimNodeFactory ("debug");

  CS::Animation::SkeletonDebugMode debugModes;
  switch (displayMode)
  {
  case DISPLAY_WIRE:
    debugModes = (CS::Animation::SkeletonDebugMode)
      (CS::Animation::DEBUG_2DLINES | CS::Animation::DEBUG_SQUARES);
    break;
  case DISPLAY_PLD:
    debugModes = CS::Animation::DEBUG_SQUARES;
    break;
  case DISPLAY_ELLIPSE:
    // Populate the animesh with default bounding boxes
    CS::Mesh::AnimatedMeshTools::PopulateSkeletonBoundingBoxes
     (animeshFactory, hasMask ? &boneMask : nullptr);

    debugModes = CS::Animation::DEBUG_ELLIPSOIDS;
    break;
  }

  debugNodeFactory->SetDebugModes (debugModes);
  debugNodeFactory->SetLeafBonesDisplayed (false);
  animPacketFactory->SetAnimationRoot (debugNodeFactory);

  // Load the debug image
  if (displayMode == DISPLAY_PLD && pldImage != "")
  {
    csRef<iTextureWrapper> texture = loader->LoadTexture
      ("pld_image", pldImage.GetData (), CS_TEXTURE_2D, 0, true, true, true);
    if (!texture.IsValid ())
      ReportWarning ("Failed to load PLD image %s!\n", CS::Quote::Single (pldImage));

    else
    {
      // Create the 2D sprite
      iTextureHandle* textureHandle = texture->GetTextureHandle ();
      if (textureHandle)
      {
	debugImage = new csSimplePixmap (textureHandle);
	debugNodeFactory->SetDebugImage (debugImage);
	debugNodeFactory->SetDebugModes (CS::Animation::DEBUG_IMAGES);
      }
    }
  }

  // Setup the bone mask
  if (hasMask)
    debugNodeFactory->SetBoneMask (boneMask);

  // Scramble of the position of the bones
  txt = clp->GetOption ("scramble", 0);
  if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1
      && fvalue > EPSILON)
  {
    csRandomFloatGen frandomGenerator (seeds[seedCount++]);
    fvalue *= 0.5f;

    for (size_t i = 0; i < boneList.GetSize (); i++)
    {
      csVector3 offset (frandomGenerator.Get (-fvalue, fvalue),
			frandomGenerator.Get (-fvalue, fvalue),
			frandomGenerator.Get (-fvalue, fvalue));
      debugNodeFactory->SetBoneOffset (boneList[i], offset);
    }
  }

  // ------------------------------
  // Setup of the 'mocap' animation node
  // ------------------------------

  csRef<CS::Animation::iSkeletonAnimationNodeFactory> mocapNodeFactory =
    animPacketFactory->CreateAnimationNode ("mocap");
  mocapNodeFactory->SetAnimation (parsingResult.animPacketFactory->GetAnimation (0));
  mocapNodeFactory->SetCyclic (!recordVideo);
  mocapNodeFactory->SetPlaybackSpeed (playbackSpeed);

  // ------------------------------
  // Setup of the 'noise' animation node
  // ------------------------------

  if (clp->GetBoolOption ("snoise", false))
  {
    // Initialize the Perlin noise modules
    csRandomGen irandomGenerator1 (seeds[seedCount++]);
    csRandomGen irandomGenerator2 (seeds[seedCount++]);
    csRandomGen irandomGenerator3 (seeds[seedCount++]);
    csRandomFloatGen frandomGenerator (seeds[seedCount++]);
    snoiseX.SetSeed (irandomGenerator1.Get (~0));
    snoiseY.SetSeed (irandomGenerator2.Get (~0));
    snoiseZ.SetSeed (irandomGenerator3.Get (~0));

    // Read the command line parameters of the noise modules
    float snratio = 1.0f;
    txt = clp->GetOption ("snratio", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
      snratio = fvalue;

    txt = clp->GetOption ("snoctaves", 0);
    if (txt && sscanf (txt.GetData (), "%i", &ivalue) == 1)
    {
      ivalue = csMin (ivalue, 30);
      ivalue = csMax (ivalue, 1);
      snoiseX.SetOctaveCount (ivalue);
      snoiseY.SetOctaveCount (ivalue);
      snoiseZ.SetOctaveCount (ivalue);
    }

    txt = clp->GetOption ("snfrequency", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    {
      snoiseX.SetFrequency (fvalue);
      snoiseY.SetFrequency (fvalue);
      snoiseZ.SetFrequency (fvalue);
    }

    txt = clp->GetOption ("snlacunarity", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    {
      snoiseX.SetLacunarity (fvalue);
      snoiseY.SetLacunarity (fvalue);
      snoiseZ.SetLacunarity (fvalue);
    }

    txt = clp->GetOption ("snpersistence", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    {
      snoiseX.SetPersistence (fvalue);
      snoiseY.SetPersistence (fvalue);
      snoiseZ.SetPersistence (fvalue);
    }

    // Create the 'noise' animation node
    csRef<CS::Animation::SkeletonNoise> skeletonNoise;
    skeletonNoise.AttachNew (new CS::Animation::SkeletonNoise ());
    skeletonNoise->SetComponent (0, &snoiseX);
    skeletonNoise->SetComponent (1, &snoiseY);
    skeletonNoise->SetComponent (2, &snoiseZ);

    csRef<CS::Animation::iSkeletonNoiseNodeFactory> noiseNodeFactory =
      noiseNodeManager->CreateAnimNodeFactory ("noise");
    noiseNodeFactory->AddSkeletonNoise (skeletonNoise);

    // Read the ratio to be applied on the translation noise
    float translationRatio = 1.0f;
    txt = clp->GetOption ("sntranslation", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
      translationRatio = fvalue;

    // Setup all bone noises
    for (csArray<CS::Animation::BoneID>::ConstIterator it =
	   boneList.GetIterator (); it.HasNext (); )
    {
      CS::Animation::BoneID boneID = it.Next ();
      bool isRoot = parsingResult.skeletonFactory->GetBoneParent (boneID)
	== CS::Animation::InvalidBoneID;

      // Add a rotation noise to all bones
      noiseNodeFactory->AddBoneNoise (boneID, 0, CS::Animation::NOISE_ROTATION,
				      frandomGenerator.Get (10.0f),
				      frandomGenerator.Get (10.0f));


      // The root bones get a position noise too
      if (isRoot || translationRatio > EPSILON)
      {
	float scale = isRoot ? 0.2f : 0.45f;
	if (translationRatio > EPSILON
	    && fabs (translationRatio - 1.0f) > EPSILON)
	  scale *= translationRatio;

	noiseNodeFactory->AddBoneNoise (boneID, 0, CS::Animation::NOISE_POSITION,
					frandomGenerator.Get (10.0f),
					frandomGenerator.Get (10.0f),
					csVector3 (scale));
      }
    }

    csRef<CS::Animation::iSkeletonBlendNodeFactory> blendNodeFactory =
      animPacketFactory->CreateBlendNode ("blend");
    debugNodeFactory->SetChildNode (blendNodeFactory);
    blendNodeFactory->AddNode (mocapNodeFactory, 1.0f);
    blendNodeFactory->AddNode (noiseNodeFactory, snratio);
  }

  else
    debugNodeFactory->SetChildNode (mocapNodeFactory);

  // ------------------------------
  // Creation of the animated mesh
  // ------------------------------

  // Create the animated mesh
  csRef<iMeshFactoryWrapper> meshFactoryWrapper =
    engine->CreateMeshFactory (meshFactory, "mocap_meshfact");
  meshWrapper = engine->CreateMeshWrapper (meshFactoryWrapper, "mocap_mesh", room);
  animesh = scfQueryInterface<CS::Mesh::iAnimatedMesh> (meshWrapper->GetMeshObject ());

  // When the animated mesh is created, the animation nodes are created too.
  // We can therefore set them up now.
  CS::Animation::iSkeletonAnimNode* rootNode =
    animesh->GetSkeleton ()->GetAnimationPacket ()->GetAnimationRoot ();

  // Find a reference to the animation nodes
  debugNode = scfQueryInterface<CS::Animation::iSkeletonDebugNode> (rootNode->FindNode ("debug"));
  animNode = rootNode->FindNode ("mocap");

  // Uncleanly define the initial default color of the debug node
  csColor color (0.0f, 8.0f, 0.0f);
  debugNode->Draw (view->GetCamera (), color);

  // ------------------------------
  // Setup of the retarget of the animation to an animesh
  // ------------------------------

  txt = clp->GetOption ("targetfile", 0);
  if (txt) targetFile = txt;

  txt = clp->GetOption ("targetname", 0);
  if (txt) targetName = txt;

  CS::Animation::iSkeletonFactory* retargetSkeletonFactory = nullptr;
  if (targetFile != "" && targetName != "")
  {
    // Load the animesh factory
    csLoadResult rc = loader->Load (targetFile.GetData ());
    if (!rc.success)
      return ReportError ("Can't load target library file %s!", CS::Quote::Single (targetFile));

    csRef<iMeshFactoryWrapper> meshfact = engine->FindMeshFactory (targetName.GetData ());
    if (!meshfact)
      return ReportError ("Can't find target mesh factory %s!", CS::Quote::Single (targetName));

    animeshFactory = scfQueryInterface<CS::Mesh::iAnimatedMeshFactory>
      (meshfact->GetMeshObjectFactory ());
    if (!animeshFactory)
      return ReportError ("Can't find the animesh interface for the animesh target!");
    retargetSkeletonFactory = animeshFactory->GetSkeletonFactory ();

    // Check if the automatic animation has to be disabled
    if (noAnimation)
      animeshFactory->GetSkeletonFactory ()->SetAutoStart (false);

    // Create a new animation tree. The structure of the tree is:
    //   + Retarget node
    //     + Animation node with the mocap data
    csRef<CS::Animation::iSkeletonAnimPacketFactory> animPacketFactory =
      animeshFactory->GetSkeletonFactory ()->GetAnimationPacket ();

    // Create the 'retarget' animation node
    csRef<CS::Animation::iSkeletonRetargetNodeFactory> retargetNodeFactory =
      retargetNodeManager->CreateAnimNodeFactory ("mocap_retarget");
    animPacketFactory->SetAnimationRoot (retargetNodeFactory);
    retargetNodeFactory->SetSourceSkeleton (parsingResult.skeletonFactory);

    // This mapping is for the motion capture data with the same name of the bones
    CS::Animation::BoneMapping skeletonMapping;
    CS::Animation::NameBoneMappingHelper::GenerateMapping
      (skeletonMapping, parsingResult.skeletonFactory, animeshFactory->GetSkeletonFactory ());
    retargetNodeFactory->SetBoneMapping (skeletonMapping);

    // If the animesh target is Krystal, then we load some hardcoded information to setup the retarget mode
    if (targetName == "krystal")
    {
      // Create the bone mapping between the source and the target skeletons
      /*
      // This mapping is for the motion capture data of the Carnegie Mellon University
      CS::Animation::BoneMapping skeletonMapping;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("hip"), animeshFactory->GetSkeletonFactory ()->FindBone ("Hips"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("abdomen"), animeshFactory->GetSkeletonFactory ()->FindBone ("ToSpine"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("chest"), animeshFactory->GetSkeletonFactory ()->FindBone ("Spine"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("neck"), animeshFactory->GetSkeletonFactory ()->FindBone ("Neck"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("head"), animeshFactory->GetSkeletonFactory ()->FindBone ("Head"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rCollar"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightShoulder"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rShldr"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightArm"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rForeArm"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightForeArm"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rHand"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightHand"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lCollar"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftShoulder"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lShldr"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftArm"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lForeArm"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftForeArm"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lHand"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftHand"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rThigh"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightUpLeg"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rShin"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightLeg"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("rFoot"), animeshFactory->GetSkeletonFactory ()->FindBone ("RightFoot"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lThigh"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftUpLeg"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lShin"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftLeg"));;
      skeletonMapping.AddMapping (parsingResult.skeletonFactory->FindBone ("lFoot"), animeshFactory->GetSkeletonFactory ()->FindBone ("LeftFoot"));;
      retargetNodeFactory->SetBoneMapping (skeletonMapping);
      */
      // Create the body chains used for retargeting
      CS::Animation::iBodySkeleton* bodySkeleton =
	bodyManager->CreateBodySkeleton ("target_body", animeshFactory->GetSkeletonFactory ());

      CS::Animation::iBodyChain* bodyChain = bodySkeleton->CreateBodyChain
	// (the "Hips" bone is positioned too differently from the mocap data, we therefore skip this bone)
	//("torso", animeshFactory->GetSkeletonFactory ()->FindBone ("Hips"));
	("torso", animeshFactory->GetSkeletonFactory ()->FindBone ("ToSpine"));
      bodyChain->AddSubChain (animeshFactory->GetSkeletonFactory ()->FindBone ("Head"));
      retargetNodeFactory->AddBodyChain (bodyChain);

      bodyChain = bodySkeleton->CreateBodyChain
	("left_arm", animeshFactory->GetSkeletonFactory ()->FindBone ("LeftShoulder"));
      bodyChain->AddSubChain (animeshFactory->GetSkeletonFactory ()->FindBone ("LeftHand"));
      retargetNodeFactory->AddBodyChain (bodyChain);

      bodyChain = bodySkeleton->CreateBodyChain
	("right_arm", animeshFactory->GetSkeletonFactory ()->FindBone ("RightShoulder"));
      bodyChain->AddSubChain (animeshFactory->GetSkeletonFactory ()->FindBone ("RightHand"));
      retargetNodeFactory->AddBodyChain (bodyChain);

      bodyChain = bodySkeleton->CreateBodyChain
	("left_leg", animeshFactory->GetSkeletonFactory ()->FindBone ("LeftUpLeg"));
      bodyChain->AddSubChain (animeshFactory->GetSkeletonFactory ()->FindBone ("LeftToeBase"));
      retargetNodeFactory->AddBodyChain (bodyChain);

      bodyChain = bodySkeleton->CreateBodyChain
	("right_leg", animeshFactory->GetSkeletonFactory ()->FindBone ("RightUpLeg"));
      bodyChain->AddSubChain (animeshFactory->GetSkeletonFactory ()->FindBone ("RightToeBase"));
      retargetNodeFactory->AddBodyChain (bodyChain);
    }

    // Create the playback animation node of the motion captured data
    csRef<CS::Animation::iSkeletonAnimationNodeFactory> mocapNodeFactory =
      animPacketFactory->CreateAnimationNode ("mocap_retarget");
    mocapNodeFactory->SetAnimation
      (parsingResult.animPacketFactory->GetAnimation (0));
    mocapNodeFactory->SetCyclic (true);
    mocapNodeFactory->SetPlaybackSpeed (playbackSpeed);
    retargetNodeFactory->SetChildNode (mocapNodeFactory);

    // Create the 'retarget' animated mesh
    csRef<iMeshWrapper> retargetMesh =
      engine->CreateMeshWrapper (meshfact, "retarget", room, csVector3 (0.0f));
  }

  // ------------------------------
  // Setup of the noise cloud points
  // ------------------------------

  txt = clp->GetOption ("ncount", 0);
  int noiseCount;
  if (txt && sscanf (txt.GetData (), "%i", &noiseCount) == 1)
  {
    // Initialize the Perlin noise modules
    csRandomGen irandomGenerator (seeds[seedCount++]);
    csRandomGen irandomGenerator2 (seeds[seedCount++]);
    csRandomFloatGen frandomGenerator (seeds[seedCount++]);
    noiseX.SetSeed (irandomGenerator.Get (~0));
    noiseY.SetSeed (irandomGenerator2.Get (~0));

    // Read the command line parameters of the noise modules
    txt = clp->GetOption ("noctaves", 0);
    if (txt && sscanf (txt.GetData (), "%i", &ivalue) == 1)
    {
      ivalue = csMin (ivalue, 30);
      ivalue = csMax (ivalue, 1);
      noiseX.SetOctaveCount (ivalue);
      noiseY.SetOctaveCount (ivalue);
    }

    txt = clp->GetOption ("nscale", 0);
    float fvalue;
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
      noiseScale = fvalue;

    txt = clp->GetOption ("nfrequency", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    {
      noiseX.SetFrequency (fvalue);
      noiseY.SetFrequency (fvalue);
    }

    txt = clp->GetOption ("nlacunarity", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    {
      noiseX.SetLacunarity (fvalue);
      noiseY.SetLacunarity (fvalue);
    }

    txt = clp->GetOption ("npersistence", 0);
    if (txt && sscanf (txt.GetData (), "%f", &fvalue) == 1)
    {
      noiseX.SetPersistence (fvalue);
      noiseY.SetPersistence (fvalue);
    }

    // Create the noise points
    for (int i = 0; i < noiseCount; i++)
    {
      csVector3 point;
      point[0] = frandomGenerator.Get (10.0f);
      point[1] = frandomGenerator.Get (10.0f);
      point[2] = frandomGenerator.Get (10.0f);
      noisePoints.Push (point);
    }
  }

  // ------------------------------
  // Setup of the camera
  // ------------------------------

  // Compute the bounding box of the skeleton and the position of the target
  // of the camera
  csBox3 skeletonBBox;
  csVector3 cameraTarget (0.0f);

  // Compute the bounding box of the bones of the skeleton that are visible
  if (boneList.GetSize ())
  {
    csQuaternion rotation;
    csVector3 offset;
    CS::Animation::ChannelID rootChannel = CS::Animation::InvalidChannelID;
    CS::Animation::iSkeletonAnimation* animation = parsingResult.animPacketFactory->GetAnimation (0);
    for (size_t i = 0; i < boneList.GetSize (); i++)
      if (!hasMask || boneMask.IsBitSet (i))
      {
	if (rootChannel == CS::Animation::InvalidChannelID
	    && animation->FindChannel (i))
	  rootChannel = animation->FindChannel (i);

	parsingResult.skeletonFactory->GetTransformAbsSpace (i, rotation, offset);
	skeletonBBox.AddBoundingVertex (offset);
      }
    cameraTarget = skeletonBBox.GetCenter ();

    // Find the initial position of the root of the skeleton
    if (!noAnimation
	&& rootChannel != CS::Animation::InvalidChannelID)
    {
      float time;
      CS::Animation::BoneID rootBone;
      animation->GetKeyFrame (rootChannel, 0, rootBone, time, rotation, offset);
      cameraTarget += offset;
    }
  }

  // If there are no bones then use the position of the root of the target mesh
  if (hasMask && boneMask.AllBitsFalse ()
      && retargetSkeletonFactory
      && retargetSkeletonFactory->GetBoneOrderList ().GetSize ())
  {
    csQuaternion rotation;
    csVector3 offset;
    retargetSkeletonFactory->GetTransformAbsSpace
      (retargetSkeletonFactory->GetBoneOrderList ().Get (0), rotation, offset);
    cameraTarget += offset;
  }

  // Compute the position of the camera
  txt = clp->GetOption ("poscamera", 0);
  float scale = 1.0f;
  float value;
  if (txt && sscanf (txt.GetData (), "%f", &value) == 1)
    scale = value;
  csVector3 cameraOffset = csVector3 (0.0f, 0.0f, -300.0f * scale) * globalScale;

  // Check if the user has provided an angle for the camera
  txt = clp->GetOption ("rotcamera", 0);
  float angle;
  if (txt && sscanf (txt.GetData (), "%f", &angle) == 1)
    // TODO: the csYRotMatrix3 is defined in right-handed coordinate system!
    cameraOffset = csYRotMatrix3 (-angle * 3.1415927f / 180.0f) * cameraOffset;

  // Check if the user has provided an offset for the camera target
  csVector3 targetOffset (0.0f);
  txt = clp->GetOption ("offxcamera", 0);
  if (txt && sscanf (txt.GetData (), "%f", &value) == 1)
    targetOffset[0] = value;

  txt = clp->GetOption ("offycamera", 0);
  if (txt && sscanf (txt.GetData (), "%f", &value) == 1)
    targetOffset[1] = value;

  // Update the position of the camera
  view->GetCamera ()->GetTransform ().SetOrigin (cameraTarget + cameraOffset);
  view->GetCamera ()->GetTransform ().LookAt
    (-cameraOffset + targetOffset, csVector3 (0.0f, 1.0f, 0.0f));

  // Check if the camera has to be in 'LookAt' mode
  txt = clp->GetOption ("lookat", 0);
  if (txt)
  {
    targetBoneID = parsingResult.skeletonFactory->FindBone (txt);
    if (targetBoneID == CS::Animation::InvalidBoneID)
      ReportWarning ("Could not find lookat bone %s!",
		     CS::Quote::Single (txt.GetData ()));

    else
      cameraManager->SetCameraMode (CS::Utility::CAMERA_MOVE_LOOKAT);
  }

  // Check if the user has provided an angle for the skeleton
  txt = clp->GetOption ("rotskeleton", 0);
  if (txt && sscanf (txt.GetData (), "%f", &angle) == 1)
  {
    csOrthoTransform meshCenterTransform (csMatrix3 (), skeletonBBox.GetCenter ());
    csOrthoTransform rotationTransform (csZRotMatrix3 (angle * 3.1415927f / 180.0f),
					csVector3 (0.0f));
    meshWrapper->QuerySceneNode ()->GetMovable ()->GetTransform () *=
      meshCenterTransform.GetInverse () * rotationTransform * meshCenterTransform;
    meshWrapper->QuerySceneNode ()->GetMovable ()->UpdateMove ();
  }

  // Display the origin
  if (displayMode != DISPLAY_PLD)
    CS::Debug::VisualDebuggerHelper::DebugTransform
      (GetObjectRegistry (), csTransform (), true);

  // ------------------------------
  // Setup of the video recording
  // ------------------------------

  if (recordVideo)
  {
    // Check if a video file has been provided
    if (recordFile != "")
    {
      // Check if there is a slash in the filename
      size_t index = recordFile.FindLast ('\\');
      if (index == (size_t) -1)
	index = recordFile.FindLast ('/');

      if (index == (size_t) -1)
	recordFile = "/this/" + recordFile;

      movieRecorder->SetRecordingFile (recordFile.GetData ());
    }

    // Else check if a default video file format has been provided
    else if (videoFormat != "")
      movieRecorder->SetFilenameFormat (videoFormat.GetData ());

    // Disable the display of the HUD
    hudManager->SetEnabled (false);

    // Disable the motion of the camera
    cameraManager->SetCameraMode (CS::Utility::CAMERA_NO_MOVE);

    // Register a callback for the changes of the state of the playback of the animation
    if (duration < 1)
      animNode->AddAnimationCallback (this);

    // Start the movie recording
    movieRecorder->Start ();
  }

  // ------------------------------
  // Initialization of the HUD
  // ------------------------------

  if (hudManager->GetEnabled ())
  {
    hudManager->GetStateDescriptions ()->Push ("Frame:");
    csString hudTxt;
    hudTxt.Format ("Mocap FPS: %.2f", 1.0f / parsingResult.frameDuration);
    hudManager->GetStateDescriptions ()->Push (hudTxt);
    hudTxt.Format ("Total length: %.2f seconds", animNode->GetDuration ());
    hudManager->GetStateDescriptions ()->Push (hudTxt);
  }

  return true;
}

void MocapViewer::AnimationFinished (CS::Animation::iSkeletonAnimNode* node)
{
  // This is the end of the animation, exit the application in order to end the recording
  // of the video
  csRef<iEventQueue> q (csQueryRegistry<iEventQueue> (GetObjectRegistry ()));
  if (q) q->GetEventOutlet()->Broadcast (csevQuit (GetObjectRegistry ()));
}

//---------------------------------------------------------------------------

CS_IMPLEMENT_APPLICATION

int main (int argc, char* argv[])
{
  return MocapViewer ().Main (argc, argv);
}
