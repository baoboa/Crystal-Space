import bpy

from sys import float_info
from mathutils import *

from .util import *
from .transform import *
from .renderbuffer import *
from .submesh import *
from io_scene_cs.utilities import B2CS
from io_scene_cs.utilities import StringProperty


def GetMaterial(self, index):
  if index < len(self.materials):
    return self.materials[index]
  return None

bpy.types.Mesh.GetMaterial = GetMaterial

def GetFirstMaterial(self):
  for i,mat in enumerate(self.materials):
    if self.materials[i] != None:
      return self.materials[i]
  return None

bpy.types.Mesh.GetFirstMaterial = GetFirstMaterial

def GetSubMeshesRaw(self, name, indexV, indexGroups, mappingBuffer = []):
  """ Compute the CS submeshes of this Blender mesh by remapping
      Blender vertices to CS vertices, triangulating the faces and
      duplicating the double sided faces
      param name: name of the mesh
      param indexV: starting index of the CS vertices
      param indexGroups: buffer defining for each submesh (identified by the 
            mesh name, the material and the texture) its CS faces, composed
            by three vertices
      param mappingBuffer: mapping buffer defining a CS vertex for each
            face vertex of the Blender mesh
  """
  
  # Starting index of the cs vertices composing this submesh
  firstIndex = indexV
  # Active UV texture
  tface = self.all_uv_textures.active.data if self.all_uv_textures.active else None
  # For each face composing this mesh
  for index, face in enumerate(self.all_faces):
    im = tface[index].image if tface else None
    # Identify the submesh by the mesh name, material and texture
    triplet = (name, self.GetMaterial(face.material_index), im)
    if triplet not in indexGroups:
      indexGroups[triplet] = []
    # Triangulate the mesh face
    indices = [v for v in face.vertices]
    tt = [2,1,0] if len(indices)==3 else [2,1,0,3,2,0]
    if len(mappingBuffer) == 0:
      # No mapping buffer => directly copy the index of Blender vertices
      for i in tt:
        indexGroups[triplet].append(face.vertices[i])
        indexV += 1
    else:
      # Mapping buffer => use the corresponding cs index of these vertices
      if not B2CS.properties.enableDoublesided or not self.show_double_sided:
        # Single sided mesh
        for i in tt:
          # Add vertices composing the triangle to submesh
          for mapV in mappingBuffer[face.vertices[i]]:
            if mapV['face'] == index and mapV['vertex'] == i:
              # cs index = firstIndex + j  
              # (where j is the index of this vertex in the submesh)
              indexGroups[triplet].append(firstIndex + mapV['csVertex'])
              indexV += 1
              break
      else:
        # Double sided mesh
        # Search for CS indices of the vertices composing the face
        frontFaceIndices = []
        tt = [0,1,2] if len(indices)==3 else [0,1,2,3]
        for i in tt:
          for mapV in mappingBuffer[face.vertices[i]]:
            if mapV['face'] == index and mapV['vertex'] == i:
              # front face:  cs index = firstIndex + 2*j  
              # back face:   cs index = firstIndex + 2*j + 1 
              # (where j is the cs index of the vertex for a single sided face)
              frontFaceIndices.append(firstIndex + 2 * mapV['csVertex'])
              break

        # Add vertices composing the triangle
        for j in [2,1,0]:
          # front side of the mesh
          indexGroups[triplet].append(frontFaceIndices[j])
          indexV += 1
        for j in [0,1,2]:
          # back side of the mesh
          indexGroups[triplet].append(frontFaceIndices[j] + 1)
          indexV += 1

        # The face is not a triangle => triangulation
        if len(indices)!=3:
          # Add vertices composing the second triangle
          for j in [3,2,0]:
            # front side of the mesh
            indexGroups[triplet].append(frontFaceIndices[j])
            indexV += 1
          for j in [3,0,2]:
            # back side of the mesh
            indexGroups[triplet].append(frontFaceIndices[j] + 1)
            indexV += 1

  return indexV, indexGroups

bpy.types.Mesh.GetSubMeshesRaw = GetSubMeshesRaw


