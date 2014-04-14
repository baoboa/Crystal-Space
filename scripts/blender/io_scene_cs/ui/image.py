import bpy

from io_scene_cs.utilities import rnaType, rnaOperator, B2CS, BoolProperty

from io_scene_cs.utilities import RemovePanels, RestorePanels 

def active_node_mat(mat):
  if mat is not None:
      mat_node = mat.active_node_material
      if mat_node:
          return mat_node
      else:
          return mat

  return None

def context_tex_datablock(context):
  idblock = context.material
  if idblock:
      return active_node_mat(idblock)

  idblock = context.lamp
  if idblock:
      return idblock

  idblock = context.world
  if idblock:
      return idblock

  idblock = context.brush
  return idblock

class csTexturePanel():
  bl_space_type = 'PROPERTIES'
  bl_region_type = 'WINDOW'
  bl_context = "texture"
  b2cs_context = "texture"
  REMOVED = []
  
  @classmethod
  def poll(cls, context):
    tex = context.texture
    r = (tex and tex.type == 'IMAGE')
    if r:
      csTexturePanel.REMOVED = RemovePanels("texture", ["TEXTURE_PT_preview", "TEXTURE_PT_context_texture", "TEXTURE_PT_image1"])
    else:
      RestorePanels(csTexturePanel.REMOVED)
      csTexturePanel.REMOVED = []
    return r    


@rnaType
class TEXTURE_PT_B2CS_image(csTexturePanel, bpy.types.Panel):
  bl_label = "Image"

  def draw(self, context):
    layout = self.layout

    tex = context.texture

    #layout.template_image(tex, "image", tex.image_user, True)
    
    row = layout.row(align=True)
    row.template_ID(tex, "image", new='image.new', open='image.open', unlink='image.unlink')
    row = layout.row(align=True)
    row.prop(tex.image, "filepath")
    row.operator("image.reload", icon='FILE_REFRESH', text='')


            
@rnaType
class TEXTURE_PT_B2CS_texture(csTexturePanel, bpy.types.Panel):
  bl_label = "Crystal Space Texture"


  def draw(self, context):
    layout = self.layout
    
    tex = context.texture_slot
    
    idblock = context_tex_datablock(context)
    
    if type(idblock) == bpy.types.Material:
      if idblock.type in ('SURFACE', 'HALO', 'WIRE'):
      
        props = ['use_map_color_diffuse', 'use_map_normal', 'use_map_displacement', 'use_map_specular',]
        
        layout.label(text="Usage:")
        for prop in props:
          text = prop.split('_')
          row = layout.row(align=True)
          row.prop(tex, prop, text=text[-1])
          row.active = not any([getattr(tex, p) for p in props if p != prop])
          row.enabled = row.active
    
        layout.label(text="Properties:")
        row = layout.row(align=True)
        if tex.texture.image:
          row.prop(tex.texture.image, "binAlpha")



BoolProperty(['Image'], attr="binAlpha", name="Binary Alpha", description="Use binary alpha for this texture")
