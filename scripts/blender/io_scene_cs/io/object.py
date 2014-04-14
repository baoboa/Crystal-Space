import bpy
from mathutils import *

from .util import *
from .transform import *
from .renderbuffer import *
from .morphtarget import *
from .material import *
from io_scene_cs.utilities import B2CS


class Hierarchy:
  OBJECTS = {}
  exportedFactories = []
  def __init__(self, anObject, anEmpty):
    self.object = anObject
    self.empty = anEmpty
    #uname = anEmpty.uname if anEmpty else anObject.uname
    uname = anObject.uname
    if uname in Hierarchy.OBJECTS:
      self.id = Hierarchy.OBJECTS[uname]
    else:
      self.id = len(Hierarchy.OBJECTS)
      Hierarchy.OBJECTS[uname] = self.id

  def _Get(obj):
    return obj, [Hierarchy._Get(child) for child in obj.children]

  def Get(self):
    return Hierarchy._Get(self.object)

  @property
  def uname(self):
    return "hier" + str(self.id)

  def AsCSRef(self, func, depth=0, dirName='factories/'):
    if self.object.parent_type != 'BONE' and self.object.data.name not in Hierarchy.exportedFactories:
      func(' '*depth +'<library>%s%s</library>'%(dirName,self.object.data.name))

  def GetDependencies(self):
    dependencies = EmptyDependencies()

    subMeshess = {}
    objects = self.Get()
    def Gets(objs):
      for ob, children in objs:
        if ob.data not in subMeshess:
          subMeshess[ob.data] = ob.data.GetSubMeshes()
        Gets(children)

    Gets([objects])

    for subs in subMeshess.values():
      for sub in subs:
        MergeDependencies(dependencies, sub.GetDependencies())

    return dependencies


