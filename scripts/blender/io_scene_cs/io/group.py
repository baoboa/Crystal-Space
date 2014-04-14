import bpy
import mathutils

from .util import *
from .transform import *
from .submesh import *
from .material import *
from io_scene_cs.utilities import B2CS


def GroupallObjects(self, matrix=None):
  objects = []
  for ob in self.objects:
    if ob.type == 'EMPTY':
      if ob.dupli_type=='GROUP' and ob.dupli_group:
        objects.extend(ob.dupli_group.allObjects(ob.relative_matrix))
    else:
      objects.append((matrix, ob))
  return objects
  
bpy.types.Group.allObjects = GroupallObjects


def GroupAsCSRef(self, func, depth=0, dirName='factories/'):
  func(' '*depth +'<library>%s%s</library>'%(dirName,self.uname))

bpy.types.Group.AsCSRef = GroupAsCSRef


def GroupAsCS(self, func, depth=0, **kwargs):
  func(' '*depth +'<meshobj name="%s_group">'%(self.uname))
  func(' '*depth +'  <plugin>crystalspace.mesh.loader.genmesh</plugin>')
  func(' '*depth +'  <params>')
  func(' '*depth +'    <factory>%s</factory>'%(self.uname))
  func(' '*depth +'  </params>')
  #func(' '*depth +'  <move>')
  # Flip Y and Z axis.
  #func(' '*depth +'    <v x="%f" z="%f" y="%f" />'% tuple(self.relative_location))
  #func(' '*depth +'  </move>')
  
  if 'transform' in kwargs:
    MatrixAsCS(kwargs['transform'], func, depth+2)
  else:
    func(' '*depth +'  <move><v x="0" z="0" y="0" /></move>')
  
  func(' '*depth +'</meshobj>')
  
bpy.types.Group.AsCS = GroupAsCS


def WriteCSLibHeader(self, func):
  func('<?xml version="1.0" encoding="UTF-8"?>')
  func("<library xmlns=\"http://crystalspace3d.org/xml/library\">")

bpy.types.Group.WriteCSLibHeader = WriteCSLibHeader


def GroupAsCSLib(self, path=''):
  """ Export a group of Blender objects as a CS library entitled
      '<group name>' in the '<path>/factories' folder;
      objects are exported as instances or submeshes of a general mesh
  """

  def Write(fi):
    def write(data):
      fi.write(data+'\n')
    return write

  # Export group
  fa = open(Join(path, 'factories/', self.uname), 'w')
  self.WriteCSLibHeader(Write(fa))
  groupDeps = self.GetDependencies()
  use_imposter = self.HasImposter()
  ExportMaterials(Write(fa), 2, path, groupDeps, use_imposter)
  self.WriteCSGroup(Write(fa), 2, use_imposter, dontClose=False)
  fa.close()

bpy.types.Group.AsCSLib = GroupAsCSLib


