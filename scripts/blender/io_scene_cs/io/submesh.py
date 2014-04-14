from .util import *
from .renderbuffer import *


class SubMesh:
  def __init__(self, name, material, image, indices):
    self.name = name
    self.material = material
    self.image = image
    self.indices = indices

  class IndexBuffer(RenderBuffer):
    _name_ = "indices"
    _components_ = 1
    _type_ = "uint"

    def __init__(self, indices):
      self.indices = indices

    def Iter(self):
      for i, index in enumerate(self.indices):
        if i%3 == 0:
          if i!=0:
            yield faceIndices
          faceIndices = '<e c0="%d" />' % (index)
        else:
          faceIndices += ' <e c0="%d" />' % (index)
          if i==len(self.indices)-1:
            yield faceIndices

    def BeginTag(self, animesh):
      if animesh:
        return '<index type="%s" components="%d" indices="yes">' % (self._type_, self._components_)
      else:
        return '<indexbuffer indices="yes" checkelementcount="no" components="%d" type="%s">' % (self._components_, self._type_)

    def EndTag(self, animesh):
      if animesh:
        return '</index>'
      else:
        return '</indexbuffer>'

  def GetIndexBuffer(self):
    return SubMesh.IndexBuffer(self.indices)

  def AsCS(self, func, depth=0, animesh=False):
    if self.name:
      if self.material:
        self.name = '%s-%s'%(self.name,self.material.uname)
      if self.image:
        self.name = '%s-%s'%(self.name,self.image.uname)        
      func(' '*depth +'<submesh name="%s">'%(self.name))
    else:
      func(' '*depth +'<submesh>')

    if self.material:
      func(' '*depth +'  <material>'+self.material.uname+'</material>')
      if not self.material.HasDiffuseTexture() and self.material.uv_texture != 'None':
        func(' '*depth +'  <shadervar type="texture" name="tex diffuse">%s</shadervar>'%(self.material.uv_texture))
    elif self.image:
      func(' '*depth +'  <material>'+self.image.uname+'</material>')

    if self.material:
      if self.material.priority != 'object':
        func(' '*depth + '  <priority>%s</priority>'%(self.material.priority))
      if self.material.zbuf_mode != 'zuse':
        func(' '*depth + '  <%s/>'%(self.material.zbuf_mode))

    SubMesh.IndexBuffer(self.indices).AsCS(func, depth+2, animesh)
    func(' '*depth +'</submesh>')

  def AsCSTriangles(self, func, depth=0, animesh=False):
    text = ''
    for i, index in enumerate(self.indices):
      text += '%d ' % (index)
    func(' '*depth +'<triangles>'+text+'</triangles>')

  def GetDependencies(self):
    dependencies = EmptyDependencies()
    if self.material:
      MergeDependencies(dependencies, self.material.GetDependencies())
    if self.image:
      dependencies['T'][self.image.uname] = self.image
    return dependencies
