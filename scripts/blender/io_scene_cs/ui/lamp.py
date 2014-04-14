import bpy

from io_scene_cs.utilities import rnaType, rnaOperator, B2CS, BoolProperty

from io_scene_cs.utilities import HasSetProperty, RemoveSetPropertySet 

from io_scene_cs.utilities import RemovePanels, RestorePanels 


class csLampPanel():
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "data"
  b2cs_context = "data"
  bl_label = ""
  REMOVED = []

  @classmethod
  def poll(cls, context):
    ob = bpy.context.active_object
    r = (ob and ob.type == 'LAMP' and ob.data)
    if r:
      csLampPanel.REMOVED = RemovePanels("data", ["DATA_PT_shadow"])
    else:
      RestorePanels(csLampPanel.REMOVED)
      csLampPanel.REMOVED = []
    return r

@rnaOperator
class MESH_OT_csLamp_RemoveProperty(bpy.types.Operator):
  bl_idname = "csLamp_RemoveProperty"
  bl_label = ""

  def invoke(self, context, event):
    ob = bpy.context.active_object.data
    RemoveSetPropertySet(ob, self.properties.prop)
    return('FINISHED',)
        

@rnaType
class MESH_PT_csLamp(csLampPanel,bpy.types.Panel):
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "data"
  b2cs_context = "data"
  bl_label = "Crystal Space Properties"

  def draw(self, context):
    layout = self.layout
    
    ob = bpy.context.active_object
    
    if ob.type == 'LAMP':
      ob = bpy.context.active_object.data
      row = layout.row()
      row.prop(ob, "no_shadows")
      

BoolProperty(['Lamp'], 
     attr="no_shadows", 
     name="No shadows", 
     description="Whether or not this lamp can cast shadows", 
     default=False)