def WriteCSGroup(self, func, depth=0, use_imposter=False, dontClose=False):
  """ Write an xml description of the meshes composing this group:
      objects are exported as instances or submeshes of a general mesh
  """

  # Get mapping buffers and submeshes for the objects composing the group
  meshData = []
  subMeshess = []
  mappingBuffers = []
  mappingVertices = []
  mappingNormals = []
  indexV = 0

  for m, ob in self.allObjects():
    numCSVertices = 0
    indexObject = find(lambda obCpy: obCpy.name[:-4] == ob.name[:len(obCpy.name[:-4])], meshData)
    if indexObject == None:
      # Export group objects as instances of the general mesh
      if self.groupedInstances:
        # Get a deep copy of the object
        obCpy = ob.GetTransformedCopy()
      # Export group objects as submeshes of the general mesh
      else:
        # Get the world transformation of this object
        matrix = ob.matrix_world
        if m:
          matrix = matrix * m
        # Get a deep copy of this object, transformed to its world position
        obCpy = ob.GetTransformedCopy(matrix)
      # Tessellate the copied object
      obCpy.data.update_faces()
      meshData.append(obCpy)
      # Generate mapping buffers
      mapVert, mapBuf, norBuf = obCpy.data.GetCSMappingBuffers()
      numCSVertices = len(mapVert)
      if B2CS.properties.enableDoublesided and obCpy.data.show_double_sided:
        numCSVertices = 2*len(mapVert)
      # Generate submeshes
      subMeshess.append(obCpy.data.GetSubMeshes(obCpy.name,mapBuf,indexV))
      mappingBuffers.append(mapBuf)
      mappingVertices.append(mapVert)
      mappingNormals.append(norBuf)
      indexV += numCSVertices

      warning = "(WARNING: double sided mesh implies duplication of its vertices)" \
          if B2CS.properties.enableDoublesided and obCpy.data.show_double_sided else ""
      print('number of CS vertices for mesh "%s" = %s  %s'%(obCpy.name,numCSVertices,warning))

  # Export the group of objects as a general mesh factory
  func(' '*depth + '<meshfact name=\"%s\">'%(self.uname))

  if self.groupedInstances:
    # EXPORT OBJECTS AS INSTANCES OF THE GENERAL MESH
    print("The objects composing group '%s' are exported as instances of a general mesh"%(self.uname))
    func(' '*depth + '  <instances>')

    # Export first object of the group as a basic general mesh
    m, ob = self.allObjects()[0]

    func(' '*depth + '    <meshfact name=\"%s-instance\">'%(self.uname))
    func(' '*depth + '      <plugin>crystalspace.mesh.loader.factory.genmesh</plugin>')
    #if use_imposter:
    #  func(' '*depth + '      <imposter range="10.0" tolerance="45.0" camera_tolerance="45.0" shader="lighting_imposter"/>')
    mat = ob.GetDefaultMaterial()
    if mat != None and mat.priority != 'object':
      func(' '*depth + '      <priority>%s</priority>'%(mat.priority))
    if mat != None and mat.zbuf_mode != 'zuse':
      func(' '*depth + '      <%s/>'%(mat.zbuf_mode))
    if ob.data and ob.data.no_shadow_receive:
      func(' '*depth + '      <noshadowreceive />')
    if ob.data and ob.data.no_shadow_cast:
      func(' '*depth + '      <noshadowcast />')
    if ob.data and ob.data.limited_shadow_cast:
      func(' '*depth + '      <limitedshadowcast />')
    func(' '*depth + '      <params>')

    # Export render buffers
    indexObject = find(lambda obCpy: obCpy.name[:-4] == ob.name[:len(obCpy.name[:-4])], meshData)
    args = {}
    args['meshData'] = [meshData[indexObject]]
    args['mappingBuffers'] = [mappingBuffers[indexObject]]
    args['mappingVertices'] = [mappingVertices[indexObject]]
    args['mappingNormals'] = [mappingNormals[indexObject]]
    for buf in GetRenderBuffers(**args):
      buf.AsCS(func, depth+8)   

    # Export submeshes
    for sub in ob.data.GetSubMeshes(ob.name,mappingBuffers[indexObject]):
      sub.AsCS(func, depth+8)

    func(' '*depth + '      </params>')
    func(' '*depth + '    </meshfact>')

    # Export objects of the group as instances of the basic general mesh
    min_corner = mathutils.Vector((0.0, 0.0, 0.0))
    max_corner = mathutils.Vector((0.0, 0.0, 0.0))
    for m, ob in self.allObjects():
      # Define an instance of the general mesh
      func(' '*depth + '    <instance>')
      matrix = ob.matrix_world
      if m: 
        matrix = matrix * m
      MatrixAsCS(matrix, func, depth+4, True)
      func(' '*depth + '    </instance>')
      
      # Determine object's bounding box
      for corner in ob.bound_box:
        corner =  matrix * mathutils.Vector(corner)
        for i in range(3):
          if corner[i] < min_corner[i]:
            min_corner[i] = corner[i]
          elif corner[i] > max_corner[i]:
            max_corner[i] = corner[i]      
      
    func(' '*depth + '  </instances>')

    # Export group's bounding box
    func(' '*depth + '  <bbox>')
    func(' '*depth + '    <v x="%s" y="%s" z="%s" />'%(min_corner[0], min_corner[2], min_corner[1]))
    func(' '*depth + '    <v x="%s" y="%s" z="%s" />'%(max_corner[0], max_corner[2], max_corner[1]))
    func(' '*depth + '  </bbox>')

    func(' '*depth + '</meshfact>')

  else:
    # EXPORT OBJECTS AS SUBMESHES OF THE GENERAL MESH
    print("The objects composing group '%s' are exported as submeshes of a general mesh"%(self.uname))
    func(' '*depth + '  <plugin>crystalspace.mesh.loader.factory.genmesh</plugin>')
    #if use_imposter:
    #  func(' '*depth + '  <imposter range="10.0" tolerance="45.0" camera_tolerance="45.0" shader="lighting_imposter"/>')
    for m, ob in self.allObjects():
      mat = ob.GetDefaultMaterial(notifications = False)
      if mat != None and mat.priority != 'object':
        func(' '*depth + '  <priority>%s</priority>'%(mat.priority))
      if mat != None and mat.zbuf_mode != 'zuse':
        func(' '*depth + '  <%s/>'%(mat.zbuf_mode))
      if ob.data and ob.data.no_shadowreceive:
        func(' '*depth + '  <noshadowreceive />')
      if ob.data and ob.data.no_shadow_cast:
        func(' '*depth + '  <noshadowcast />')
      if ob.data and ob.data.limited_shadow_cast:
        func(' '*depth + '  <limitedshadowcast />')
    func(' '*depth + '  <params>')
    
    def SubmeshesLackMaterial(subMeshess):
      for submeshes in subMeshess:
        for sub in submeshes:
          if not sub.material:
            return True
      return False

    # There is a submesh without a material
    if SubmeshesLackMaterial(subMeshess):
      mat = None
      for m, ob in self.allObjects():
        mat = ob.GetDefaultMaterial(notifications = False)
        if mat != None:
          break
      if mat == None:
        print('WARNING: Factory "%s" has no material!'%(self.name))
      func(' '*depth + '    <material>%s</material>'%(mat.uname if mat!=None else 'None'))   
  
    # Export the render buffers of all objects composing the group
    args = {}
    args['meshData'] = meshData
    args['mappingBuffers'] = mappingBuffers
    args['mappingVertices'] = mappingVertices
    args['mappingNormals'] = mappingNormals
    for buf in GetRenderBuffers(**args):
      buf.AsCS(func, depth+4)
    
    # Export the submeshes composing the group's objects
    for submeshes in subMeshess:
      for sub in submeshes:
        sub.AsCS(func, 6)
  
    func(' '*depth + '  </params>')
    func(' '*depth + '</meshfact>')
    
  if not dontClose:
    func("</library>")
  
  # Delete working copies of objects
  for obCpy in meshData:
    bpy.data.objects.remove(obCpy)

bpy.types.Group.WriteCSGroup = WriteCSGroup


def HasImposter(self):
  """ Indicates if one of the objects belonging to this group
      uses an imposter
  """
  for m, ob in self.allObjects():
    if ob.data.use_imposter:
      return True
  return False

bpy.types.Group.HasImposter = HasImposter


def CheckVisibility(self):
  """ Return True if all objects composing the group are visible;
      False otherwise
  """
  for ob in self.objects:
    if ob.hide:
      # There is only one visibility factor for all objects 
      # composing a group => if one of them is hidden,
      # they are all hidden
      return False
  return True

bpy.types.Group.CheckVisibility = CheckVisibility


def GroupDependencies(self):
  """ Get the materials and the textures associated with
      the objects composing the group
  """
  dependencies = EmptyDependencies()
  for m, ob in self.allObjects():
    MergeDependencies(dependencies, ob.GetMaterialDependencies())
  return dependencies

bpy.types.Group.GetDependencies = GroupDependencies