#======== Genmesh and Animesh ===========================================================

  def WriteCSLibHeader(self, func, animesh=False):
    """ Write an xml header for a CS library
        param animesh: indicates if the library decribes an animesh
    """
    func('<?xml version="1.0" encoding="UTF-8"?>')
    func("<library xmlns=\"http://crystalspace3d.org/xml/library\">")

    if animesh:
      func('  <plugins>')
      func('    <plugin name="animesh">crystalspace.mesh.loader.animesh</plugin>')
      func('    <plugin name="animeshfact">crystalspace.mesh.loader.factory.animesh</plugin>')
      func('    <plugin name="skeletonfact">crystalspace.mesh.loader.factory.skeleton</plugin>')
      func('  </plugins>')      


  def WriteCSAnimeshHeader(self, func, depth, skelRef=True):
    """ Write an xml header for a CS animesh factory
        param skelRef: indicates if a skeleton library should be
              specified for this mesh
    """
    # Reference skeleton as a CS library
    if skelRef and self.object.type == 'ARMATURE':
      func(" "*depth + "<library>factories/%s_rig</library>"%(self.object.name))

    # Animesh factory header
    func(" "*depth + "<meshfact name=\"%s\">"%(self.object.name))
    func(" "*depth + "  <plugin>crystalspace.mesh.loader.factory.animesh</plugin>")

    # Render priority and Z-mode properties
    mat = self.object.GetDefaultMaterial()
    if mat != None and mat.priority != 'object':
      func(' '*depth + '  <priority>%s</priority>'%(mat.priority))
    if mat != None and mat.zbuf_mode != 'zuse':
      func(' '*depth + '  <%s/>'%(mat.zbuf_mode))

    # Shadow properties
    noshadowreceive = noshadowcast = limitedshadowcast = False
    for child in self.object.children:
      if child.type == 'MESH' and child.parent_type != 'BONE':
        if child.data.no_shadow_receive:
          noshadowreceive = True
        if child.data.no_shadow_cast:
          noshadowcast = True
        if child.data.limited_shadow_cast:
          limitedshadowcast = True
    if noshadowreceive:
      func(' '*depth + '  <noshadowreceive />')
    if noshadowcast:
      func(' '*depth + '  <noshadowcast />')
    if limitedshadowcast:
      func(' '*depth + '  <limitedshadowcast />')
      

  def AsCSLib(self, path='', animesh=False, **kwargs):
    """ Export this Blender mesh as a CS library file entitled '<mesh name>' in the 
        '<path>/factories' folder; it is exported as a general mesh or an animated 
        mesh depending of the 'animesh' parameter; if an armature is defined for an 
        animesh, it is exported as a library file entitled '<mesh name>_rig' in the
        'factories' folder
    """

    def Write(fi):
      def write(data):
        fi.write(data+'\n')
      return write

    if self.object.data.name in Hierarchy.exportedFactories:
      print('Skipping "%s" factory export, already done' % (self.object.data.name))
      return
    # Export mesh
    fa = open(Join(path, 'factories/', self.object.data.name), 'w')
    self.WriteCSLibHeader(Write(fa), animesh)
    objectDeps = self.object.GetDependencies()
    use_imposter = not animesh and self.object.data.use_imposter
    ExportMaterials(Write(fa), 2, path, objectDeps, use_imposter)
    if animesh:
      self.WriteCSAnimeshHeader(Write(fa), 2)
    self.WriteCSMeshBuffers(Write(fa), 2, path, animesh, dontClose=False)
    fa.close()
    Hierarchy.exportedFactories.append(self.object.data.name)

    # Export skeleton and animations
    if self.object.type == 'ARMATURE' and self.object.data.bones:
      skel = self.object
      print('Exporting skeleton and animations:', Join(path, 'factories/', '%s_rig'%(skel.name)))
      fb = open(Join(path, 'factories/', '%s_rig'%(skel.name)), 'w')
      skel.data.AsCSSkelAnim(Write(fb), 2, skel, dontClose=False)
      fb.close()


  def WriteCSMeshBuffers(self, func, depth=0, path='', animesh=False, dontClose=False, **kwargs):
    """ Write an xml decription of a Blender object and its children of type 'MESH':
        - animesh=False: mesh and children without armature neither morph targets 
          are exported as imbricated Crystal Space general mesh factories
        - animesh=True: mesh and children with armature and/or morph targets 
          are exported as a Crystal Space animated mesh factory
    """

    # Build CS mapping buffers and submeshes for this object
    print("Building CS mapping buffers...")
    meshData = []
    subMeshess = []
    mappingBuffers = []
    mappingVertices = []
    mappingNormals = []
    scales = []
    objects = self.Get()

    def Gets(objs, indexV):
      total = 0
      for ob, children in objs:
        numCSVertices = 0
        if (ob.type == 'MESH') and (not ob.hide):
          # Socket objects must be exported as general meshes
          if animesh and ob.parent_type=='BONE':
            continue
          indexObject = find(lambda obCpy: obCpy.data == ob.data, meshData)
          if indexObject == None:
            # Get object's scale
            scale = ob.GetScale()
            scales.append(scale)
            # Get a deep copy of the object
            if ((animesh and ob.parent and ob.parent.type == 'ARMATURE') or \
                (not animesh and ob.parent and ob.parent.type == 'MESH'))   \
               and ob.parent_type != 'BONE':
              # If the mesh is child of another object (i.e. an armature in case of 
              # animesh export and a mesh in case of genmesh export), transform the 
              # copied object relatively to its parent
              obCpy = ob.GetTransformedCopy(ob.relative_matrix)
            else:
              obCpy = ob.GetTransformedCopy()
            # Tessellate the copied mesh object
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
            if animesh:
              indexV += numCSVertices

            warning = "(WARNING: double sided mesh implies duplication of its vertices)" \
                if B2CS.properties.enableDoublesided and obCpy.data.show_double_sided else ""
            print('number of CS vertices for mesh "%s" = %s  %s'%(obCpy.name,numCSVertices,warning))

        total += numCSVertices + Gets(children, indexV)
      return total

    totalVertices = Gets([objects], 0)
    print('--> Total number of CS vertices = %s'%(totalVertices))

    # Export meshes as imbricated CS general mesh factories
    import copy
    if not animesh:
      def Export(objs, d):
        for ob, children in objs:
          export = (ob.type == 'MESH' and not ob.hide)
          if export:
            indexObject = find(lambda obCpy: obCpy.name[:-4] == ob.name[:len(obCpy.name[:-4])], meshData)
            lib = "from library '%s'"%(bpy.path.abspath(ob.library.filepath)) if ob.library else ''
            print('EXPORT mesh "%s" %s'%(ob.name, lib))
            args = copy.deepcopy(kwargs)
            args['meshData'] = [meshData[indexObject]]
            args['subMeshes'] = subMeshess[indexObject]
            args['mappingBuffers'] = [mappingBuffers[indexObject]]
            args['mappingVertices'] = [mappingVertices[indexObject]]
            args['mappingNormals'] = [mappingNormals[indexObject]]
            args['scales'] = [scales[indexObject]]
            args['dontClose'] = True
            ob.AsCSGenmeshLib(func, d, **args)
          Export(children, d+2)
          if export:
            func(" "*d + "</meshfact>")
          
      Export([objects], depth)

    # Export meshes as a CS animated mesh factory
    else:  
      args = copy.deepcopy(kwargs)
      args['meshData'] = meshData
      args['subMeshess'] = subMeshess
      args['mappingBuffers'] = mappingBuffers
      args['mappingVertices'] = mappingVertices
      args['mappingNormals'] = mappingNormals
      args['scales'] = scales
      self.AsCSAnimeshLib(func, depth, totalVertices, dontClose, **args)

    if not dontClose:
      func("</library>")

    # Delete working copies of the exported objects
    for obCpy in meshData:
      bpy.data.objects.remove(obCpy)