def GetSubMeshes(self, name = '', mappingBuffer = None, numVertices = 0):
  """ Create the CS submeshes of this Blender mesh
      param name: name of the mesh
      param mappingBuffer: mapping buffer defining a CS vertex for each
            face vertex of the Blender mesh
      param numVertices: number of CS vertices previously defined      
  """
  if mappingBuffer is None:
      mappingBuffer = []
  indexV = numVertices
  indexGroups = {}
  indexV, indexGroups = self.GetSubMeshesRaw(name, indexV, indexGroups, mappingBuffer)
  return [SubMesh(group[0], group[1], group[2], indexGroups[group]) for group in indexGroups]

bpy.types.Mesh.GetSubMeshes = GetSubMeshes


def MeshAsCSRef(self, func, depth=0, dirName='factories/'):
  func(' '*depth +'<library>%s%s</library>'%(dirName,self.uname))

bpy.types.Mesh.AsCSRef = MeshAsCSRef


#======= Mapping buffers ===========================================

def GetCSMappingBuffers (self):
  """ Generate mapping buffers between Blender and CS vertices.
      Return mapVert, list defining the Blender vertex corresponding
      to each CS vertex; mapBuf, list defining a list of mappingVertex
      for each Blender vertex (i.e. the face, the index of this vertex in 
      the face and the corresponding CS vertex); and normals, list defining
      the CS vertex normals
  """

  # Determine the UV texture: we only use the active UV texture or the first one
  if self.all_uv_textures.active:
    tface = self.all_uv_textures.active.data
  elif len(self.all_uv_textures) != 0:
    tface = self.all_uv_textures[0].data
  else:
    print("WARNING: mesh",self.name,"has no UV texture")
    tface = None

  epsilon = 1e-9
  forceSmooth = False
  mapBuf = []
  mapVert = []
  normals = []
  csIndex = 0
  for vertex in self.vertices:
    mapBuf.append([])

  # For all faces of the mesh
  for indexF, face in enumerate(self.all_faces):
    faceNormal = face.normal
    faceVerts = set(face.vertices)

    # For all vertices of the face
    tt = [2,1,0] if len(face.vertices)==3 else [3, 2, 1, 0]
    for i in tt:
      vertexNormal = self.vertices[face.vertices[i]].normal

      # Evaluate face smoothness using the "use_auto_smooth" mesh property
      # (along with the "auto_smooth_angle") and the "use_smooth" properties of 
      # the current face and its adjacent faces
      smooth = forceSmooth or (face.use_smooth and not self.use_auto_smooth)
      if not smooth and not faceNormal:
        print("WARNING: mesh '%s' has no normals. Setting smooth mode."%(self.name))
        forceSmooth = True
        smooth = True
      if not smooth and face.use_smooth and self.use_auto_smooth:
        for indexf, f in enumerate(self.all_faces):
          if face!=f and f.use_smooth and len(faceVerts.intersection(set(f.vertices)))!=0:
            # All set-smoothed faces with angles less than the specified 
            # auto-smooth angle have 'smooth' normals, i.e. use vertex normals
            angle = faceNormal.angle(f.normal, None)
            if angle == None or abs(angle) < self.auto_smooth_angle:
              smooth = True
              break

      newUVVertex = False
      if len(mapBuf[face.vertices[i]]) == 0:
        # Blender vertex is not defined in CS
        # => create a new CS vertex
        mappingVertex = {'face': indexF, 'vertex': i, 'csVertex': csIndex}
        mapBuf[face.vertices[i]].append(mappingVertex)
        mapVert.append(face.vertices[i])
        normals.append(vertexNormal if smooth else faceNormal)
        csIndex += 1
      else:
        if tface == None:
          # Mesh without texture coordinates
          for mappedI, mappedV in enumerate(mapVert):
            if mappedV == face.vertices[i]:
              # Blender vertex is already defined in CS 
              # (no (u,v) coordinates to differentiate the vertices in this case)
              if smooth:
                # Only one normal per vertex in smooth mesh
                # => add a reference to the corresponding CS vertex
                mappingVertex = {'face': indexF, 'vertex': i, 'csVertex': mappedI}
                mapBuf[face.vertices[i]].append(mappingVertex)
              else:
                # Not a smooth mesh
                vertexFound = False
                for mappedI, mappedV in enumerate(mapBuf[face.vertices[i]]):
                  if faceNormal == self.all_faces[mappedV['face']].normal:
                    # Blender vertex is defined in CS with the same face normal
                    # => add a reference to the corresponding CS vertex
                    vertexFound = True
                    mappingVertex = {'face': indexF, 'vertex': i, 'csVertex': mappedV['csVertex']}
                    mapBuf[face.vertices[i]].append(mappingVertex)
                    break
                if not vertexFound:
                  # Blender vertex is defined in CS with a different face normal
                  # => create a new CS vertex
                  mappingVertex = {'face': indexF, 'vertex': i, 'csVertex': csIndex}
                  mapBuf[face.vertices[i]].append(mappingVertex)
                  mapVert.append(face.vertices[i])
                  normals.append(faceNormal)
                  csIndex += 1
              break
        else:
          # Mesh with texture coordinates
          vertexFound = False
          uv = tface[indexF].uv[i]
          for mappedI, mappedV in enumerate(mapBuf[face.vertices[i]]):
            if (abs(uv[0] - tface[mappedV['face']].uv[mappedV['vertex']][0]) < epsilon) and \
               (abs(uv[1] - tface[mappedV['face']].uv[mappedV['vertex']][1]) < epsilon) and \
               (smooth or (faceNormal == self.all_faces[mappedV['face']].normal)):
              # Blender vertex is defined in CS with the same (u,v) coordinates
              # and the same face normal
              # => add a reference to the corresponding CS vertex
              vertexFound = True
              mappingVertex = {'face': indexF, 'vertex': i, 'csVertex': mappedV['csVertex']}
              mapBuf[face.vertices[i]].append(mappingVertex)
              break
          if not vertexFound:
            # Blender vertex is defined in CS with different (u,v) coordinates
            # and/or different face normal
            # => create a new CS vertex
            mappingVertex = {'face': indexF, 'vertex': i, 'csVertex': csIndex}
            mapBuf[face.vertices[i]].append(mappingVertex)
            mapVert.append(face.vertices[i])
            normals.append(vertexNormal if smooth else faceNormal)
            csIndex += 1

  return mapVert, mapBuf, normals

