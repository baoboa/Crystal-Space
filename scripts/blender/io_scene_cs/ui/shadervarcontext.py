import bpy

from io_scene_cs.utilities import rnaType, rnaOperator

from io_scene_cs.utilities import HasSetProperty, RemoveSetPropertySet 

from io_scene_cs.utilities import RemovePanels, RestorePanels 

    
class csShaderVarContextPanel():
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"
    bl_label = "Crystal Space Shader"
    bl_label = ""

    @classmethod
    def poll(cls, context): 
        ob = bpy.context.active_object
        return (ob and ob.type == 'MESH' and ob.data and ob.data.cstype != "none")



@rnaOperator
class OBJECT_PT_csShaderVarContext_RemoveProperty(bpy.types.Operator):
    bl_idname = "csShaderVarContext_RemoveProperty"
    bl_label = ""

    def invoke(self, context, event):
        ob = bpy.context.active_object
        RemoveSetPropertySet(ob, self.properties.prop)
        return('FINISHED',)


@rnaType
class OBJECT_PT_csShaderVarContext(csShaderVarContextPanel, bpy.types.Panel):
    bl_label = "Crystal Space Shader"

    def draw(self, context):
        layout = self.layout
        
        ob = bpy.context.active_object

