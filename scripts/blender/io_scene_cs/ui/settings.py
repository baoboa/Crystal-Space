import os

import bpy

from io_scene_cs.utilities import rnaType, rnaOperator, B2CS
from io_scene_cs.utilities import RemovePanels, RestorePanels 
from io_scene_cs.io import Export


class csSettingsPanel():
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "render"
  b2cs_context = "render"
  bl_label = ""
  REMOVED = []

  @classmethod
  def poll(cls, context):
    return True
 
   
@rnaType    
class B2CS_OT_export(bpy.types.Operator):
  "Export"
  bl_idname = "io_scene_cs.export"
  bl_label = "Export"

  def execute(self, context): 
    
    Export(B2CS.properties.exportPath)

    return {'FINISHED'}

def WalkTestPath():
  if os.name == 'nt':
    walktest = "walktest.exe"
  else:
    walktest = "walktest"

  # Look either in the 'CRYSTAL' and 'CRYSTAL/bin' paths
  path = os.path.join(os.environ.get("CRYSTAL"), walktest).replace('\\', '/')
  if os.path.exists(path):
    return path
  return os.path.join(os.environ.get("CRYSTAL"), "bin", walktest).replace('\\', '/')
            
def HasCrystalSpace():    
  return os.path.exists(WalkTestPath())


@rnaType    
class B2CS_OT_export_run(bpy.types.Operator):
  "Export and Run"
  bl_idname = "io_scene_cs.export_run"
  bl_label = "Export and Run."

  def execute(self, context): 
    
    path = B2CS.properties.exportPath.replace('\\', '/')
    
    Export(path)
    
    options = " "
    if B2CS.properties.console:
      options += "-console "
    if B2CS.properties.verbose:
      options += "-verbose=-scf "
    if B2CS.properties.silent:
      options += "-silent "
    if B2CS.properties.bugplug:
      options += "-plugin=bugplug "
     
    #import commands  
    #output = commands.getstatusoutput(WalkTestPath() + options + path)
    #print(output)
    
    import shlex, subprocess
    print(WalkTestPath())
    args = shlex.split(WalkTestPath() + options + path)
    print(args)
    output = subprocess.call(args)
    print(output)

    return {'FINISHED'}
      

@rnaType
class RENDER_PT_csSettingsPanel(csSettingsPanel, bpy.types.Panel):
  bl_label = "Crystal Space Export"
  
  def draw(self, context):
    layout = self.layout
    
    row = layout.row()
    row.prop(B2CS.properties, "library")
    row = layout.row()
    row.prop(B2CS.properties, "enableDoublesided")
    row = layout.row()
    row.prop(B2CS.properties, "exportPath")
      
    row = layout.row()
    row.operator("io_scene_cs.export", text="Export")
      
    row = layout.row()
    if not B2CS.properties.library:
      if HasCrystalSpace():
        row.operator("io_scene_cs.export_run", text="Export and Run")
        row = layout.row()
        row.prop(B2CS.properties, "console")
        row.prop(B2CS.properties, "verbose")
        row.prop(B2CS.properties, "silent")
        row.prop(B2CS.properties, "bugplug")
      else:
        row.label(text="'walktest' isn't available!")
      
        
default_path = os.environ.get("TEMP")
if not default_path:
  if os.name == 'nt':
    default_path = "c:/tmp/"
  else:
    default_path = "/tmp/"
elif not default_path.endswith(os.sep):
  default_path += os.sep
            
B2CS.StringProperty( attr="exportPath",
        name="Export path",
        description="Export path", 
        default=default_path, subtype='DIR_PATH')
        
B2CS.BoolProperty( attr="console",
        name="Console",
        description="Enable the '-console' flag of 'walktest'", 
        default=True)
        
B2CS.BoolProperty( attr="verbose",
        name="Verbose",
        description="Enable the '-verbose=-scf' flag of 'walktest'", 
        default=True)

B2CS.BoolProperty( attr="silent",
        name="Silent",
        description="Enable the '-silent' flag of 'walktest'", 
        default=True)

B2CS.BoolProperty( attr="bugplug",
        name="Bugplug",
        description="Enable bugplug in 'walktest'",
        default=False)

B2CS.BoolProperty( attr="library",
        name="Export as a CS library",
        description="Export all mesh factories in an unique CS library file", 
        default=False)

B2CS.BoolProperty( attr="enableDoublesided",
        name="Enable double sided meshes",
        description="Global enabling of the 'Double Sided' option for all meshes",
        default=False)
