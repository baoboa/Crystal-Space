import bpy

from io_scene_cs.utilities import rnaType, rnaOperator, B2CS, BoolProperty, StringProperty

from io_scene_cs.utilities import HasSetProperty, RemoveSetPropertySet 

from io_scene_cs.utilities import RemovePanels, RestorePanels 


class csPortalPanel():
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "data"
  b2cs_context = "data"
  bl_label = ""
  REMOVED = []

  @classmethod
  def poll(cls, context):
    ob = bpy.context.active_object
    r = (ob and ob.type == 'MESH' and ob.data)
    if r:
      csPortalPanel.REMOVED = RemovePanels("data", ["DATA_PT_uv_texture", "DATA_PT_vertex_colors", "DATA_PT_vertex_groups"])
    else:
      RestorePanels(csPortalPanel.REMOVED)
      csPortalPanel.REMOVED = []
    return r


@rnaOperator
class MESH_OT_csPortal_RemoveProperty(bpy.types.Operator):
  bl_idname = "csPortal_RemoveProperty"
  bl_label = ""

  def invoke(self, context, event):
    ob = bpy.context.active_object.data
    RemoveSetPropertySet(ob, self.properties.prop)
    return('FINISHED',)
        

@rnaType
class MESH_PT_csPortal(csPortalPanel, bpy.types.Panel):
  bl_label = "Crystal Space Portal"

  def draw(self, context):    
    ob = context.active_object

    if ob.type == 'MESH':
      # Draw a checkbox to define current mesh object as a portal
      layout = self.layout
      row = layout.row()
      row.prop(ob.data, "portal")

      if ob.data.portal:
          # Get the user defined name of the target mesh
          row = layout.row()
          split = row.split(percentage=0.5)
          colL = split.column()
          colL.label(text='Destination mesh:', icon='MESH_DATA')
          colR = split.column()
          colR.prop(ob.data, "destinationPortal", text='')

          # Verify that the target mesh name corresponds 
          # to a mesh object of the world
          source = ob.data
          target = source.destinationPortal
          if target:
              targetIsValid = False
              for o in bpy.data.objects:
                  if o.name == target and o.type=='MESH':
                      targetIsValid = True
                      break
              # Write an error message if the given name is not valid
              if not targetIsValid:
                  row = colR.row()
                  row.label(text="ERROR: unknown mesh name")


BoolProperty(['Mesh'], 
             attr="portal", 
             name="Portal", 
             description="Whether or not this mesh is defined as a CS portal", 
             default=False)

StringProperty(['Mesh'], 
               attr="destinationPortal", 
               name="Destination portal", 
               description="Name of the destination mesh for this portal",
               default='')