bpy.types.Mesh.GetCSMappingBuffers = GetCSMappingBuffers


#======= Armature ===========================================

def GetBoneNames (bones):
  ''' Return a list of bone names with their corresponding order index
      param bones: list of bones      
  '''
  def GetChildBoneNames (children, boneNames, index):
    for child in children:
      index += 1
      boneNames[child.name] = index
      boneNames, index = GetChildBoneNames(child.children, boneNames, index)
    return boneNames, index
    
  boneNames = {}
  index = -1
  for bone in bones:
    if not bone.parent:
      index += 1
      boneNames[bone.name] = index
      boneNames, index = GetChildBoneNames(bone.children, boneNames, index)
  return boneNames
  

def AsCSSkeletonAnimations (self, func, depth, armatureObject, dontClose=False):
  """ Export Blender skeleton and associated animations
  """
  if not dontClose:
    func('<?xml version="1.0" encoding="UTF-8"?>')
    func('<library xmlns=\"http://crystalspace3d.org/xml/library\">')
  func(' '*depth + '<addon plugin="crystalspace.skeletalanimation.loader">')

  # EXPORT ANIMATIONS
  boneNames = GetBoneNames(self.bones)
  armatureName = armatureObject.name

  # Create scene for baking faster 
  # and link a copy of the armature to this new scene
  armatureObjectCpy = armatureObject.copy()
  bakingScene = bpy.data.scenes.new('bakingScene')
  bakingScene.objects.link(armatureObjectCpy)
  initialPose = armatureObjectCpy.data.pose_position
  armatureObjectCpy.data.pose_position = 'POSE'
  bakingScene.update()

  # Actions
  exportedActions = []
  actions = []
  if armatureObjectCpy.animation_data and bpy.data.actions:
    actions = bpy.data.actions

  for action in actions:
    armatureObjectCpy.animation_data.action = action
    boneKeyframes = {}
    lostBones = []
    cyclic_action = False

    # Curves
    for curve in action.fcurves:
      if not curve.is_valid:
        print("Error: Curve is not valid", curve.data_path, curve.array_index)
        continue
        
      dp = curve.data_path        # (e.g. "pose.bones['BoneName'].location")
      if dp[:10] == "pose.bones":
        dpSplit = dp[10:].split('[')[1].split(']')
        boneName = dpSplit[0][1:-1:]
        op = dpSplit[1][1:]
          
        if boneName not in boneNames:
          if boneName not in lostBones:
            lostBones.append(boneName)
          continue

        if op != 'location' and op != 'rotation_quaternion':
          continue

        # TODO: export 'scale' operations

        # Keyframes
        if boneName not in boneKeyframes:
          boneKeyframes[boneName] = []
        for keyframe in curve.keyframe_points:
          if keyframe.co[0] not in boneKeyframes[boneName]:
            boneKeyframes[boneName].append(keyframe.co[0])

    if len(lostBones):
      print("WARNING: Action '%s' not exported: the following bones are not part of the armature"%(action.name),lostBones)
      continue

    if len(boneKeyframes) == 0:
      print("WARNING: Action '%s' not exported: no keyframe defined"%(action.name))
      continue

    """
    # Verify if the action defines armature movement
    displacementFound = False
    for bone in boneKeyframes.keys():
      if len(boneKeyframes[bone]) > 1:
        displacementFound = True
        break
    if not displacementFound:
      print("INFO: Action '%s' defines a pose (no armature movement)"%(action.name))
    """

    # Write animation packet
    exportedActions.append(action)
    if len(exportedActions) == 1:
      func(' '*depth + '<animationpacket name="%s_packet">'%(armatureName))
    func(' '*depth + '  <animation name="%s">'%(action.name))

    fps = 24.0
    for boneName in boneKeyframes.keys():
      # Write bone index
      boneIndex = boneNames[boneName]
      func(' '*depth + '    <channel bone="%s">'%(boneIndex))
      frames = boneKeyframes[boneName]
      for time in sorted(frames):
        # Write time and bone position/rotation for each frame
        bakingScene.frame_set(int(round(time)))
        bone = armatureObjectCpy.data.bones[boneName]
        pos, quat = bone.GetBoneCurrentMatrix(armatureObjectCpy)
        scale = armatureObjectCpy.scale
        csTime = time/fps
        func(' '*(depth+6) + '<key time="%g" x="%s" y="%s" z="%s" qx="%s" qy="%s" qz="%s" qw="%s" />' % \
             (csTime, scale[0]*pos[0], scale[2]*pos[2], scale[1]*pos[1], quat.x, quat.z, quat.y, -quat.w))
      func(' '*depth + '    </channel>')
    func(' '*depth + '  </animation>')

  if len(exportedActions) != 0:
    # Write animations tree
    func(' '*depth + '  <node type="fsm" name="root">')
    for action in exportedActions:
      func(' '*depth + '    <state name="%s">'%(action.name))
      func(' '*depth + '      <node type="animation" animation="%s"/>'%(action.name))
      func(' '*depth + '    </state>')
    func(' '*depth + '  </node>')
    func(' '*depth + '</animationpacket>')
    print('INFO: %s animation(s) exported for skeleton "%s_rig"'%(len(exportedActions),armatureName))

  # Unlink the armature copy and delete baking scene
  armatureObjectCpy.data.pose_position = initialPose
  bakingScene.objects.unlink(armatureObjectCpy)
  bpy.data.objects.remove(armatureObjectCpy)
  bpy.data.scenes.remove(bakingScene)

  # EXPORT SKELETON
  func(' '*depth + '<skeleton name="%s_rig">'%(armatureName))
  for bone in self.bones:
    if not bone.parent:
      bone.AsCS(func, depth+2, armatureObject.matrix_world)
  if len(exportedActions):
    func(' '*depth + '  <animationpacket>%s_packet</animationpacket>'%(armatureName))
  func(' '*depth + '</skeleton>')
  print('INFO: %s bone(s) exported for skeleton "%s_rig"'%(len(boneNames),armatureName))

  func(' '*depth + '</addon>')
  if not dontClose:
    func('</library>')