#======== Animesh ==============================================================

  def AsCSAnimeshLib(self, func, depth=0, totalVertices=0, dontClose=False, **kwargs):
    """ Write an xml description of this mesh, including children meshes,
        as a CS animated mesh factory (i.e. material, render buffers, submeshes, 
        bone influences, morph targets, sockets)
    """

    # Recover submeshes from kwargs
    subMeshess = kwargs.get('subMeshess', [])

    # Export the animesh factory
    func(" "*depth + "  <params>")

    # Take the first found material as default object material
    mat = self.object.GetDefaultMaterial()
    if mat != None:
      func(" "*depth + "    <material>%s</material>"%(mat.uname))
    else:
      func(" "*depth + "    <material>%s</material>"%(self.uv_texture if self.uv_texture!=None else 'None'))

    # Export object's render buffers
    print('EXPORT factory "%s"' % (self.object.name))
    for buf in GetRenderBuffers(**kwargs):
      buf.AsCS(func, depth+4, True)

    # Export bone influences
    if self.object.type == 'ARMATURE':
      # Specify skeleton name
      func(' '*depth + '    <skeleton>%s_rig</skeleton>'%(self.object.name))

      func(' '*depth + '    <boneinfluences>')
      for influences in self.object.data.GetBoneInfluences(**kwargs):
        func(' '*depth + '      <bi bone="%s" weight="%.4f"/>'%(influences[0],influences[1]))
      func(' '*depth + '    </boneinfluences>')

    # Export object's submeshes
    for submeshes in subMeshess:
      for sub in submeshes:
        sub.AsCS(func, depth+4, True)

    # Export morph targets
    for mt in GetMorphTargets(totalVertices, **kwargs):
      mt.AsCS(func, depth+4)

    # Find sockets
    if self.object.type == 'ARMATURE':
      socketList = {}
      for ob in bpy.data.objects:
        if ob.parent==self.object and ob.parent_type=='BONE' and ob.parent_bone:
          if ob.parent_bone not in socketList.keys():
            socketList[ob.parent_bone] = []
          socketList[ob.parent_bone].append(ob)

      # Export sockets
      bones = self.object.data.bones
      for boneName in socketList.keys():
        for socketObject in socketList[boneName]:
          bone = bones[boneName]
          bone.AsCSSocket(func, depth+4, socketObject, self.object)

    func(' '*depth + '    <tangents automatic="true"/>')
    func(" "*depth + "  </params>")

    if not dontClose:
      func(" "*depth + "</meshfact>")


