try:
  import bpy
except:
  bpy = None

def rnaType(rna_type):
    if bpy: bpy.utils.register_class(rna_type)
    return rna_type


def rnaOperator(rna_op):
    #if bpy: bpy.types.register(rna_op)
    return rna_op

    
def HasSetProperty(ob, name):
  if isinstance(ob, bpy.types.Object):
     return bpy.types.Object.is_property_set(ob, name)
  elif isinstance(ob, bpy.types.Mesh):
     return bpy.types.Mesh.is_property_set(ob, name)
  return False


def RemoveSetPropertySet(ob, name):
  if isinstance(ob, bpy.types.Object):
     bpy.types.Object.__delitem__(ob, name)
  elif isinstance(ob, bpy.types.Mesh):
     bpy.types.Mesh.__delitem__(ob, name)


def RemovePanels(bl_context, exceptions=[], removed=[]):
  '''
  for typeName in dir(bpy.types):
    type = getattr(bpy.types, typeName)
    if hasattr(type, "bl_context") and type.bl_context == bl_context:
      if hasattr(type, "bl_space_type") and type.bl_space_type == "PROPERTIES":
        if hasattr(type, "bl_region_type") and type.bl_region_type == "WINDOW":
          if not hasattr(type, "b2cs_context"):
            if typeName not in exceptions:
              print("Removing: ", type.__module__, type.__name__)
              removed.append([type.__module__, type])
              #bpy.types.unregister(type)
              bpy.utils.unregister_class(type)
  '''
  return removed
  

def RestorePanels(removed):
  for m, c in removed:
    if not hasattr(bpy.types, str(c.__name__)):
      print("Restoring: ", m, c.__name__)
      c = getattr(__import__(m), c.__name__)
      #bpy.types.register(c)
      bpy.utils.register_class(c)

  
def Property(typ, types, **kwargs):
  attr = kwargs['attr']
  del kwargs['attr']
  kwargs['options'] = {'HIDDEN'}
  #print('I: Added property %s of type %s to %s.'%(attr, typ, str(types)))
  for klass in types:
    p = getattr(bpy.props, typ)
    t = getattr(bpy.types, klass)
    setattr(t, attr, p(**kwargs))


def BoolProperty(types, **kwargs):
  Property('BoolProperty', types, **kwargs)   

def StringProperty(types, **kwargs):
  Property('StringProperty', types, **kwargs)  

def EnumProperty(types, **kwargs):
  Property('EnumProperty', types, **kwargs)  
        
class B2CS:
  def B2CSGetProperties(self):
    if "io_scene_cs.settings" not in bpy.data.texts:
      text = bpy.data.texts.new("io_scene_cs.settings")
    else:
      text = bpy.data.texts["io_scene_cs.settings"]
    return text
    
  def BoolProperty(self, **kwargs):
    BoolProperty(['Text'], **kwargs)
    
  def StringProperty(self, **kwargs):
    StringProperty(['Text'], **kwargs)
    
  properties = property(B2CSGetProperties) 

B2CS = B2CS()

SHADERS =(("DEFAULT", "Default", "Default"),
          ("*null", "*null", "Shader with no effect."),
          ("/shader/lighting/lighting_default_binalpha.xml", "lighting_default_binalpha", "Use when using a texture with binary alpha."),) 

def GetShaderName(f):
  i = [l[0] for l in SHADERS].index(f)
  if i >=0:
    return SHADERS[i][1]
  return None
    
bpy.types.OperatorProperties.prop = bpy.props.StringProperty(name="operator argument", description="")
