__all__ = ["ui", "oi", "utilities"]

bl_info = {
    "name": "Export Crystal Space 3D format",
    "description": "Export meshes, scenes and animations",
    "author": "The Crystal Space team",
    "version": (2, 1),
    "blender": (2, 5, 9),
    "api": 39685,
    "location": "The main panel is in 'Properties > Render'",
    "warning": "",
    "wiki_url": "http://www.crystalspace3d.org/docs/online/manual/Blender.html",
    "tracker_url": "http://crystalspace3d.org/trac/CS/report",
    "category": "Import-Export"}

import bpy

class ExportCS(bpy.types.Operator):
    bl_idname = "export.cs"
    bl_label = 'Export CS'

    def execute(self, context):
        #TODO: let the user specify a file path
        return {'FINISHED'}
        
def menu_func(self, context):
    self.layout.operator(ExportCS.bl_idname, text="CrystalSpace 3D")

def register():
    bpy.utils.register_module(__name__)
    #bpy.types.INFO_MT_file_export.append(menu_func)

    if "io_scene_cs.settings" not in bpy.data.texts:
        bpy.data.texts.new("io_scene_cs.settings")
    if "io_scene_cs.utilities" not in bpy.data.texts:
        bpy.data.texts.new("io_scene_cs.utilities")

    from . import utilities
    from . import ui
    from . import io

def unregister():
    bpy.utils.unregister_module(__name__)
    #bpy.types.INFO_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()