#======== Genmesh ==============================================================

def AsCSGenmeshLib(self, func, depth=0, **kwargs):
  """ Write an xml description of this mesh as a CS general mesh factory
      (i.e. header, material, render buffers and submeshes)
  """

  # Write genmesh header
  func(' '*depth + '<meshfact name=\"%s\">'%(self.data.name))
  func(' '*depth + '  <plugin>crystalspace.mesh.loader.factory.genmesh</plugin>')
  if self.data.use_imposter:
    func(' '*depth + '  <imposter range="100.0" tolerance="0.4" camera_tolerance="0.4" shader="lighting_imposter"/>')
  mat = self.GetDefaultMaterial()
  if mat != None and mat.priority != 'object':
    func(' '*depth + '  <priority>%s</priority>'%(mat.priority))
  if mat != None and mat.zbuf_mode != 'zuse':
    func(' '*depth + '  <%s/>'%(mat.zbuf_mode))
  if self.data.no_shadow_receive:
    func(' '*depth + '  <noshadowreceive />')
  if self.data.no_shadow_cast:
    func(' '*depth + '  <noshadowcast />')
  if self.data.limited_shadow_cast:
    func(' '*depth + '  <limitedshadowcast />')
  func(' '*depth + '  <params>')

  # Recover submeshes from kwargs
  subMeshes = kwargs.get('subMeshes', [])

  # Take the first found material as default object material
  if mat != None:
    func(" "*depth + "    <material>%s</material>"%(mat.uname))
  else:
    func(" "*depth + "    <material>%s</material>"%(self.uv_texture if self.uv_texture!=None else 'None'))
  if mat != None and not mat.HasDiffuseTexture() and mat.uv_texture != 'None':
    func(' '*depth + '    <shadervar type="texture" name="tex diffuse">%s</shadervar>'%(mat.uv_texture))

  # Export mesh's render buffers
  for buf in GetRenderBuffers(**kwargs):
    buf.AsCS(func, depth+4, False)

  # Export mesh's submeshes
  if len(subMeshes)==1:
    subMeshes[0].AsCSTriangles(func, depth+4, False)
  else:
    for sub in subMeshes:
      sub.AsCS(func, depth+4, False)

  if 'meshData' in kwargs:
    meshData = kwargs['meshData']
    # We know there is only one mesh in the data.
    if meshData[0].data.use_auto_smooth:
      func(" "*depth + "    <autonormals/>")

  func(' '*depth + '  </params>')

  # Don't close 'meshfact' flag if another mesh object is defined as 
  # an imbricated genmesh factory of this factory
  if not kwargs.get('dontClose', False):
    func(' '*depth + '</meshfact>')

bpy.types.Object.AsCSGenmeshLib = AsCSGenmeshLib


#======== Object ====================================================================

# Property defining an UV texture's name for a mesh ('None' if not defined)
StringProperty(['Object'], attr="uv_texture", name="UV texture", default='None')