bpy.types.Armature.AsCSSkelAnim = AsCSSkeletonAnimations


def GetBoneInfluences (self, **kwargs):
  """ Generate a list of bone influences (4 per vertex) for each vertex of
      the mesh animated by this armature.
      param kwargs: mapping buffers of this armature
  """
  # Recover mapping buffers from kwargs
  meshData = kwargs.get('meshData', [])
  mappingVertices = kwargs.get('mappingVertices', [])

  # Get bone influences per vertex (max 4)
  boneNames = GetBoneNames(self.bones)
  epsilon = 1e-8
  influences = []
  lostInfluences = False
  undefinedGroups = False
  lostGroups = []
  for index, ob in enumerate(meshData):
    # For each mesh depending of the armature
    for indexV in mappingVertices[index]:
      # For each vertex of the mesh
      weights = []
      for groupEl in ob.data.vertices[indexV].groups:
        # For each vertex group
        if groupEl.weight > epsilon:
          # Get the bone and its influence (weight) on the vertex
          groupIndex = find(lambda gr: gr.index==groupEl.group, ob.vertex_groups)
          if groupIndex == None:
            undefinedGroups = True
            continue
          group = ob.vertex_groups[groupIndex]
          if group.name not in boneNames:
            if group.name not in lostGroups:
              lostGroups.append(group.name)
            continue
          boneIndex = boneNames[group.name]
          weights.append([boneIndex, groupEl.weight])

          # TODO: treat all armatures, not only the root one

      # Sort bone influences of the vertex by descending order
      weights.sort(key=lambda el: el[1],reverse=True)

      # Add the 4 largest influences of the vertex
      countBones = 0
      sumWeights = 0
      for influence in weights:
        if countBones < 4:
          countBones += 1
          sumWeights += influence[1]
        else:
          lostInfluences = True
          break

      # Normalize the bone influences to 1
      vertexInfluence = []
      for i in range(countBones):
        if sumWeights > epsilon:
          weights[i][1] = weights[i][1] / sumWeights
        vertexInfluence.append(weights[i])

      # Fill vertex influences with null values if less then 4 non null
      # influences have been found for this vertex
      if countBones < 4:
        for i in range(4-countBones):
          vertexInfluence.append([0, 0.0])

      # Add bone influences to the list
      for i in range(4):
        influences.append(vertexInfluence[i])
      if B2CS.properties.enableDoublesided and ob.data.show_double_sided:
        # Duplicate the influences in case of a double sided mesh
        for i in range(4):
          influences.append(vertexInfluence[i])

  if undefinedGroups:
    print("ERROR: undefined vertex group index; corresponding bone influences have been lost!")
  if len(lostGroups):
    print("WARNING: vertex groups defined on unknown bones; corresponding bone influences have been lost!",lostGroups)
  if lostInfluences:
    print("WARNING: some bone influences have been lost (max=4/vertex)")

  return influences

