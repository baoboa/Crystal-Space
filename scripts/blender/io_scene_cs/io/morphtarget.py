from .renderbuffer import *
from io_scene_cs.utilities import B2CS


class MorphTarget:
  def __init__(self, name, offsets, numVertices, startIndex=0):
    self.name = name
    self.offsets = offsets
    self.numVertices = numVertices
    self.startIndex = startIndex
  
  class OffsetBuffer(RenderBuffer):
    _name_ = "offset"
    _components_ = 3
    _type_ = "float"

    def __init__(self, offsets, numVertices, startIndex):
      RenderBuffer.__init__(self,None)

      for i in range(startIndex):
        RenderBuffer.AddElement(self,(0.0,0.0,0.0))

      for i in range(len(offsets)):
        RenderBuffer.AddElement(self,tuple(offsets[i]))

      for i in range(startIndex + len(offsets), numVertices):
        RenderBuffer.AddElement(self,(0.0,0.0,0.0))


    def Iter(self):
      epsilon = 1e-8
      for offset in self.buffer:
        if abs(offset[0])<epsilon and abs(offset[1])<epsilon and abs(offset[2])<epsilon:
          yield '<e c0="%.1f" c2="%.1f" c1="%.1f" />' % (0.0,0.0,0.0)
        else:
          yield '<e c0="%.8f" c2="%.8f" c1="%.8f" />' % offset

    def SubBeginTag(self):
      return '<offsets type="%s" components="%d">' % (self._type_, self._components_)

    def SubEndTag(self):
      return '</offsets>'


  def GetOffsetBuffer(self):
    return MorphTarget.OffsetBuffer(self.offsets)

  def AsCS(self, func, depth=0):
    func(' '*depth +'<morphtarget name="%s">'%(self.name))
    MorphTarget.OffsetBuffer(self.offsets,self.numVertices,self.startIndex).AsCS(func, depth+2, True)
    func(' '*depth +'</morphtarget>')


#===== static method GetMorphTargets ==============================

def GetMorphTargets(numVertices, **kwargs):
  """ Generate a list of MorphTarget objects for the meshes 
      passed in kwargs
  """

  # Recover buffers from kwargs
  meshData = kwargs.get('meshData', [])
  mappingVertices = []
  if 'mappingVertices' in kwargs:
    mappingVertices = kwargs['mappingVertices']
  else:
    print("ERROR: GetMorphTargets - mapping buffers are not defined")
  scales = kwargs.get('scales', [])

  # Generate an offset buffer for each morph target defined 
  # on the mesh object
  epsilon = 1e-8
  shapes = []
  morphTargets = []
  startIndex = 0
  # For each mesh composing the object
  for index,ob in enumerate(meshData):
    if ob.data.shape_keys:
      mtCount = 0
      scale = scales[index]
      # Get the original coordinates of mesh vertices
      originalCo = [tuple(v.co) for v in ob.data.vertices]
      # For each morph target defined on this mesh
      for j,block in enumerate(ob.data.shape_keys.key_blocks):
        # Skip the 'Basis' morph target which corresponds to the reference pose
        # (i.e. the 'rest' pose)
        if j==0 and block.name=="Basis":
          continue
        # Create an offset buffer for the new morph target
        if block.name not in shapes:
          shapes.append(block.name)
          # Get the blended coordinates of mesh vertices
          if ob.parent and ob.parent.type == 'ARMATURE' and ob.parent_type != 'BONE':
            blendedCo = [tuple(ob.relative_matrix * v.co) for v in block.data]
          else:
            blendedCo = [tuple(v.co) for v in block.data]
          nonNullOffset = False
          # Calculate the offset of each vertex
          offsets = []
          for v in mappingVertices[index]:
            offset = []
            for j in range(3):
              offset.append(scale[j]*(blendedCo[v][j]-originalCo[v][j]))
              if abs(offset[j])>epsilon:
                nonNullOffset = True
            offsets.append(offset)
            # Duplicate vertex offset if the mesh is double sided
            # and 'double sided' option is enable
            if B2CS.properties.enableDoublesided and ob.data.show_double_sided:
              offsets.append(offset)              
          # Add the morph target to the list if it contains non null offsets
          if nonNullOffset:
            mt = MorphTarget(block.name, offsets, numVertices, startIndex)
            morphTargets.append(mt)
            mtCount += 1
          else:
            print('WARNING: morph target "%s" not exported (all offsets are null)'%(block.name))
      if mtCount:
        print('INFO: %s morph target(s) exported for mesh "%s"'%(mtCount,ob.name[:-4]))
    startIndex += len(mappingVertices[index])

  return morphTargets