def ObjectAsCS(self, func, depth=0, **kwargs):
  """ Write a reference to this object (as part of a sector in the 'world' file); 
      only mesh, armature, group and lamp objects are described
  """
  name = self.uname
  if 'name' in kwargs:
    name = kwargs['name']+':'+name

  if self.type == 'MESH':
    if len(self.data.vertices)!=0 and len(self.data.all_faces)!=0:
      # Temporary disable of meshref support because of incompatibility
      # issues with lighter2 that needs to be genmesh aware in order to
      # pack the lightmaps per submeshes
      #func(' '*depth +'<meshref name="%s_object">'%(self.name))
      #func(' '*depth +'  <factory>%s</factory>'%(self.name))

      func(' '*depth +'<meshobj name="%s_object">'%(self.name))

      if self.parent and self.parent.type == 'ARMATURE':
        func(' '*depth +'  <plugin>crystalspace.mesh.loader.animesh</plugin>')
      else:
        func(' '*depth +'  <plugin>crystalspace.mesh.loader.genmesh</plugin>')
      func(' '*depth +'  <params>')
      func(' '*depth +'    <factory>%s</factory>'%(self.data.name))
      func(' '*depth +'  </params>')

      if self.parent and self.parent_type == 'BONE':
        matrix = self.matrix_world
      else:
        matrix = self.relative_matrix
        if 'transform' in kwargs:
          matrix =  matrix * kwargs['transform']
      MatrixAsCS(matrix, func, depth+2)

      #func(' '*depth +'</meshref>')
      func(' '*depth +'</meshobj>')

  elif self.type == 'ARMATURE':
    # Temporary disable of meshref support because of incompatibility
    # issues with lighter2 that needs to be genmesh aware in order to
    # pack the lightmaps per submeshes
    #func(' '*depth +'<meshref name="%s_object">'%(name))
    #func(' '*depth +'  <factory>%s</factory>'%(name))

    func(' '*depth +'<meshobj name="%s_object">'%(self.name))
    func(' '*depth +'  <plugin>crystalspace.mesh.loader.animesh</plugin>')
    func(' '*depth +'  <params>')
    func(' '*depth +'    <factory>%s</factory>'%(self.name))
    func(' '*depth +'  </params>')

    matrix = self.relative_matrix
    if 'transform' in kwargs:
      matrix =  matrix * kwargs['transform']
    MatrixAsCS(matrix, func, depth+2)

    #func(' '*depth +'</meshref>')
    func(' '*depth +'</meshobj>')

  elif self.type == 'LAMP':
    func(' '*depth +'<light name="%s">'%(name))
    # Flip Y and Z axis.
    func(' '*depth +'  <center x="%f" z="%f" y="%f" />'% tuple(self.relative_matrix.to_translation()))
    func(' '*depth +'  <color red="%f" green="%f" blue="%f" />'% tuple(self.data.color))
    func(' '*depth +'  <radius>%f</radius>'%(self.data.distance))
    func(' '*depth +'  <attenuation>linear</attenuation>')
    if self.data.no_shadows:
      func(' '*depth +'  <noshadows />')
    func(' '*depth +'</light>')

  elif self.type == 'EMPTY':
    if self.dupli_type=='GROUP' and self.dupli_group:
      if self.dupli_group.doMerge:
        self.dupli_group.AsCS(func, depth, transform=self.relative_matrix)
      else:
        for ob in self.dupli_group.objects:
          if not ob.parent:
            ob.AsCS(func, depth, transform=self.relative_matrix, name=self.uname)
    else:
      func(' '*depth +'<node name="%s">'%(name))
      func(' '*depth +'  <position x="%f" z="%f" y="%f" />'% tuple(self.relative_matrix.to_translation()))
      func(' '*depth +'</node>')

    #Handle children: translate to top level.
    for obj in self.children:
      if not obj.hide:
        obj.AsCS(func, depth, transform=self.relative_matrix, name=self.uname)

  elif self.type != 'CAMERA':
    print('\nWARNING: Object "%s" of type "%s" is not supported!'%(self.name, self.type))

bpy.types.Object.AsCS = ObjectAsCS