bpy.types.Armature.GetBoneInfluences = GetBoneInfluences


#======= Bone ===========================================

def GetBoneMatrix (self, worldTransform):
  ''' Return translation and rotation (quaternion) parts of the local bone 
      transformation corresponding to the rest pose
  '''
  if not self.parent:
    loc, rot, scale = DecomposeMatrix(worldTransform)
    bone_matrix = self.matrix_local * rot
  else:
    parent_imatrix = Matrix(self.parent.matrix_local).inverted()
    bone_matrix = parent_imatrix * self.matrix_local

  return (bone_matrix.to_translation(), self.matrix.to_quaternion())

bpy.types.Bone.GetBoneMatrix = GetBoneMatrix


def GetBoneCurrentMatrix (self, armatureObject):
  ''' Return translation and rotation (quaternion) parts of the local bone 
      transformation corresponding to the current pose of the armature
  '''
  poseBones = armatureObject.pose.bones
  poseBone = poseBones[self.name]

  if not self.parent:
    loc, rot, scale = DecomposeMatrix(poseBone.matrix_channel)
    bone_matrix = poseBone.matrix * rot
  else:
    poseBoneMatrix = poseBone.matrix
    poseParent = poseBones[self.parent.name]
    poseParentMatrix = poseParent.matrix
    parent_imatrix = Matrix(poseParentMatrix).inverted()
    bone_matrix = parent_imatrix * poseBoneMatrix

  return (bone_matrix.to_translation(), bone_matrix.to_quaternion())

