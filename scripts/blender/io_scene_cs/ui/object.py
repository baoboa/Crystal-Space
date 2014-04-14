import bpy

from io_scene_cs.utilities import rnaType, rnaOperator, B2CS

from io_scene_cs.utilities import HasSetProperty, RemoveSetPropertySet 

from io_scene_cs.utilities import RemovePanels, RestorePanels 

    
class csObjectPanel():
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "object"
  b2cs_context = "object"
  bl_label = ""
  REMOVED = []

  @classmethod
  def poll(cls, context): 
    ob = bpy.context.active_object
    #r = (ob and ob.type == 'MESH' and ob.data)
    r = ob
    if r:
      csObjectPanel.REMOVED = RemovePanels("object", ["OBJECT_PT_relations", "OBJECT_PT_transform"])
    else:
      RestorePanels(csObjectPanel.REMOVED)
      csObjectPanel.REMOVED = []
    return r


@rnaOperator
class OBJECT_OT_csObject_RemoveProperty(bpy.types.Operator):
  bl_idname = "csObject_RemoveProperty"
  bl_label = ""

  def invoke(self, context, event):
    ob = bpy.context.active_object
    RemoveSetPropertySet(ob, self.properties.prop)
    return('FINISHED',)


@rnaType
class OBJECT_PT_csObject(csObjectPanel, bpy.types.Panel):
  bl_label = "Crystal Space Mesh Object"
  bl_options = {'HIDE_HEADER'}

  def LayoutAddProperty(self, row, ob, name):
    split = row.split(percentage=0.5)
    colL = split.column()
    colR = split.column()
  
    colL.prop(ob, name)
    
    if ob.data and hasattr(ob.data, name):
      if not HasSetProperty(ob, name):
        if HasSetProperty(ob.data, name):
          colR.label(text="(factory: '%s')"%getattr(ob.data, name))
        else:
          colR.label(text="(default: '%s')"%getattr(ob.data, name))
      else:
        d = colR.operator("csObject_RemoveProperty", text="Default")
        d.prop = name