def IsExportable(self):
  """ Check if this object can be exported
  """
  # Hidden objects are never exported
  # (except for empty objects, which are composed of other objects)
  if self.hide and self.type != 'EMPTY':
    return False

  # Armature objects are exported if at least one of 
  # its child meshes is visible
  if self.type == 'ARMATURE':
    for child in self.children:
      if child.parent_type != 'BONE' and not child.hide:
        return True
    return False

  # Mesh objects are exported as individual mesh factories if 
  # - they are not submeshes of a visible armature,
  # - they are not submeshes of a visible mesh,
  # - they are not portals, 
  # - they have a non null number of vertices and faces,
  # - they are socket objects
  if self.type == 'MESH':
    def IsChildOfExportedFactory(ob):
      ''' Test if ob is child of a visible mesh or armature
          (and not linked by a socket to a bone)
      '''
      if ob.parent_type=='BONE' and ob.parent_bone:
        return False
      if ob.parent:
        return (ob.parent.type=='ARMATURE' and not ob.parent.hide) or \
               (ob.parent.type=='MESH' and not ob.parent.hide)
      return False

    if not IsChildOfExportedFactory(self) and not self.data.portal \
          and len(self.data.vertices)!=0 and len(self.data.all_faces)!=0:
      return True
    return False      

  # The export of Empty objects depends of their components
  if self.type == 'EMPTY':
    # Groups of objects are exported if the group itself and 
    # all its composing elements are visible
    if self.dupli_type=='GROUP' and self.dupli_group:
      if self.hide or not self.dupli_group.CheckVisibility():
        return False

      # Check if this group is part of a merging group
      mergingGroup = self.hasMergingGroup()
      if mergingGroup:
        # This group is exported only if all objects of the
        # merging group are visible
        for gr in mergingGroup.objects:
          if gr != self:
            if gr.hide:
              return False
            if not gr.dupli_group.CheckVisibility():
              return False
    else:
      # Empty objects which are not groups are exported as map nodes
      # (notice that each of their components can be individually exported)
      return True

  # All other types of objects are always exportable (if they are visible)
  return True

bpy.types.Object.IsExportable = IsExportable


def ObjectDependencies(self, empty=None):
  """ Get the dependencies of this object and its children, i.e. textures, 
      materials, armatures, meshes and groups associated with it
  """
  dependencies = EmptyDependencies()  

  if self.type == 'ARMATURE':
    # Object with skeleton ==> 'A' type (animesh)
    if self.children:
      hier = Hierarchy(self, empty)
      print('ObjectDependencies: ', hier.uname, '-', self.name)
      dependencies['A'][hier.uname] = hier
      MergeDependencies(dependencies, self.GetMaterialDependencies())

  elif self.type == 'MESH':
    if self.data.shape_keys:
      # Mesh with morph targets ==> 'A' type (animesh)
      hier = Hierarchy(self, empty)
      print('ObjectDependencies: ', hier.uname, '-', self.name)
      dependencies['A'][hier.uname] = hier
    else:
      # Mesh without skeleton neither morph target 
      # or child of a bone (attached by socket) ==> 'F' type (genmesh)
      hier = Hierarchy(self, empty)
      print('ObjectDependencies: ', hier.uname, '-', self.name)
      dependencies['F'][hier.uname] = hier

    # Material of the mesh ==> 'M' type
    # Associated textures  ==> 'T' type
    MergeDependencies(dependencies, self.GetMaterialDependencies())

  elif self.type == 'EMPTY':
    if self.dupli_type=='GROUP' and self.dupli_group:
      # Group of objects ==> 'G' type
      if self.dupli_group.doMerge:
        dependencies['G'][self.dupli_group.uname] = self.dupli_group
        MergeDependencies(dependencies, self.dupli_group.GetDependencies())
      else:
        for ob in [ob for ob in self.dupli_group.objects if not ob.parent]:
          MergeDependencies(dependencies, ob.GetDependencies(self))

  return dependencies

bpy.types.Object.GetDependencies = ObjectDependencies