bpy.types.Bone.GetBoneCurrentMatrix = GetBoneCurrentMatrix


def AsCSBone (self, func, depth, worldTransform):
  """ Write an xml description (name and local transformation) of this bone
  """
  func(' '*depth + '<bone name="%s">'%(self.name))

  # Transform matrix
  pos, quat = self.GetBoneMatrix(worldTransform)
  scale = worldTransform.to_scale()
  func(' '*depth + '  <transform x="%s" y="%s" z="%s" qx="%s" qy="%s" qz="%s" qw="%s" />'% \
          (scale[0]*pos[0], scale[2]*pos[2], scale[1]*pos[1], quat.x, quat.z, quat.y, -quat.w))
  for childBone in self.children:
    childBone.AsCS(func, depth+2, worldTransform)

  func(' '*depth + '</bone>')

bpy.types.Bone.AsCS = AsCSBone


def GetSocketMatrix (self, socketObject, armatureObject):
  """ Get the relative transformation (location and rotation) of the socketObject
      attached to this bone, according to the current pose position of the armature
  """
  # Save current armature state (rest or pose position)
  # and set the 'pose mode'
  initialPose = armatureObject.data.pose_position
  armatureObject.data.pose_position = 'POSE'

  # Compute socket matrix for current armature pose
  poseBones = armatureObject.pose.bones
  poseBone = poseBones[self.name]
  bone_iMatrix = poseBone.matrix.inverted()
  skel_iMatrix = armatureObject.matrix_world.inverted()
  socketMatrix = bone_iMatrix * skel_iMatrix * socketObject.matrix_world

  # Restore initial armature state
  armatureObject.data.pose_position = initialPose

  # Decompose socket matrix
  loc, rot, scale = DecomposeMatrix(socketMatrix)
  rot = rot.to_euler()
  return (loc, rot)

bpy.types.Bone.GetSocketMatrix = GetSocketMatrix


def AsCSSocket(self, func, depth, socketObject, armature):
  """ Write an xml description of this socket (i.e. name of the attached object, 
      name of the bone and relative transformation matrix), using the current pose
      position of the armature
  """
  print("Export socket: object '%s' attached to bone '%s'"%(socketObject.name,self.name))

  # Get local transformation of the socket
  pos, rot = self.GetSocketMatrix(socketObject, armature)

  # Get armature's scale
  scale = armature.scale

  # Write socket description
  func(' '*depth + '<socket name="%s" bone="%s">'%(socketObject.name, self.name))  
  func(' '*depth + '  <transform>')
  func(' '*depth + '    <vector x="%s" y="%s" z="%s" />' % (scale[0]*pos[0], scale[2]*pos[2], scale[1]*pos[1]))
  func(' '*depth + '    <matrix>')
  if rot[0]:
    func(' '*depth + '      <rotx>%s</rotx>'%(rot[0]))
  if rot[1]:
    func(' '*depth + '      <rotz>%s</rotz>'%(rot[1]))
  if rot[2]:
    func(' '*depth + '      <roty>%s</roty>'%(-rot[2]))
  func(' '*depth + '    </matrix>')
  func(' '*depth + '  </transform>')
  func(' '*depth + '</socket>')

bpy.types.Bone.AsCSSocket = AsCSSocket
