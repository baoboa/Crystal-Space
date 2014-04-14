import bpy

import os

from .util import *
from .transform import *
from .image import *
from .renderbuffer import *
from .submesh import *
from .data import *
from .object import *
from .group import *
from .scene import *
from .material import ExportMaterials
from io_scene_cs.utilities import B2CS


def Write(fi):
  def write(data):
    fi.write(data+'\n')
  return write


def Export(path):

  # Set interaction mode to 'OBJECT' mode
  editMode = False
  for ob in bpy.data.objects:
    if ob.mode == 'EDIT':
      if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')
        editMode = True
        break

  # Export Blender data to Crystal Space format
  exportAsLibrary = B2CS.properties.library
  if exportAsLibrary:
    print("\nEXPORTING: "+Join(path, 'library')+" ====================================")
    print("  All objects of the world are exported as factories in 'library' file.\n")
    ExportLibrary(path)

  else:
    print("\nEXPORTING: "+Join(path, 'world')+" ====================================")
    print("  All scenes composing this world are exported as sectors in the 'world' file ;\n",
           " all objects of these scenes are exported as separated libraries.\n")
    ExportWorld(path)

  # Restore interaction mode
  if editMode:
    if bpy.ops.object.mode_set.poll():
      bpy.ops.object.mode_set(mode='EDIT')


def ExportWorld(path):
  """ Export scenes composing the world as sectors in the 'world' file,
      saved in the directory specified by 'path'. All objects of these scenes
      are exported as separated libraries in the 'factories' subfolder.
  """

  # Create the export directory for textures
  if not os.path.exists(Join(path, 'textures/')):
    os.makedirs(Join(path, 'textures/'))

  # Create the export directory for factories
  if not os.path.exists(Join(path, 'factories/')):
    os.makedirs(Join(path, 'factories/'))

  # Get data about all objects composing this world
  deps = util.EmptyDependencies()
  cameras = {}
  for scene in bpy.data.scenes:
    MergeDependencies(deps, scene.GetDependencies())
    cameras.update(scene.GetCameras())

  # Create a 'world' file containing the xml description of the scenes
  f = open(Join(path, 'world'), 'w')
  Write(f)('<?xml version="1.0" encoding="UTF-8"?>')
  Write(f)('<world xmlns=\"http://crystalspace3d.org/xml/library\">')

  # Export the objects composing the world
  for typ in deps:
    if typ == 'A':
      # Animated meshes
      for name, fact in deps[typ].items():
        print('\nEXPORT OBJECT "%s" AS A CS ANIMATED MESH\n'%(fact.object.name))
        print('Writing fact',fact.uname,':', Join(path, 'factories/', fact.object.name))
        fact.AsCSRef(Write(f), 2, 'factories/')
        # Export animesh factory
        fact.AsCSLib(path, animesh=True)
    elif typ == 'F':
      # General meshes
      for name, fact in deps[typ].items():
        print('\nEXPORT OBJECT "%s" AS A CS GENERAL MESH\n'%(fact.object.name))
        print('Writing fact',fact.uname,':', Join(path, 'factories/', fact.object.name))
        fact.AsCSRef(Write(f), 2, 'factories/')
        # Export genmesh factory
        fact.AsCSLib(path, animesh=False)
    elif typ == 'G':
      # Groups of objects
      for name, group in deps[typ].items():
        print('\nEXPORT GROUP "%s" AS A CS GENERAL MESH\n'%(group.uname))
        print('Writing group', Join(path, 'factories/', group.uname))
        group.AsCSRef(Write(f), 2, 'factories/')
        # Export group of genmeshes
        group.AsCSLib(path)

  # Export cameras
  if cameras:
    ExportCameras(Write(f), 2, cameras)
  else:
    # Set a default camera if none is defined
    bpy.context.scene.CameraAsCS(Write(f), 2)

  # Export scenes as CS sectors in the 'world' file
  for scene in bpy.data.scenes:
    scene.AsCS(Write(f), 2)

  Write(f)('</world>')
  f.close()

  Hierarchy.exportedFactories = []

  print("\nEXPORTING complete ==================================================")


def ExportLibrary(path):
  """ Export all objects composing the world as factories in a single 'library' file,
      placed in the directory specified by 'path'.
  """

  # Create the export directory for textures
  if not os.path.exists(Join(path, 'textures/')):
    os.makedirs(Join(path, 'textures/'))

  # Get data about all objects composing the world
  deps = util.EmptyDependencies()
  for scene in bpy.data.scenes:
    MergeDependencies(deps, scene.GetDependencies())

  # Create a 'library' file containing the xml description of the objects
  f = open(Join(path, 'library'), 'w')
  Write(f)('<?xml version="1.0" encoding="UTF-8"?>')
  Write(f)('<library xmlns=\"http://crystalspace3d.org/xml/library\">')

  # Export the textures/materials/shaders of the objects
  use_imposter = False
  for scene in bpy.data.scenes:
    for ob in scene.objects:
      if ob.HasImposter():
        use_imposter = True
        break
  ExportMaterials(Write(f), 2, path, deps, use_imposter)

  # Export the objects composing the Blender world
  for typ in deps:
    if typ == 'A':
      # Animated meshes
      for name, fact in deps[typ].items():
        ob = fact.object
        print('\nEXPORT OBJECT "%s" AS A CS ANIMATED MESH\n'%(ob.name))
        print('Writing fact',fact.uname,'in', Join(path, 'library'))
        # Export animesh factory
        fact.WriteCSAnimeshHeader(Write(f), 2, skelRef=False)
        # Export skeleton and animations
        if ob.type == 'ARMATURE' and ob.data.bones:
          print('Exporting skeleton and animations: %s_rig'%(ob.name))
          ob.data.AsCSSkelAnim(Write(f), 4, ob, dontClose=True)
        # Export mesh buffers
        fact.WriteCSMeshBuffers(Write(f), 2, path, animesh=True, dontClose=True)
        Write(f)('  </meshfact>')
    elif typ == 'F':
      # General meshes
      for name, fact in deps[typ].items():
        ob = fact.object
        print('\nEXPORT OBJECT "%s" AS A CS GENERAL MESH\n'%(ob.name))
        print('Writing fact',fact.uname,'in', Join(path, 'library'))
        # Export genmesh factory
        fact.WriteCSMeshBuffers(Write(f), 2, path, animesh=False, dontClose=True)
    elif typ == 'G':
      # Groups of objects
      for name, group in deps[typ].items():
        print('\nEXPORT GROUP "%s" AS A CS GENERAL MESH\n'%(group.uname))
        print('Writing group', group.uname,'in', Join(path, 'library'))
        # Export group of meshes
        use_imposter = group.HasImposter()
        group.WriteCSGroup(Write(f), 2, use_imposter, dontClose=True)

  Write(f)('</library>')
  f.close()

  print("\nEXPORTING complete ==================================================")
