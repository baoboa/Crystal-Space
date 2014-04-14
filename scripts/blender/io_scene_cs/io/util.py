import bpy

import os
import shutil


def EmptyDependencies():
  # 'T'  -> textures
  # 'M'  -> materials
  # 'TM' -> texture materials (created for meshes with UV texture but no material)
  # 'A'  -> armatures
  # 'F'  -> meshes
  # 'G'  -> groups of objects
  return {'T':{}, 'M':{}, 'TM':{}, 'A':{}, 'F':{}, 'G':{}, }

  
def MergeDependencies(dep1, dep2):
  for typ in dep1:
    dep1[typ].update(dep2[typ])


def Join(*args):
  path =  os.path.join(*args)
  return path.replace('\\', '/')
  
UNIQUE_NAMES = {'None': '', }
def GetUniqueName(obj):
  library = obj.library.filepath if obj.library else None
  if str(library) not in UNIQUE_NAMES:
    UNIQUE_NAMES[str(library)] = str(len(UNIQUE_NAMES))+'-'
  return bpy.path.clean_name(UNIQUE_NAMES[str(library)] + obj.name)

UNIQUE_FILENAMES = {}
def GetUniqueFileName(obj):
  path, fileName = os.path.split(bpy.path.abspath(obj.filepath))
  if fileName not in UNIQUE_FILENAMES:
    UNIQUE_FILENAMES[fileName] = {path:'',}
  elif path not in UNIQUE_FILENAMES[fileName]:
      UNIQUE_FILENAMES[fileName][path] = str(len(UNIQUE_FILENAMES[fileName]))+'-'
  return UNIQUE_FILENAMES[fileName][path] + fileName

#Absolute path is unique.
bpy.types.Image.uname = property(GetUniqueName)
bpy.types.Image.ufilename = property(GetUniqueFileName)
bpy.types.Material.uname = property(GetUniqueName)
bpy.types.Mesh.uname = property(GetUniqueName)
bpy.types.Object.uname = property(GetUniqueName)  
bpy.types.Group.uname = property(GetUniqueName)
bpy.types.Scene.uname = property(GetUniqueName)

# Some support code
def GetTextures(self):
  return [t.texture for t in self.texture_slots if t and t.texture]

bpy.types.Material.textures = property(GetTextures)

def GetMaterials(self):
  return [m.material for m in self.material_slots if m and m.material]

bpy.types.Mesh.materials = property(GetMaterials)
bpy.types.Object.materials = property(GetMaterials)

def SaveImage(self, path):
  dst = Join(path, 'textures/', self.ufilename)
  print("INFO: saving image", self.name, "to", dst, "...")
  try:
    if self.packed_file:
      self.save_render(dst)
    else:
      library = os.path.split(bpy.path.abspath(self.library.filepath))[0] if self.library else None
      if library != None and self.filepath.startswith('//'):
        src = os.path.join(library, self.filepath[2:])
      else:
        src = bpy.path.abspath(self.filepath) 
      shutil.copyfile(src, dst)
    print("INFO: Done.")
    return True
  except IOError:
    print("WARNING: couldn't copy image %s!"%(src))
  return False

bpy.types.Image.save_export = SaveImage

def AbsPathImage(self):
  return os.path.normpath(bpy.path.abspath(self.filepath))

bpy.types.Image.absfilepath = property(AbsPathImage)

# Utility method to search in a list
def find (f, seq):
  ''' Return index of first item in sequence where f(item) == True. '''
  for i,item in enumerate(seq):
    if f(item): 
      return i

# Utility method to decompose a matrix
def DecomposeMatrix (matrix):
  ''' Return the location, rotation and scale parts of a transformation matrix '''
  m = matrix.to_4x4()
  loc, quat, scale = m.decompose()
  # transform quaternion to a 4x4 rotation matrix
  rot = quat.to_matrix().to_4x4()
  return loc, rot, scale


# Utility methods ensuring compatibility with new API of Blender 2.63
# Blender 2.62                 ->  Blender 2.63
# bpy.types.Mesh.faces         ->  bpy.types.Mesh.tessfaces
# bpy.types.Mesh.uv_textures   ->  bpy.types.Mesh.tessface_uv_textures
# bpy.types.Mesh.vertex_colors ->  bpy.types.Mesh.tessface_vertex_colors

# Tessellation
def UpdateFaces (self):

  ve = bpy.app.version

  if (ve[0] > 2) or ((ve[0] == 2) and (ve[1] >= 63)):
    self.update(calc_tessface=True)

bpy.types.Mesh.update_faces = UpdateFaces

# Faces
def GetFaces(mesh):
    
  ve = bpy.app.version

  if (ve[0] > 2) or ((ve[0] == 2) and (ve[1] >= 63)):
    if len(mesh.tessfaces) > 0:
      return mesh.tessfaces

    return mesh.polygons

  return mesh.faces

bpy.types.Mesh.all_faces = property(GetFaces)

# UV textures
def GetUVTextures(mesh):
    
  ve = bpy.app.version

  if (ve[0] > 2) or ((ve[0] == 2) and (ve[1] >= 63)):
    if len(mesh.tessface_uv_textures) > 0:
      return mesh.tessface_uv_textures

  return mesh.uv_textures

bpy.types.Mesh.all_uv_textures = property(GetUVTextures)

# Vertex colors
def GetVertexColors(mesh):
    
  ve = bpy.app.version

  if (ve[0] > 2) or ((ve[0] == 2) and (ve[1] >= 63)):
    if len(mesh.tessface_vertex_colors) > 0:
      return mesh.tessface_vertex_colors

  return mesh.vertex_colors

bpy.types.Mesh.all_vertex_colors = property(GetVertexColors)
