import bpy

from .util import *


def ObjectRelativeMatrix(self):
  return self.matrix_local
  '''
  mat = self.matrix
  if self.parent:
    mat = mat - self.parent.relative_matrix
  return mat
  '''

bpy.types.Object.relative_matrix = property(ObjectRelativeMatrix)


def MatrixAsCS(matrix, func, depth=0, noMove=False):
  ''' unique() is not defined in the new mathutils API '''
  rot=matrix.to_euler()
  if not noMove:
    func(' '*depth +'<move>')
  # Flip Y and Z axis.
  func(' '*depth +'  <v x="%s" z="%s" y="%s" />' % tuple(matrix.to_translation()))
  if rot[0] or rot[1] or rot[2]:
    func(' '*depth +'  <matrix>')
    # Order 'YZX' is important!
    if rot[2]:
      func(' '*depth +'    <roty>%s</roty>'%(rot[2]))
    if rot[1]:
      func(' '*depth +'    <rotz>%s</rotz>'%(-rot[1]))
    if rot[0]:
      func(' '*depth +'    <rotx>%s</rotx>'%(-rot[0]))
    func(' '*depth +'  </matrix>')
  
  if not noMove:
    func(' '*depth +'</move>')
