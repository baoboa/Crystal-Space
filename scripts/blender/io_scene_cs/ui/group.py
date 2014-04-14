import bpy

from io_scene_cs.utilities import rnaType, rnaOperator, B2CS, BoolProperty

from io_scene_cs.utilities import HasSetProperty, RemoveSetPropertySet 

from io_scene_cs.utilities import RemovePanels, RestorePanels 

    
class csGroupPanel():
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "object"
  b2cs_context = "object"
  bl_label = ""
  REMOVED = []

  @classmethod
  def poll(cls, context): 
    ob = bpy.context.active_object
    r = (ob and ob.type == 'MESH') or (ob and ob.type == 'EMPTY')
    if r:
      csGroupPanel.REMOVED = RemovePanels("object", ["OBJECT_PT_relations", "OBJECT_PT_transform"])
    else:
      RestorePanels(csGroupPanel.REMOVED)
      csGroupPanel.REMOVED = []
    return r


@rnaOperator
class OBJECT_OT_csGroup_RemoveProperty(bpy.types.Operator):
  bl_idname = "csGroup_RemoveProperty"
  bl_label = ""

  def invoke(self, context, event):
    ob = bpy.context.active_object
    RemoveSetPropertySet(ob, self.properties.prop)
    return('FINISHED',)


@rnaType
class OBJECT_PT_B2CS_groups(csGroupPanel, bpy.types.Panel):
  bl_label = "Crystal Space Mesh Groups"
  
  def draw(self, context):
    layout = self.layout

    ob = bpy.context.active_object
    
    row = layout.row(align=True)
    row.operator("object.group_link", text="Add to Group")
    row.operator("object.group_add", text="", icon='ZOOMIN')

    index = 0
    value = str(tuple(context.scene.cursor_location))
    for group in bpy.data.groups:
      if ob.name in group.objects:
        col = layout.column(align=True)

        col.context_pointer_set("group", group)

        row = col.box().row()
        row.prop(group, "name", text="")
        row.operator("object.group_remove", text="", icon='X', emboss=False)

        split = col.box().split()

        col = split.column()
        col.prop(group, "layers", text="Dupli")

        
        row = col.row()
        row.prop(group, "doMerge")
        
        if group.doMerge:
          row = col.row()
          row.prop(group, "groupedInstances")

        col = split.column()
        col.prop(group, "dupli_offset", text="")
        
        prop = col.operator("wm.context_set_value", text="From Cursor")
        prop.data_path = "object.users_group[%d].dupli_offset" % index
        prop.value = value
        index += 1
        

BoolProperty(['Group'], attr="doMerge", name="Merge", description="Merge the objects in this group into one on export")
BoolProperty(['Group'], attr="groupedInstances", name="Grouped Instances", description="Export children as a grouped Instances")
