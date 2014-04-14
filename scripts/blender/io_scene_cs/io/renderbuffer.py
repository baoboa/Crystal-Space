from io_scene_cs.utilities import B2CS


class RenderBuffer:
  def __init__(self, mesh, **kwargs):
    self.me = mesh
    self.kwargs = kwargs
    self.buffer = []

  def __iter__(self):
        return self.Iter()

  def __repr__(self):
        return self._name_

  def AddElement(self, element):
    self.buffer.append(element)

  def IsEmpty(self):
    return len(self.buffer) == 0

  def BeginTag(self, animesh):
    if animesh:
      return self.SubBeginTag()
    else:
      return '<renderbuffer name="%s" checkelementcount="no" components="%d" type="%s">' % (self._name_, self._components_, self._type_)

  def EndTag(self, animesh):
    if animesh:
      return self.SubEndTag()
    else:
      return '</renderbuffer>'

  def AsCS(self, func, depth=0, animesh=False):
    func(' '*depth + self.BeginTag(animesh))
    for data in self:
      func(' '*depth + '  ' + data)
    func(' '*depth + self.EndTag(animesh))


class PositionBuffer(RenderBuffer):
  _name_ = "position"
  _components_ = 3
  _type_ = "float"

  def Iter(self):
    for pos in self.buffer:
      yield '<e c0="%.8f" c2="%.8f" c1="%.8f" />' % pos

  def SubBeginTag(self):
    return '<vertex type="%s" components="%d">' % (self._type_, self._components_)

  def SubEndTag(self):
    return '</vertex>'


class NormalBuffer(RenderBuffer):
  _name_ = "normal"
  _components_ = 3
  _type_ = "float"

  def Iter(self):
    for normal in self.buffer:
      yield '<e c0="%.8f" c2="%.8f" c1="%.8f" />' % normal

  def SubBeginTag(self):
    return '<normal type="%s" components="%d">' % (self._type_, self._components_)

  def SubEndTag(self):
    return '</normal>'


class TCBuffer(RenderBuffer):
  _name_ = "texture coordinate"
  _components_ = 2
  _type_ = "float"

  def Iter(self):
    for coord in self.buffer:
      yield '<e c0="%.6f" c1="%.6f" />' % coord

  def SubBeginTag(self):
    return '<texcoord type="%s" components="%d">' % (self._type_, self._components_)

  def SubEndTag(self):
    return '</texcoord>'


class ColorBuffer(RenderBuffer):
  _name_ = "color"
  _components_ = 4
  _type_ = "float"

  def Iter(self):
    for color in self.buffer:
      yield '<e c0="%.6f" c1="%.6f" c2="%.6f" c3="1" />' % color

  def SubBeginTag(self):
    return '<color type="%s" components="%d">' % (self._type_, self._components_)

  def SubEndTag(self):
    return '</color>'


#===== class RenderBuffers =========================================

class RenderBuffers:
  def __init__(self, **kwargs):
    self.meshData = kwargs.get('meshData', [])
    self.mappingBuffers = []
    if 'mappingBuffers' in kwargs:
      self.mappingBuffers = kwargs['mappingBuffers']
    else:
      print("ERROR: mapping buffers are not defined to build the render buffers")
    self.mappingVertices = kwargs.get('mappingVertices', [])
    self.mappingNormals = kwargs.get('mappingNormals', [])
    self.scales = kwargs.get('scales', [])

    self.positionBuffer = PositionBuffer(None, **kwargs)
    self.normalBuffer = NormalBuffer(None, **kwargs)
    self.texCoordBuffer = TCBuffer(None, **kwargs)
    self.colorBuffer = ColorBuffer(None, **kwargs)


  def GenerateBuffers(self):
    """ Build CS render buffers by using the Blender/CS mapping buffers
        defined in this RenderBuffer object
    """
    # For each mesh composing the object
    for i, ob in enumerate(self.meshData):
      doubleSided = B2CS.properties.enableDoublesided and ob.data.show_double_sided

      # Get the active UV texture and vertex colors
      tface = ob.data.all_uv_textures.active.data if ob.data.all_uv_textures.active else None
      cface = ob.data.all_vertex_colors.active.data if ob.data.all_vertex_colors.active else None

      # Get object's scale
      if self.scales:
        scale = self.scales[i]
      else:
        scale = ob.scale

      # For each vertex of this mesh
      for indexCS, indexBlender in enumerate(self.mappingVertices[i]):
        # Add the vertex coordinates to the CS position buffer
        pos = ob.data.vertices[indexBlender].co
        self.positionBuffer.AddElement(tuple(map(lambda j: scale[j]*pos[j], range(3))))
        if doubleSided:
          # Duplicate the vertex coordinates in case of a double sided mesh
          self.positionBuffer.AddElement(tuple(map(lambda j: scale[j]*pos[j], range(3))))

        # Add the vertex normal to the CS normal buffer
        # Flip Y and Z axis.
        self.normalBuffer.AddElement(tuple(self.mappingNormals[i][indexCS]))
        if doubleSided:
          # Duplicate and inverse the vertex normal in case of a double sided mesh
          nor = self.mappingNormals[i][indexCS]
          self.normalBuffer.AddElement((-nor[0],-nor[1],-nor[2]))
        
        def findFace(csVertices, csIndex):
          ''' Find the face containing this vertex '''
          for mapV in csVertices:
            if mapV['csVertex'] == csIndex:
              return mapV['face'], mapV['vertex']
          return -1, -1

        face, vi = findFace(self.mappingBuffers[i][indexBlender],indexCS)
        
        # Add uv coordinates of this vertex to the CS texture coordinates buffer
        if tface and face != -1:
          uv = tface[face].uv[vi]
          uvco = (uv[0],1.0-uv[1])
        else:
          uvco = (0.0,0.0)
        self.texCoordBuffer.AddElement(uvco)
        if doubleSided:
          # Duplicate the uv coordinates in case of a double sided mesh
          self.texCoordBuffer.AddElement(uvco)

        # Add the vertex color to the CS color buffer
        if cface and face != -1:
          colors = [cface[face].color1, cface[face].color2, cface[face].color3, cface[face].color4]
          col = tuple(colors[vi])
        else:
          col = (1.0,1.0,1.0)
        self.colorBuffer.AddElement(col)
        if doubleSided:
          # Duplicate the color in case of a double sided mesh
          self.colorBuffer.AddElement(col)

 
#===== static method GetRenderBuffers ==============================

def GetRenderBuffers(**kwargs):
  """ Generate render buffers (position, normal, texture coordinates and color 
      for each CS vertex) from mesh data and mapping buffers passed in kwargs
  """
  buffers = RenderBuffers(**kwargs)
  buffers.GenerateBuffers()

  if not buffers.positionBuffer.IsEmpty():
    yield buffers.positionBuffer

  if not buffers.normalBuffer.IsEmpty():
    yield buffers.normalBuffer

  if not buffers.texCoordBuffer.IsEmpty():
    yield buffers.texCoordBuffer

  if not buffers.colorBuffer.IsEmpty():
    yield buffers.colorBuffer