def GetDefaultMaterial (self, notifications = True):
  """ Get the default material of this armature or mesh object
  """
  # Initialize default material with 'None'
  mat = None

  # Armature object
  if self.type == 'ARMATURE':
    # Take the first found material among children as default object material
    foundMaterial = False
    for child in self.children:
      if child.type == 'MESH' and child.parent_type!='BONE':
        mat = child.GetDefaultMaterial(notifications)
        if mat != None:
          foundMaterial = True
          break
    if not foundMaterial and notifications:
      print('WARNING: armature object "%s" has no child with texture coordinates'%(self.name))
  elif self.type == 'MESH':
    # Mesh object
    if len(self.data.uv_textures) != 0 and self.data.GetFirstMaterial():
      # Take the first defined material as default object material
      mat = self.data.GetFirstMaterial()
    elif notifications:
      if len(self.data.uv_textures) == 0:
        print('WARNING: mesh object "%s" has no texture coordinates'%(self.name))
          
  return mat

bpy.types.Object.GetDefaultMaterial = GetDefaultMaterial


def GetMaterialDeps(self):
  """ Get materials and textures associated with this object
  """
  dependencies = EmptyDependencies()
  if self.type == 'MESH':
    foundDiffuseTexture = False
    # Material of the mesh ==> 'M' type
    # and associated textures ==> 'T' type
    for mat in self.materials:
      dependencies['M'][mat.uname] = mat
      MergeDependencies(dependencies, mat.GetDependencies())
      foundDiffuseTexture = foundDiffuseTexture or mat.HasDiffuseTexture() 
    # Search for a diffuse texture if none is defined among the materials
    if not foundDiffuseTexture:
      if self.data and self.data.uv_textures and self.data.uv_textures.active:
        # UV texture of the mesh ==> 'T' type
        for index, facedata in enumerate(self.data.uv_textures.active.data):
          if facedata.image and facedata.image.uname not in dependencies['T'].keys():
            dependencies['T'][facedata.image.uname] = facedata.image
            material = self.data.GetMaterial(self.data.all_faces[index].material_index)
            if material:
              material.uv_texture = facedata.image.uname
            else:
              # Create a material if the mesh has a texture but no material
              dependencies['TM'][facedata.image.uname] = facedata.image
              if self.uv_texture == 'None':
                self.uv_texture = facedata.image.uname
  return dependencies

bpy.types.Object.GetMaterialDeps = GetMaterialDeps


def GetMaterialDependencies(self):
  """ Get materials and textures associated with this object
      and its children
  """
  dependencies = self.GetMaterialDeps()
  for child in self.children:
    if child.type == 'MESH' and child.parent_type != 'BONE':
      MergeDependencies(dependencies, child.GetMaterialDependencies())
  return dependencies

bpy.types.Object.GetMaterialDependencies = GetMaterialDependencies


def HasImposter(self):
  """ Indicates if this object uses an imposter
  """
  if self.type == 'MESH':
    if self.data and self.data.use_imposter:
      return True
  elif self.type == 'EMPTY':
    if self.dupli_type=='GROUP' and self.dupli_group:
      if self.dupli_group.HasImposter():
        return True
  return False

bpy.types.Object.HasImposter = HasImposter


def GetScale(self):
  """ Get object scale
  """
  if self.parent and self.parent_type == 'BONE':
    pscale = self.parent.scale    
    oscale = self.scale
    scale = Vector((pscale[0]*oscale[0], pscale[1]*oscale[1], pscale[2]*oscale[2]))
  elif self.parent and self.parent.type == 'ARMATURE':
    scale = self.parent.scale
  else:
    scale = self.scale

  return scale

bpy.types.Object.GetScale = GetScale


def GetTransformedCopy(self, matrix = None):
  """ Get a deep copy of this object, transformed by matrix
  """
  # Make a copy of the current object
  obCpy = self.copy()
  # Make a deep copy of the object data block
  obCpy.data = self.data.copy()
  # Transform the copied object
  if matrix:
    obCpy.data.transform(matrix)
    obCpy.data.calc_normals()

  return obCpy

bpy.types.Object.GetTransformedCopy = GetTransformedCopy
