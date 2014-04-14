/*
  Copyright (C) 2003-2006 by Marten Svanfeldt
		2004-2006 by Frank Richter

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"
#include <ctype.h>

#include "imap/services.h"
#include "iutil/hiercache.h"
#include "iutil/plugin.h"
#include "iutil/string.h"
#include "iutil/stringarray.h"
#include "ivaria/reporter.h"
#include "ivideo/rendermesh.h"
#include "ivideo/material.h"

#include "csgfx/renderbuffer.h"
#include "csutil/csendian.h"
#include "csutil/documenthelper.h"
#include "csutil/stringarray.h"
#include "csutil/stringquote.h"

#include "shader.h"
#include "shadertech.h"
#include "xmlshader.h"
#include "shaderwrapper.h"

CS_PLUGIN_NAMESPACE_BEGIN(XMLShader)
{

CS_LEAKGUARD_IMPLEMENT (csXMLShaderTech);

/* Magic value for tech + pass cache files.
 * The most significant byte serves as a "version", increase when the
 * cache file format changes. */
static const uint32 cacheFileMagic = 0x0a747863;

//---------------------------------------------------------------------------

struct csXMLShaderTech::CachedPlugins
{
  CachedPlugin unified, vproc;
};

CS_IMPLEMENT_STATIC_CLASSVAR_REF (csXMLShaderTech, instParamsTargets,  
                                  GetInstParamsTargets, csDirtyAccessArray<csVertexAttrib>, ()); 
CS_IMPLEMENT_STATIC_CLASSVAR_REF (csXMLShaderTech, instParams, GetInstParams, 
                                  csDirtyAccessArray<csShaderVariable*>, ()); 
CS_IMPLEMENT_STATIC_CLASSVAR_REF (csXMLShaderTech, instParamPtrs,  
                                  GetInstParamPtrs, csDirtyAccessArray<csShaderVariable**>, ()); 
CS_IMPLEMENT_STATIC_CLASSVAR_REF (csXMLShaderTech, instOuterVar,  
                                  GetInstOuterVars, csArray<csShaderVariable*>, ()); 
CS_IMPLEMENT_STATIC_CLASSVAR_REF (csXMLShaderTech, instParamBuffers,  
                                  GetInstParamBuffers, csDirtyAccessArray<iRenderBuffer*>, ()); 

csXMLShaderTech::csXMLShaderTech (csXMLShader* parent) : 
passes(0), passesCount(0), currentPass((size_t)~0),
xmltokens (parent->compiler->xmltokens)
{
  csXMLShaderTech::parent = parent;

  do_verbose = parent->compiler->do_verbose;
}

csXMLShaderTech::~csXMLShaderTech()
{
  delete[] passes;
}

static inline bool IsDestalphaMixmode (uint mode)
{
  return (mode == CS_FX_DESTALPHAADD);
}

csVertexAttrib csXMLShaderTech::ParseVertexAttribute (const char* dest,
                                                      iShaderDestinationResolver* resolve)
{
  csVertexAttrib attrib = CS_VATTRIB_INVALID;
  int i;
  for(i=0;i<16;i++)
  {
    csString str;
    str.Format ("attribute %d", i);
    if (strcasecmp(str, dest)==0)
    {
      attrib = (csVertexAttrib)(CS_VATTRIB_0 + i);
      break;
    }
  }
  if (attrib == CS_VATTRIB_INVALID)
  {
    if (strcasecmp (dest, "position") == 0)
    {
      attrib = CS_VATTRIB_POSITION;
    }
    else if (strcasecmp (dest, "normal") == 0)
    {
      attrib = CS_VATTRIB_NORMAL;
    }
    else if (strcasecmp (dest, "color") == 0)
    {
      attrib = CS_VATTRIB_COLOR;
    }
    else if (strcasecmp (dest, "primary color") == 0)
    {
      attrib = CS_VATTRIB_PRIMARY_COLOR;
    }
    else if (strcasecmp (dest, "secondary color") == 0)
    {
      attrib = CS_VATTRIB_SECONDARY_COLOR;
    }
    else if (strcasecmp (dest, "texture coordinate") == 0)
    {
      attrib = CS_VATTRIB_TEXCOORD;
    }
    else if (strcasecmp (dest, "matrix object2world") == 0)
    {
      attrib = CS_IATTRIB_OBJECT2WORLD;
    }
    else
    {
      static const char mapName[] = "texture coordinate ";
      if (strncasecmp (dest, mapName, sizeof (mapName) - 1) == 0)
      {
        const char* target = dest + sizeof (mapName) - 1;

        int texUnit = resolve ? resolve->ResolveTU (target) : -1;
        if (texUnit >= 0)
        {
          attrib = (csVertexAttrib)((int)CS_VATTRIB_TEXCOORD0 + texUnit);
        }
        else
        {
          char dummy;
          if (sscanf (target, "%d%c", &texUnit, &dummy) == 1)
          {
            attrib = (csVertexAttrib)((int)CS_VATTRIB_TEXCOORD0 + texUnit);
          }
        }
      }
      else
      {
        attrib = resolve ? resolve->ResolveBufferDestination (dest) : CS_VATTRIB_INVALID;
      }
    }
  }
  return attrib;
}

struct csXMLShaderTech::LoadHelpers
{
  iSyntaxService* synldr;
  iStringSet* strings;
  iShaderVarStringSet* stringsSvName;
};

bool csXMLShaderTech::LoadPass (iDocumentNode *node, ShaderPass* pass, 
                                size_t variant, iFile* cacheFile,
                                iHierarchicalCache* cacheTo)
{
  LoadHelpers hlp;
  hlp.synldr = parent->compiler->synldr;
  hlp.strings = parent->compiler->strings;
  hlp.stringsSvName = parent->compiler->stringsSvName;

  CachedPlugins cachedPlugins;

  if (!GetProgramPlugins (node, cachedPlugins, variant))
  {
    SetFailReason ("Couldn't load shader plugin(s).");
    return false;
  }

  csRef<iShaderProgram> program;

  bool result = true;
  bool setFailReason = true;
  
  csString tagShader, tagVPr;
  
  csRef<iShaderDestinationResolver> resolve;

  {
    csRef<iHierarchicalCache> progCache;
    if (cacheTo)
      progCache = cacheTo->GetRootedCache (csString().Format ("/pass%dprog",
                                           GetPassNumber (pass)));
    // new unified syntax detected
    pass->program = LoadProgram (0, cachedPlugins.unified.programNode,
                                 pass, variant, progCache, cachedPlugins.unified, tagShader);
    if (!pass->program)
      return false;

    resolve = scfQueryInterface<iShaderDestinationResolver> (pass->program);
  }

  //load vproc
  if (cachedPlugins.vproc.programNode)
  {
    csRef<iHierarchicalCache> vprCache;
    if (cacheTo) vprCache = cacheTo->GetRootedCache (csString().Format (
      "/pass%dvpr", GetPassNumber (pass)));
    program = LoadProgram (resolve, cachedPlugins.vproc.programNode,
      pass, variant, vprCache, cachedPlugins.vproc, tagVPr);
    if (program)
    {
      pass->vproc = program;
    }
    else
    {
      if (do_verbose && setFailReason)
      {
	SetFailReason ("pass %d vertex preprocessor failed to load",
	  GetPassNumber (pass));
	setFailReason = false;
      }
      result = false;
    }
  }

  if (result)
  {
    if (!ParseModes (pass, node, hlp)) return false;

    const csGraphics3DCaps* caps = parent->g3d->GetCaps();
    if (IsDestalphaMixmode (pass->mixMode) && !caps->DestinationAlpha)
    {
      if (do_verbose)
	SetFailReason ("destination alpha not supported by renderer");
      return false;
    }
  }

  WritePass (pass, cacheFile);

  //if we got this far, load buffermappings
  int passNum (GetPassNumber (pass));
  if (result && !ParseBuffers (*pass, passNum,
    node, hlp, resolve)) result = false;

  //get texturemappings
  if (result && !ParseTextures (*pass, node, hlp, resolve)) result = false;

  // Load pseudo-instancing binds
  if (result && !ParseInstances (*pass, passNum, node, hlp, resolve))
    result = false;
  
  return result;
}

struct PassActionPrecache
{
  PassActionPrecache (int passIndex, csXMLShaderTech* tech, size_t variant,
    iHierarchicalCache* cacheTo, const char* cacheType) : passIndex (passIndex),
    tech (tech), variant (variant), cacheTo (cacheTo)
  {
    if (cacheTo)
    {
      progCache = cacheTo->GetRootedCache (csString().Format (
        "/pass%d%s", passIndex, cacheType));
    }
  }
  
  bool Perform (const char* tag,
    csXMLShaderTech::CachedPlugin& cachedPlugin)
  {
    // load fp
    /* This is done before VP loading b/c the VP could query for TU bindings
    * which are currently handled by the FP. */
    bool result = true;
    bool setFailReason = true;

    csRef<iBase> prog;
    if (cachedPlugin.programNode)
    {
      csRef<iBase>* cacheP = progTagCache.GetElementPointer (tag);
      if (!cacheP)
      {
    if (!tech->PrecacheProgram (0, cachedPlugin.programNode, variant,
        progCache, cachedPlugin, prog, tag))
    {
      if (tech->do_verbose && setFailReason)
      {
        tech->SetFailReason ("pass %d program failed to precache",
          passIndex);
        setFailReason = false;
      }
      result = false;
    }
    progTagCache.PutUnique (tag, prog);
      }
      else
    prog = *cacheP;
    }

    oneComboWorked |= result;

    return false;
  }

  bool oneComboWorked;
private:
  int passIndex;
  csXMLShaderTech* tech;
  size_t variant;
  iHierarchicalCache* cacheTo;

  csRef<iHierarchicalCache> progCache;

  csHash<csRef<iBase>, csString> progTagCache;
};

bool csXMLShaderTech::PrecachePass (iDocumentNode *node, ShaderPass* pass, 
                                    size_t variant, iFile* cacheFile,
                                    iHierarchicalCache* cacheTo)
{
  LoadHelpers hlp;
  hlp.synldr = parent->compiler->synldr;
  hlp.strings = parent->compiler->strings;
  hlp.stringsSvName = parent->compiler->stringsSvName;

  CachedPlugins cachedPlugins;

  if (!GetProgramPlugins (node, cachedPlugins, variant))
  {
    SetFailReason ("Couldn't load shader plugin(s).");
    return false;
  }

  if (!ParseModes (pass, node, hlp)) return false;

  WritePass (pass, cacheFile);
  
  PassActionPrecache passActionProg (GetPassNumber (pass), this, variant,
    cacheTo, "prog");
  PassActionPrecache passActionVPr (GetPassNumber (pass), this, variant,
    cacheTo, "vpr");
  LoadPassPrograms (passActionProg, passActionVPr, variant, cachedPlugins);
  
  return passActionProg.oneComboWorked && passActionVPr.oneComboWorked;
}
  
template<typename PassAction>
bool csXMLShaderTech::LoadPassPrograms (PassAction& actionProg,
                                        PassAction& actionVPr,
                                        size_t variant,
                                        CachedPlugins& cachedPlugins)
{
  bool succeeded = false;

  if (cachedPlugins.unified.programNode)
  {
    csRef<iStringArray> tags = cachedPlugins.unified.programPlugin->QueryPrecacheTags (
      cachedPlugins.unified.progType);
    for (size_t t = 0; t < tags->GetSize(); t++)
    {
      if (actionProg.Perform (tags->Get (t), cachedPlugins.unified))
      {
        succeeded = true;
        break;
      }
    }
  }

  if (cachedPlugins.vproc.programNode)
  {
    csRef<iStringArray> tags = cachedPlugins.vproc.programPlugin->QueryPrecacheTags (
      cachedPlugins.vproc.progType);
    for (size_t t = 0; t < tags->GetSize(); t++)
    {
      if (actionVPr.Perform (tags->Get (t), cachedPlugins.vproc))
      {
        succeeded = true;
        break;
      }
    }
  }

  return succeeded;
}

bool csXMLShaderTech::ParseModes (ShaderPass* pass,
                                  iDocumentNode* node, 
                                  LoadHelpers& h)
{
  csRef<iDocumentNode> nodeMixMode = node->GetNode ("mixmode");
  if (nodeMixMode != 0)
  {
    uint mm;
    if (h.synldr->ParseMixmode (nodeMixMode, mm, true))
      pass->mixMode = mm;
  }

  csRef<iDocumentNode> nodeAlphaToCoverage = node->GetNode ("alphatocoverage");
  if (nodeAlphaToCoverage)
  {
    bool atcFlag;
    h.synldr->ParseBool (nodeAlphaToCoverage, atcFlag, true);
    pass->alphaToCoverage = atcFlag;
  }
  
  csRef<iDocumentNode> nodeAtcMixMode = node->GetNode ("atcixmode");
  if (nodeAtcMixMode != 0)
  {
    uint mm;
    if (h.synldr->ParseMixmode (nodeAtcMixMode, mm, true))
      pass->atcMixMode = mm;
  }
  
  csRef<iDocumentNode> nodeAlphaMode = node->GetNode ("alphamode");
  if (nodeAlphaMode != 0)
  {
    csAlphaMode am;
    if (h.synldr->ParseAlphaMode (nodeAlphaMode, h.strings, am))
    {
      pass->alphaMode = am;
    }
  }

  csRef<iDocumentNode> nodeAlphaTestOptions = node->GetNode ("alphatestoptions");
  if (nodeAlphaTestOptions != 0)
  {
    csRef<iDocumentAttribute> funcAttr = nodeAlphaTestOptions->GetAttribute ("func");
    if (funcAttr)
    {
      const char* func = funcAttr->GetValue();
      if ((strcasecmp (func, "greater") == 0) || (strcasecmp (func, "gt") == 0))
	pass->alphaTestOpt.func = CS::Graphics::atfGreater;
      else if ((strcasecmp (func, "greater_equal") == 0) || (strcasecmp (func, "ge") == 0))
	pass->alphaTestOpt.func = CS::Graphics::atfGreaterEqual;
      else if ((strcasecmp (func, "lower") == 0) || (strcasecmp (func, "lt") == 0))
	pass->alphaTestOpt.func = CS::Graphics::atfLower;
      else if ((strcasecmp (func, "lower_equal") == 0) || (strcasecmp (func, "le") == 0))
	pass->alphaTestOpt.func = CS::Graphics::atfLowerEqual;
      else
      {
	parent->compiler->Report (CS_REPORTER_SEVERITY_WARNING,
	  nodeAlphaTestOptions,
	  "Shader %s, pass %d: unknown alpha test function %s",
	  CS::Quote::Single (parent->GetName ()),
	  GetPassNumber (pass),
	  CS::Quote::Double (func));
      }
    }
    csRef<iDocumentAttribute> threshAttr = nodeAlphaTestOptions->GetAttribute ("threshold");
    if (threshAttr)
      pass->alphaTestOpt.threshold = threshAttr->GetValueAsFloat();
  }

  csRef<iDocumentNode> nodeZmode = node->GetNode ("zmode");
  if (nodeZmode != 0)
  {
    csRef<iDocumentNode> node;
    csRef<iDocumentNodeIterator> it;
    it = nodeZmode->GetNodes ();
    while (it->HasNext ())
    {
      csRef<iDocumentNode> child = it->Next ();
      if (child->GetType () != CS_NODE_ELEMENT) continue;
      node = child;
      break;
    }

    if (h.synldr->ParseZMode (node, pass->zMode, true))
    {
      pass->overrideZmode = true;
    }
    else
    {
      h.synldr->ReportBadToken (node);
    }
  }

  csRef<iDocumentNode> nodeFlipCulling = node->GetNode ("flipculling");
  if (nodeFlipCulling)
  {
    bool flipCullFlag;
    h.synldr->ParseBool(nodeFlipCulling, flipCullFlag, false);
    pass->cullMode = flipCullFlag ? CS::Graphics::cullFlipped : CS::Graphics::cullNormal;
    if (parent->compiler->do_verbose)
    {
      parent->compiler->Report (CS_REPORTER_SEVERITY_WARNING,
	nodeFlipCulling,
	"Shader %s, pass %d: <flipculling> is deprecated",
	CS::Quote::Single (parent->GetName ()), GetPassNumber (pass));
    }
  }

  csRef<iDocumentNode> nodeCullmode = node->GetNode ("cullmode");
  if (nodeCullmode)
  {
    const char* cullModeStr = nodeCullmode->GetContentsValue();
    if (cullModeStr)
    {
      if (strcmp (cullModeStr, "normal") == 0)
      {
	pass->cullMode = CS::Graphics::cullNormal;
      }
      else if (strcmp (cullModeStr, "flipped") == 0)
      {
	pass->cullMode = CS::Graphics::cullFlipped;
      }
      else if (strcmp (cullModeStr, "disabled") == 0)
      {
	pass->cullMode = CS::Graphics::cullDisabled;
      }
      else
      {
      parent->compiler->Report (CS_REPORTER_SEVERITY_WARNING,
	nodeFlipCulling,
	"Shader %s, pass %d: invalid culling mode %s",
	CS::Quote::Single (parent->GetName ()), GetPassNumber (pass),
	CS::Quote::Single (cullModeStr));
      }
    }
  }

  csRef<iDocumentNode> nodeZOffset = node->GetNode ("zoffset");
  if (nodeZOffset)
  {
    h.synldr->ParseBool (nodeZOffset, pass->zoffset, false);
  }


  pass->wmRed = true;
  pass->wmGreen = true;
  pass->wmBlue = true;
  pass->wmAlpha = true;
  csRef<iDocumentNode> nodeWM = node->GetNode ("writemask");
  if (nodeWM != 0)
  {
    if (nodeWM->GetAttributeValue ("r") != 0)
      pass->wmRed = !strcasecmp (nodeWM->GetAttributeValue ("r"), "true");
    if (nodeWM->GetAttributeValue ("g") != 0)
      pass->wmGreen = !strcasecmp (nodeWM->GetAttributeValue ("g"), "true");
    if (nodeWM->GetAttributeValue ("b") != 0)
      pass->wmBlue = !strcasecmp (nodeWM->GetAttributeValue ("b"), "true");
    if (nodeWM->GetAttributeValue ("a") != 0)
      pass->wmAlpha = !strcasecmp (nodeWM->GetAttributeValue ("a"), "true");
  }
  
  pass->minLights = node->GetAttributeValueAsInt ("minlights");
  
  return true;
}

bool csXMLShaderTech::ParseBuffers (ShaderPassPerTag& pass, int passNum,
                                    iDocumentNode* node, 
                                    LoadHelpers& h,
                                    iShaderDestinationResolver* resolve)
{
  csRef<iDocumentNodeIterator> it;
  it = node->GetNodes (xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_BUFFER));
  while(it->HasNext ())
  {
    csRef<iDocumentNode> mapping = it->Next ();
    if (mapping->GetType () != CS_NODE_ELEMENT) continue;

    const char* dest = mapping->GetAttributeValue ("destination");
    csVertexAttrib attrib = ParseVertexAttribute (dest, resolve);
    if ((attrib > CS_VATTRIB_INVALID) 
      && (CS_VATTRIB_IS_SPECIFIC (attrib) || CS_VATTRIB_IS_GENERIC (attrib))) 
    {
      const char* cname = mapping->GetAttributeValue("customsource");
      const char *source = mapping->GetAttributeValue ("source");
      if ((source == 0) && (cname == 0))
      {
        SetFailReason ("invalid buffermapping, source missing.");
        return false;
      }
      csRenderBufferName sourceName = 
	csRenderBuffer::GetBufferNameFromDescr (source);
      
      // The user has explicitly asked for a "none" mapping
      const bool explicitlyUnmapped = (strcasecmp (source, "none") == 0);
      if (((sourceName == CS_BUFFER_NONE) || (cname != 0)) 
	&& !explicitlyUnmapped)
      {
        //custom name
	if (cname == 0)
	  cname = source;

        CS::ShaderVarStringID varID = h.stringsSvName->Request (cname);
        pass.custommapping_id.Push (varID);

        pass.custommapping_attrib.Push (attrib);
	pass.custommapping_buffer.Push (CS_BUFFER_NONE);
      }
      else
      {
        //default mapping
	if ((sourceName < CS_BUFFER_POSITION) && !explicitlyUnmapped)
        {
          SetFailReason ("invalid buffermapping, %s not allowed here.",
	    CS::Quote::Single (source));
          return false;
        }
        
  if (CS_VATTRIB_IS_SPECIFIC (attrib))
	  pass.defaultMappings[attrib] = sourceName;
	else
	{
	  pass.custommapping_attrib.Push (attrib);
	  pass.custommapping_buffer.Push (sourceName);
          pass.custommapping_id.Push (CS::InvalidShaderVarStringID);
	  /* Those buffers are mapped by default to some specific vattribs; 
	   * since they are now to be mapped to some generic vattrib,
	   * turn off the default map. */
	  if (sourceName == CS_BUFFER_POSITION)
	    pass.defaultMappings[CS_VATTRIB_POSITION] = CS_BUFFER_NONE;
	}
      }
    }
    else
    {
      if (attrib != CS_VATTRIB_UNUSED && parent->compiler->do_verbose)
      {
        parent->compiler->Report (CS_REPORTER_SEVERITY_WARNING,
	  "Shader %s, pass %d: invalid buffer destination %s",
	  CS::Quote::Single (parent->GetName ()), passNum,
	  CS::Quote::Single (dest));
      }
    }
  }
  return true;
}

bool csXMLShaderTech::ParseTextures (ShaderPassPerTag& pass,
                                     iDocumentNode* node, 
                                     LoadHelpers& h,
                                     iShaderDestinationResolver* resolve)
{
  csRef<iDocumentNodeIterator> it;
  it = node->GetNodes (xmltokens.Request (
    csXMLShaderCompiler::XMLTOKEN_TEXTURE));
  while(it->HasNext ())
  {
    csRef<iDocumentNode> mapping = it->Next ();
    if (mapping->GetType() != CS_NODE_ELEMENT) continue;
    const char* dest = mapping->GetAttributeValue ("destination");
    if (mapping->GetAttribute("name") && dest)
    {
      int texUnit = resolve ? resolve->ResolveTU (dest) : -1;
      if (texUnit < 0)
      {
	if (csStrNCaseCmp (dest, "unit ", 5) == 0)
	{
	  dest += 5;
	  char dummy;
	  if (sscanf (dest, "%d%c", &texUnit, &dummy) != 1)
	    texUnit = -1;
	}
      }

      if (texUnit < 0) continue;
      
      ShaderPass::TextureMapping texMap;
      
      const char* compareMode = mapping->GetAttributeValue ("comparemode");
      const char* compareFunc = mapping->GetAttributeValue ("comparefunc");
      if (compareMode && compareFunc)
      {
        if (strcmp (compareMode, "rToTexture") == 0)
          texMap.texCompare.mode = CS::Graphics::TextureComparisonMode::compareR;
        else if (strcmp (compareMode, "none") == 0)
          texMap.texCompare.mode = CS::Graphics::TextureComparisonMode::compareNone;
	else
	{
          SetFailReason ("invalid texture comparison mode %s",
	    CS::Quote::Single (compareMode));
          return false;
	}
      
        if (strcmp (compareFunc, "lequal") == 0)
          texMap.texCompare.function = CS::Graphics::TextureComparisonMode::funcLEqual;
        else if (strcmp (compareFunc, "gequal") == 0)
          texMap.texCompare.function = CS::Graphics::TextureComparisonMode::funcGEqual;
	else
	{
          SetFailReason ("invalid texture comparison function %s",
	    CS::Quote::Single (compareMode));
          return false;
	}
      }
      
      CS::Graphics::ShaderVarNameParser parser (
        mapping->GetAttributeValue("name"));
      texMap.tex.id = h.stringsSvName->Request (parser.GetShaderVarName ());
      parser.FillArrayWithIndices (texMap.tex.indices);
      const char* fallbackName = mapping->GetAttributeValue("fallback");
      if (fallbackName && *fallbackName)
      {
	CS::Graphics::ShaderVarNameParser fbparser (
	  fallbackName);
	texMap.fallback.id = h.stringsSvName->Request (
	  fbparser.GetShaderVarName ());
	fbparser.FillArrayWithIndices (texMap.fallback.indices);
      }
      texMap.textureUnit = texUnit;
      pass.textures.Push (texMap);
    }
  }
  return true;
}

bool csXMLShaderTech::ParseInstances (ShaderPassPerTag& pass, int passNum,
                                      iDocumentNode* node,
                                      LoadHelpers& hlp,
                                      iShaderDestinationResolver* resolve)
{
  csRef<iDocumentNodeIterator> it = node->GetNodes (xmltokens.Request (
    csXMLShaderCompiler::XMLTOKEN_INSTANCEPARAM));
  while(it->HasNext ())
  {
    csRef<iDocumentNode> mapping = it->Next ();
    if (mapping->GetType () != CS_NODE_ELEMENT) continue;

    const char* dest = mapping->GetAttributeValue ("destination");
    csVertexAttrib attrib = ParseVertexAttribute (dest, resolve);
    if (attrib > CS_VATTRIB_INVALID)
    {
      const char *source = mapping->GetAttributeValue ("source");
      if (source == 0)
      {
        SetFailReason ("invalid instanceparam, source missing.");
        return false;
      }

      ShaderPass::InstanceMapping map;
      map.variable = hlp.stringsSvName->Request (source);
      map.destination = attrib;

      pass.instances_binds.Push (map);
    }
    else
    {
      if (attrib != CS_VATTRIB_UNUSED)
      {
        SetFailReason ("invalid instanceparam destination.");
        if(do_verbose)
        {
          parent->compiler->Report (CS_REPORTER_SEVERITY_WARNING,
            "Shader %s, pass %d: invalid instanceparam destination %s",
            CS::Quote::Single (parent->GetName ()), passNum,
	    CS::Quote::Single (dest));
        }
        return false;
      }
    }
  }
  pass.instances_binds.ShrinkBestFit ();
  // Read number of instances SV
  {
    csRef<iDocumentNode> instancesNumNode = node->GetNode (xmltokens.Request (
      csXMLShaderCompiler::XMLTOKEN_INSTANCESNUM));
    if (instancesNumNode && (instancesNumNode->GetType() == CS_NODE_ELEMENT))
    {
      const char* source = instancesNumNode->GetContentsValue();
      pass.instancesNumVar = hlp.stringsSvName->Request (source);
    }
  }
  return true;
}

// Used to generate data written on disk!
enum
{
  cacheFlagWMR = 4,
  cacheFlagWMG,
  cacheFlagWMB,
  cacheFlagWMA,
  
  cacheFlagOverrideZ,
  cacheFlagZoffset,
  
  cacheFlagAlphaAuto,
  
  cacheFlagAlphaToCoverage,
  
  cacheFlagLast
};

bool csXMLShaderTech::WritePass (ShaderPass* pass, 
                                 iFile* cacheFile)
{
  if (!cacheFile) return false;

  {
    uint32 cacheFlags = 0;
    
    if (pass->wmRed) cacheFlags |= 1 << cacheFlagWMR;
    if (pass->wmGreen) cacheFlags |= 1 << cacheFlagWMG;
    if (pass->wmBlue) cacheFlags |= 1 << cacheFlagWMB;
    if (pass->wmAlpha) cacheFlags |= 1 << cacheFlagWMA;
    
    if (pass->overrideZmode) cacheFlags |= 1 << cacheFlagOverrideZ;
    if (pass->zoffset) cacheFlags |= 1 << cacheFlagZoffset;
    
    if (pass->alphaMode.autoAlphaMode) cacheFlags |= 1 << cacheFlagAlphaAuto;
    
    if (pass->alphaToCoverage) cacheFlags |= 1 << cacheFlagAlphaToCoverage;
    
    cacheFlags |= uint (pass->cullMode) << cacheFlagLast;
    
    uint32 diskFlags = csLittleEndian::UInt32 (cacheFlags);
    if (cacheFile->Write ((char*)&diskFlags, sizeof (diskFlags))
	!= sizeof (diskFlags)) return false;
  }
  
  {
    uint32 diskMM = csLittleEndian::UInt32 (pass->mixMode);
    if (cacheFile->Write ((char*)&diskMM, sizeof (diskMM))
	!= sizeof (diskMM)) return false;
  }
  {
    uint32 diskMM = csLittleEndian::UInt32 (pass->atcMixMode);
    if (cacheFile->Write ((char*)&diskMM, sizeof (diskMM))
	!= sizeof (diskMM)) return false;
  }
  
  if (pass->alphaMode.autoAlphaMode)
  {
    if (!WriteShadervarName (pass->alphaMode.autoModeTexture, cacheFile))
      return false;
  }
  else
  {
    uint32 diskAlpha = csLittleEndian::UInt32 (pass->alphaMode.alphaType);
    if (cacheFile->Write ((char*)&diskAlpha, sizeof (diskAlpha))
	!= sizeof (diskAlpha)) return false;
  }
  
  {
    uint32 diskATOFunc = csLittleEndian::UInt32 (pass->alphaTestOpt.func);
    if (cacheFile->Write ((char*)&diskATOFunc, sizeof (diskATOFunc))
	!= sizeof (diskATOFunc)) return false;
  }
  {
    uint32 diskATOThresh = csLittleEndian::UInt32 (csIEEEfloat::FromNative (pass->alphaTestOpt.threshold));
    if (cacheFile->Write ((char*)&diskATOThresh, sizeof (diskATOThresh))
	!= sizeof (diskATOThresh)) return false;
  }
  
  {
    uint32 diskZ = csLittleEndian::UInt32 (pass->zMode);
    if (cacheFile->Write ((char*)&diskZ, sizeof (diskZ))
	!= sizeof (diskZ)) return false;
  }
  {
    int32 diskMinLights = csLittleEndian::Int32 (pass->minLights);
    if (cacheFile->Write ((char*)&diskMinLights, sizeof (diskMinLights))
	!= sizeof (diskMinLights)) return false;
  }
  return true;
}
  
iShaderProgram::CacheLoadResult csXMLShaderTech::LoadPassFromCache (
  ShaderPass* pass, iDocumentNode* node, size_t variant, iFile* cacheFile,
  iHierarchicalCache* cache)
{
  if (!cacheFile) return iShaderProgram::loadFail;

  CachedPlugins plugins;
  if (!GetProgramPlugins (node, plugins, variant))
    return iShaderProgram::loadFail;
  
  if (!ReadPass (pass, cacheFile)) return iShaderProgram::loadFail;
  
  csString tagShader, tagVPr;
  
  iShaderProgram::CacheLoadResult loadRes;

  iBase *previous = 0;
  csRef<iShaderDestinationResolver> resolve;

  {
    // new unified syntax
    int passNum (GetPassNumber (pass));
    csRef<iHierarchicalCache> progCache;
    if (cache)
      progCache = cache->GetRootedCache (csString().Format ("/pass%dprog", passNum));
    loadRes = LoadProgramFromCache (0, progCache, plugins.unified, pass->program,
                                    tagShader, passNum);
    if (loadRes != iShaderProgram::loadSuccessShaderValid) return loadRes;
    previous = pass->program;
    resolve = scfQueryInterface<iShaderDestinationResolver> (pass->program);
  }

  if (plugins.vproc.available)
  {
    csRef<iHierarchicalCache> vprCache;
    if (cache) vprCache = cache->GetRootedCache (csString().Format (
      "/pass%dvpr", GetPassNumber (pass)));
    loadRes = LoadProgramFromCache (previous, vprCache,
                                    plugins.vproc, pass->vproc, tagVPr,
                                    GetPassNumber (pass));
    if (loadRes != iShaderProgram::loadSuccessShaderValid) return loadRes;
  }
  
  LoadHelpers hlp;
  hlp.synldr = parent->compiler->synldr;
  hlp.strings = parent->compiler->strings;
  hlp.stringsSvName = parent->compiler->stringsSvName;

  int passNum (GetPassNumber (pass));
  //if we got this far, load buffermappings
  if (!ParseBuffers (*pass, passNum,
      node, hlp, resolve))
    return iShaderProgram::loadFail;

  //get texturemappings
  if (!ParseTextures (*pass, node, hlp, resolve))
    return iShaderProgram::loadFail;

  // Load pseudo-instancing binds
  if (!ParseInstances (*pass, passNum, node, hlp, resolve))
    return iShaderProgram::loadFail;
  
  return iShaderProgram::loadSuccessShaderValid;
}

bool csXMLShaderTech::ReadPass (ShaderPass* pass, 
                                iFile* cacheFile)
{
  if (!cacheFile) return false;

  {
    uint32 diskFlags;
    if (cacheFile->Read ((char*)&diskFlags, sizeof (diskFlags))
	!= sizeof (diskFlags)) return false;
    uint32 cacheFlags = csLittleEndian::UInt32 (diskFlags);
  
    pass->wmRed = (cacheFlags & (1 << cacheFlagWMR)) != 0;
    pass->wmGreen = (cacheFlags & (1 << cacheFlagWMG)) != 0;
    pass->wmBlue = (cacheFlags & (1 << cacheFlagWMB)) != 0;
    pass->wmAlpha = (cacheFlags & (1 << cacheFlagWMA)) != 0;
    
    pass->overrideZmode = (cacheFlags & (1 << cacheFlagOverrideZ)) != 0;
    pass->zoffset = (cacheFlags & (1 << cacheFlagZoffset)) != 0;
    
    pass->alphaMode.autoAlphaMode = (cacheFlags & (1 << cacheFlagAlphaAuto)) != 0;
    
    pass->alphaToCoverage = (cacheFlags & (1 << cacheFlagAlphaToCoverage)) != 0;

    pass->cullMode = (CS::Graphics::MeshCullMode)((cacheFlags >> cacheFlagLast) & 0x3);
  }
  
  {
    uint32 diskMM;
    if (cacheFile->Read ((char*)&diskMM, sizeof (diskMM))
	!= sizeof (diskMM)) return false;
    pass->mixMode = csLittleEndian::UInt32 (diskMM);
  }
  {
    uint32 diskMM;
    if (cacheFile->Read ((char*)&diskMM, sizeof (diskMM))
	!= sizeof (diskMM)) return false;
    pass->atcMixMode = csLittleEndian::UInt32 (diskMM);
  }
  
  if (pass->alphaMode.autoAlphaMode)
  {
    pass->alphaMode.autoModeTexture = ReadShadervarName (cacheFile);
  }
  else
  {
    uint32 diskAlpha;
    if (cacheFile->Read ((char*)&diskAlpha, sizeof (diskAlpha))
	!= sizeof (diskAlpha)) return false;
    pass->alphaMode.alphaType =
      (csAlphaMode::AlphaType)csLittleEndian::UInt32 (diskAlpha);
  }
  
  {
    uint32 diskATOFunc;
    if (cacheFile->Read ((char*)&diskATOFunc, sizeof (diskATOFunc))
	!= sizeof (diskATOFunc)) return false;
    pass->alphaTestOpt.func =
      (CS::Graphics::AlphaTestFunction)csLittleEndian::UInt32 (diskATOFunc);
  }
  {
    uint32 diskATOThresh;
    if (cacheFile->Read ((char*)&diskATOThresh, sizeof (diskATOThresh))
	!= sizeof (diskATOThresh)) return false;
    pass->alphaTestOpt.threshold =
      csIEEEfloat::ToNative (csLittleEndian::UInt32 (diskATOThresh));
  }
  
  {
    uint32 diskZ;
    if (cacheFile->Read ((char*)&diskZ, sizeof (diskZ))
	!= sizeof (diskZ)) return false;
    pass->zMode = (csZBufMode)csLittleEndian::UInt32 (diskZ);
  }
  {
    int32 diskMinLights;
    if (cacheFile->Read ((char*)&diskMinLights, sizeof (diskMinLights))
	!= sizeof (diskMinLights)) return false;
    pass->minLights = csLittleEndian::Int32 (diskMinLights);
  }
  return true;
}
  
bool csXMLShaderCompiler::LoadSVBlock (iLoaderContext* ldr_context,
    iDocumentNode *node, iShaderVariableContext *context)
{
  csRef<csShaderVariable> svVar;
  
  csRef<iDocumentNodeIterator> it = node->GetNodes ("shadervar");
  while (it->HasNext ())
  {
    csRef<iDocumentNode> var = it->Next ();
    svVar.AttachNew (new csShaderVariable);

    if (synldr->ParseShaderVar (ldr_context, var, *svVar))
      context->AddVariable(svVar);
  }

  return true;
}

bool csXMLShaderTech::WriteShadervarName (CS::ShaderVarStringID svid,
					  iFile* cacheFile)
{
  const char* svStr = parent->compiler->stringsSvName->Request (svid);
  return CS::PluginCommon::ShaderCacheHelper::WriteString (cacheFile, svStr);
}

CS::ShaderVarStringID csXMLShaderTech::ReadShadervarName (iFile* cacheFile)
{
  const char* texSvName = 
    CS::PluginCommon::ShaderCacheHelper::ReadString (cacheFile);
  return texSvName ? parent->compiler->stringsSvName->Request (texSvName)
	      : CS::InvalidShaderVarStringID;
}

csPtr<iShaderProgram> csXMLShaderTech::LoadProgram (
  iShaderDestinationResolver* resolve, iDocumentNode* node, ShaderPass* /*pass*/,
  size_t variant, iHierarchicalCache* cacheTo, CachedPlugin& cacheInfo,
  csString& tag)
{
  csRef<iShaderProgram> program;

  program = cacheInfo.programPlugin->CreateProgram (cacheInfo.progType);
  if (program == 0)
    return 0;
  if (!program->Load (resolve, cacheInfo.programNode))
    return 0;

  /* Even if compilation fails, for since we need to handle the case where a
      program is valid but not supported on the current HW in caching, 
      flag program as available */
  cacheInfo.available = true;
  
  csRef<iString> progTag;
  if (!program->Compile (cacheTo, &progTag))
    return 0;
  if (!progTag.IsValid()) return 0;
  tag = progTag->GetData();
    
  return csPtr<iShaderProgram> (program);
}
  
iShaderProgram::CacheLoadResult csXMLShaderTech::LoadProgramFromCache (
  iBase* previous, iHierarchicalCache* cache, const CachedPlugin& cacheInfo,
  csRef<iShaderProgram>& prog, csString& tag, int passNumber)
{
  prog = cacheInfo.programPlugin->CreateProgram (cacheInfo.progType);
  csRef<iString> progTag;
  csRef<iString> failReason;
  iShaderProgram::CacheLoadResult loadRes = prog->LoadFromCache (cache, 
    previous, cacheInfo.programNode, &failReason, &progTag);
  if (loadRes == iShaderProgram::loadFail)
  {
    if (parent->compiler->do_verbose)
      SetFailReason(
	"Failed to read %s for pass %d from cache: %s",
	CS::Quote::Single (cacheInfo.progType.GetData()), passNumber,
	failReason.IsValid() ? failReason->GetData() : "no reason given");
    return iShaderProgram::loadFail;
  }
  
  if (progTag.IsValid()) tag = progTag->GetData();

  return loadRes;
}

bool csXMLShaderTech::PrecacheProgram (iBase* previous, iDocumentNode* node, 
  size_t variant, iHierarchicalCache* cacheTo, 
  CachedPlugin& cacheInfo, csRef<iBase>& object, const char* tag)
{
  return cacheInfo.programPlugin->Precache (cacheInfo.progType, tag, previous,
    cacheInfo.programNode, cacheTo, &object);
}

bool csXMLShaderTech::GetProgramPlugins (iDocumentNode *node,
                                         CachedPlugins& cacheInfo,
                                         size_t variant)
{
    csRef<iDocumentNode> programNodeShader;
    programNodeShader = node->GetNode (xmltokens.Request (
      csXMLShaderCompiler::XMLTOKEN_SHADER));

    if (programNodeShader)
    {
      if (!GetProgramPlugin (programNodeShader, cacheInfo.unified, variant))
        return false;
    }
    else
    {
      if (!csXMLShaderWrapper::FillCachedPlugin (this, node, cacheInfo.unified, variant))
        return false;
    }

  {
    csRef<iDocumentNode> programNodeVPr;
    programNodeVPr = node->GetNode(xmltokens.Request (
      csXMLShaderCompiler::XMLTOKEN_VPROC));
      
    if (programNodeVPr)
      if (!GetProgramPlugin (programNodeVPr, cacheInfo.vproc, variant))
        return false;
  }

  return true;
}

bool csXMLShaderTech::GetProgramPlugin (iDocumentNode *node,
                                        CachedPlugin& cacheInfo,
                                        size_t variant)
{
  if (node->GetAttributeValue("plugin") == 0)
  {
    parent->compiler->Report (CS_REPORTER_SEVERITY_ERROR,
      "No shader program plugin specified for <%s> in shader %s",
      node->GetValue (), CS::Quote::Single (parent->GetName ()));
    return false;
  }

  csStringFast<256> plugin ("crystalspace.graphics3d.shader.");
  plugin.Append (node->GetAttributeValue("plugin"));
  // @@@ Also check if 'plugin' is a full class ID

  //load the plugin
  csRef<iShaderProgramPlugin> plg;
  plg = csLoadPluginCheck<iShaderProgramPlugin> (parent->compiler->objectreg,
	plugin, false);
  if(!plg)
  {
    if (parent->compiler->do_verbose)
      parent->compiler->Report (CS_REPORTER_SEVERITY_WARNING,
	  "Couldn't retrieve shader plugin %s for <%s> in shader %s",
	  CS::Quote::Single (plugin.GetData()),
	  node->GetValue (),
	  CS::Quote::Single (parent->GetName ()));
    return false;
  }

  const char* programType = node->GetAttributeValue("type");
  if (programType == 0)
    programType = node->GetValue ();
  csRef<iDocumentNode> programNode;
  if (node->GetAttributeValue ("file") != 0)
    programNode = parent->LoadProgramFile (node->GetAttributeValue ("file"), 
      variant);
  else
    programNode = node;

  /* Even if compilation fails, for since we need to handle the case where a
      program is valid but not supported on the current HW in caching, 
      flag program as available */
  cacheInfo.available = true;
  cacheInfo.pluginID = plugin;
  cacheInfo.progType = programType;
  cacheInfo.programPlugin = plg;
  cacheInfo.programNode = programNode;
  return true;
}

bool csXMLShaderTech::LoadBoilerplate (iLoaderContext* ldr_context, 
                                       iDocumentNode* node,
                                       iDocumentNode* parentSV)
{
  if ((node->GetType() != CS_NODE_ELEMENT) || 
    (xmltokens.Request (node->GetValue()) 
    != csXMLShaderCompiler::XMLTOKEN_TECHNIQUE))
  {
    if (do_verbose) SetFailReason ("Node is not a well formed technique");
    return 0;
  }
  
  iStringSet* strings = parent->compiler->strings;
  iShaderManager* shadermgr = parent->shadermgr;
  
  int requiredCount;
  const csSet<csStringID>& requiredTags = 
    shadermgr->GetTags (TagRequired, requiredCount);
  int forbiddenCount;
  const csSet<csStringID>& forbiddenTags = 
    shadermgr->GetTags (TagForbidden, forbiddenCount);

  int requiredPresent = 0;
  csRef<iDocumentNodeIterator> it = node->GetNodes (
    xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_TAG));
  while (it->HasNext ())
  {
    csRef<iDocumentNode> tag = it->Next ();

    const char* tagName = tag->GetContentsValue ();
    csStringID tagID = strings->Request (tagName);
    if (requiredTags.In (tagID))
    {
      requiredPresent++;
    }
    else if (forbiddenTags.In (tagID))
    {
      if (do_verbose) SetFailReason ("Shader tag %s is forbidden", 
	CS::Quote::Single (tagName));
      return false;
    }
  }

  if ((requiredCount != 0) && (requiredPresent == 0))
  {
    if (do_verbose) SetFailReason ("No required shader tag is present");
    return false;
  }

  //load shadervariable definitions
  if (parentSV)
  {
    csRef<iDocumentNode> varNode = parentSV->GetNode(
      xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_SHADERVARS));
    if (varNode)
      parent->compiler->LoadSVBlock (ldr_context, varNode, &svcontext);
  }

  csRef<iDocumentNode> varNode = node->GetNode(
    xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_SHADERVARS));

  if (varNode)
    parent->compiler->LoadSVBlock (ldr_context, varNode, &svcontext);

  return true;
}

bool csXMLShaderTech::Load (iLoaderContext* ldr_context,
    iDocumentNode* node, iDocumentNode* parentSV, size_t variant,
    iHierarchicalCache* cacheTo)
{
  bool result = LoadBoilerplate (ldr_context, node, parentSV);

  csRef<csMemFile> cacheFile;
  if (cacheTo)
  {
    cacheFile.AttachNew (new csMemFile);
    uint32 diskMagic = csLittleEndian::UInt32 (cacheFileMagic);
    if (cacheFile->Write ((char*)&diskMagic, sizeof (diskMagic))
	!= sizeof (diskMagic))
      cacheFile.Invalidate();
    
    if (cacheFile.IsValid())
    {
      csRef<iDocument> boilerplateDoc (parent->compiler->CreateCachingDoc());
      csRef<iDocumentNode> root = boilerplateDoc->CreateRoot();
      csRef<iDocumentNode> techNode = root->CreateNodeBefore (node->GetType());
      techNode->SetValue (node->GetValue ());
      CS::DocSystem::CloneAttributes (node, techNode);
      
      csRef<iDocumentNodeIterator> it = node->GetNodes ();
      while(it->HasNext ())
      {
	csRef<iDocumentNode> child = it->Next ();
	if (child->GetType() == CS_NODE_COMMENT) continue;
	if (xmltokens.Request (child->GetValue())
	  == csXMLShaderCompiler::XMLTOKEN_PASS) continue;
	  
	csRef<iDocumentNode> newNode = techNode->CreateNodeBefore (child->GetType());
	CS::DocSystem::CloneNode (child, newNode);
      }
      
      csRef<iDataBuffer> boilerplateBuf (parent->compiler->WriteNodeToBuf (
        boilerplateDoc));
      if (!CS::PluginCommon::ShaderCacheHelper::WriteDataBuffer (
	  cacheFile, boilerplateBuf))
	cacheFile.Invalidate();
    }
  }

  //count passes
  passesCount = 0;
  csRef<iDocumentNodeIterator> it = node->GetNodes (xmltokens.Request (
    csXMLShaderCompiler::XMLTOKEN_PASS));
  while(it->HasNext ())
  {
    it->Next ();
    passesCount++;
  }
  
  if (cacheFile.IsValid())
  {
    int32 diskPassNum = csLittleEndian::Int32 ((int32)passesCount);
    if (cacheFile->Write ((char*)&diskPassNum, sizeof (diskPassNum))
	!= sizeof (diskPassNum))
      cacheFile.Invalidate();
  }

  //alloc passes
  passes = new ShaderPass[passesCount];
  uint i;
  for (i = 0; i < passesCount; i++)
  {
    ShaderPass& pass = passes[i];
    pass.alphaMode.autoAlphaMode = true;
    pass.alphaMode.autoModeTexture = 
      parent->compiler->stringsSvName->Request (CS_MATERIAL_TEXTURE_DIFFUSE);
  }


  //first thing we load is the programs for each pass.. if one of them fail,
  //fail the whole technique
  int currentPassNr = 0;
  it = node->GetNodes (xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_PASS));
  while (it->HasNext ())
  {
    csRef<iDocumentNode> passNode = it->Next ();
    result &= LoadPass (passNode, &passes[currentPassNr++], variant, cacheFile, cacheTo);
  }
  
  if (cacheFile.IsValid())
  {
    cacheTo->CacheData (cacheFile->GetData(), cacheFile->GetSize(),
      "/passes");
  }

  return result;
}
  
bool csXMLShaderTech::Precache (iDocumentNode* node, size_t variant, 
                                iHierarchicalCache* cacheTo)
{
  csRef<csMemFile> cacheFile;
  cacheFile.AttachNew (new csMemFile);
  uint32 diskMagic = csLittleEndian::UInt32 (cacheFileMagic);
  if (cacheFile->Write ((char*)&diskMagic, sizeof (diskMagic))
      != sizeof (diskMagic))
    return false;
  
  if (cacheFile.IsValid())
  {
    csRef<iDocument> boilerplateDoc (parent->compiler->CreateCachingDoc());
    csRef<iDocumentNode> root = boilerplateDoc->CreateRoot();
    csRef<iDocumentNode> techNode = root->CreateNodeBefore (node->GetType());
    techNode->SetValue (node->GetValue ());
    CS::DocSystem::CloneAttributes (node, techNode);
    
    csRef<iDocumentNodeIterator> it = node->GetNodes ();
    while(it->HasNext ())
    {
      csRef<iDocumentNode> child = it->Next ();
      if (child->GetType() == CS_NODE_COMMENT) continue;
      if (xmltokens.Request (child->GetValue())
	== csXMLShaderCompiler::XMLTOKEN_PASS) continue;
	
      csRef<iDocumentNode> newNode = techNode->CreateNodeBefore (child->GetType());
      CS::DocSystem::CloneNode (child, newNode);
    }
    
    csRef<iDataBuffer> boilerplateBuf (parent->compiler->WriteNodeToBuf (
      boilerplateDoc));
    if (!CS::PluginCommon::ShaderCacheHelper::WriteDataBuffer (
	cacheFile, boilerplateBuf))
      return false;
  }

  //count passes
  passesCount = 0;
  csRef<iDocumentNodeIterator> it = node->GetNodes (xmltokens.Request (
    csXMLShaderCompiler::XMLTOKEN_PASS));
  while(it->HasNext ())
  {
    it->Next ();
    passesCount++;
  }
  
  int32 diskPassNum = csLittleEndian::Int32 ((int32)passesCount);
  if (cacheFile->Write ((char*)&diskPassNum, sizeof (diskPassNum))
      != sizeof (diskPassNum))
    return false;

  //alloc passes
  passes = new ShaderPass[passesCount];
  uint i;
  for (i = 0; i < passesCount; i++)
  {
    ShaderPass& pass = passes[i];
    pass.alphaMode.autoAlphaMode = true;
    pass.alphaMode.autoModeTexture = 
      parent->compiler->stringsSvName->Request (CS_MATERIAL_TEXTURE_DIFFUSE);
  }


  //first thing we load is the programs for each pass.. if one of them fail,
  //fail the whole technique
  int currentPassNr = 0;
  it = node->GetNodes (xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_PASS));
  while (it->HasNext ())
  {
    csRef<iDocumentNode> passNode = it->Next ();
    if (!PrecachePass (passNode, &passes[currentPassNr++], variant, cacheFile, cacheTo))
    {
      return false;
    }
  }
  
  return cacheTo->CacheData (cacheFile->GetData(), cacheFile->GetSize(),
    "/passes");
}
  
iShaderProgram::CacheLoadResult csXMLShaderTech::LoadFromCache (
  iLoaderContext* ldr_context, iDocumentNode* node, 
  iHierarchicalCache* cache, iDocumentNode* parentSV, size_t variant)
{
  csRef<iDataBuffer> cacheData (cache->ReadCache ("/passes"));
  if (!cacheData.IsValid())
  {
    SetFailReason("Failed to load cache data from %s",
		  CS::Quote::Single ("/passes"));
    return iShaderProgram::loadFail;
  }

  csMemFile cacheFile (cacheData, true);
  
  uint32 diskMagic;
  size_t read = cacheFile.Read ((char*)&diskMagic, sizeof (diskMagic));
  if (read != sizeof (diskMagic)) return iShaderProgram::loadFail;
  if (csLittleEndian::UInt32 (diskMagic) != cacheFileMagic)
    return iShaderProgram::loadFail;
  
  csRef<iDataBuffer> boilerPlateDocBuf =
    CS::PluginCommon::ShaderCacheHelper::ReadDataBuffer (&cacheFile);
  if (!boilerPlateDocBuf.IsValid()) return iShaderProgram::loadFail;
  
  csRef<iDocumentNode> boilerplateNode (parent->compiler->ReadNodeFromBuf (
    boilerPlateDocBuf));
  if (!boilerplateNode.IsValid()) return iShaderProgram::loadFail;
  
  if (!LoadBoilerplate (ldr_context, boilerplateNode, parentSV))
    return iShaderProgram::loadSuccessShaderInvalid;
  
  int32 diskPassNum;
  read = cacheFile.Read ((char*)&diskPassNum, sizeof (diskPassNum));
  if (read != sizeof (diskPassNum))
    return iShaderProgram::loadFail;
  
  size_t nodePassesCount = 0;
  csRef<iDocumentNodeIterator> it = node->GetNodes (xmltokens.Request (
    csXMLShaderCompiler::XMLTOKEN_PASS));
  while(it->HasNext ())
  {
    it->Next ();
    nodePassesCount++;
  }
  
  passesCount = csLittleEndian::Int32 (diskPassNum);
  if (passesCount != nodePassesCount) return iShaderProgram::loadFail;
  passes = new ShaderPass[passesCount];
  it = node->GetNodes (xmltokens.Request (csXMLShaderCompiler::XMLTOKEN_PASS));
  for (uint p = 0; p < passesCount; p++)
  {
    csRef<iDocumentNode> child = it->Next ();
    iShaderProgram::CacheLoadResult loadRes =
      LoadPassFromCache (&passes[p], child, variant, &cacheFile, cache);
    if (loadRes != iShaderProgram::loadSuccessShaderValid)
      return loadRes;
  }
  
  return iShaderProgram::loadSuccessShaderValid;
}

bool csXMLShaderTech::ActivatePass (size_t number)
{
  if(number>=passesCount)
    return false;

  currentPass = number;

  ShaderPass* thispass = &passes[currentPass];
  if(thispass->vproc) thispass->vproc->Activate ();
  if(thispass->program) thispass->program->Activate ();
  
  iGraphics3D* g3d = parent->g3d;
  if (thispass->overrideZmode)
  {
    oldZmode = g3d->GetZMode ();
    g3d->SetZMode (thispass->zMode);
  }

  g3d->GetWriteMask (orig_wmRed, orig_wmGreen, orig_wmBlue, orig_wmAlpha);
  g3d->SetWriteMask (thispass->wmRed, thispass->wmGreen, thispass->wmBlue,
    thispass->wmAlpha);

  return true;
}

bool csXMLShaderTech::DeactivatePass ()
{
  if(currentPass>=passesCount)
    return false;
  ShaderPass* thispass = &passes[currentPass];
  currentPass = (size_t)~0;

  if(thispass->vproc) thispass->vproc->Deactivate ();
  if(thispass->program) thispass->program->Deactivate ();

  iGraphics3D* g3d = parent->g3d;
  g3d->DeactivateBuffers (thispass->custommapping_attrib.GetArray (), 
    (int)thispass->custommapping_attrib.GetSize ());

  int texturesCount = (int)thispass->textures.GetSize();
  CS_ALLOC_STACK_ARRAY(int, textureUnits, texturesCount);
  for (int j = 0; j < texturesCount; j++)
    textureUnits[j] = thispass->textures[j].textureUnit;
  g3d->SetTextureState(textureUnits, 0, texturesCount);
  g3d->SetTextureComparisonModes (textureUnits, 0, texturesCount);
  
  if (thispass->overrideZmode)
    g3d->SetZMode (oldZmode);

  g3d->SetWriteMask (orig_wmRed, orig_wmGreen, orig_wmBlue, orig_wmAlpha);

  return true;
}

bool csXMLShaderTech::SetupPass (const csRenderMesh *mesh, 
			         csRenderMeshModes& modes,
			         const csShaderVariableStack& stack)
{
  if(currentPass>=passesCount)
    return false;

  iGraphics3D* g3d = parent->g3d;
  ShaderPass* thispass = &passes[currentPass];

  int lightCount = 0;
  if (stack.GetSize() > parent->compiler->stringLightCount)
  {
    csShaderVariable* svLightCount = stack[parent->compiler->stringLightCount];
    if (svLightCount != 0)
      svLightCount->GetValue (lightCount);
  }
  if (lightCount < thispass->minLights) return false;
  
  //first run the preprocessor
  if(thispass->vproc) thispass->vproc->SetupState (mesh, modes, stack);

  size_t buffersCount = thispass->custommapping_attrib.GetSize ();
  CS_ALLOC_STACK_ARRAY(iRenderBuffer*, customBuffers, buffersCount);
  //now map our buffers. all refs should be set
  size_t i;
  for (i = 0; i < thispass->custommapping_attrib.GetSize (); i++)
  {
    if (thispass->custommapping_buffer[i] != CS_BUFFER_NONE)
    {
      customBuffers[i] = modes.buffers->GetRenderBuffer (
	thispass->custommapping_buffer[i]);
    }
    else if (thispass->custommapping_id[i] < (CS::StringIDValue)stack.GetSize ())
    {
      csShaderVariable* var = 0;
      var = csGetShaderVariableFromStack (stack, thispass->custommapping_id[i]);
      if (var)
        var->GetValue (customBuffers[i]);
      else
        customBuffers[i] = 0;
    }
    else
      customBuffers[i] = 0;
  }
  g3d->ActivateBuffers (modes.buffers, thispass->defaultMappings);
  g3d->ActivateBuffers (thispass->custommapping_attrib.GetArray (), 
    customBuffers, (uint)buffersCount);
  
  //and the textures
  size_t textureCount = thispass->textures.GetSize();
  CS_ALLOC_STACK_ARRAY(int, textureUnits, textureCount);
  CS_ALLOC_STACK_ARRAY(iTextureHandle*, textureHandles, textureCount);
  CS_ALLOC_STACK_ARRAY(CS::Graphics::TextureComparisonMode, texCompare,
    textureCount);
  for (size_t j = 0; j < textureCount; j++)
  {
    textureUnits[j] = thispass->textures[j].textureUnit;
    csShaderVariable* var = 0;
    if (size_t (thispass->textures[j].tex.id) < stack.GetSize ())
    {
      var = csGetShaderVariableFromStack (stack, thispass->textures[j].tex.id);
      if (var != 0)
        var = CS::Graphics::ShaderVarArrayHelper::GetArrayItem (var, 
          thispass->textures[j].tex.indices.GetArray(),
          thispass->textures[j].tex.indices.GetSize(),
          CS::Graphics::ShaderVarArrayHelper::maFail);
    }
    if (!var && (size_t (thispass->textures[j].fallback.id) < stack.GetSize ()))
    {
      var = csGetShaderVariableFromStack (stack,
        thispass->textures[j].fallback.id);
      if (var != 0)
        var = CS::Graphics::ShaderVarArrayHelper::GetArrayItem (var, 
          thispass->textures[j].fallback.indices.GetArray(),
          thispass->textures[j].fallback.indices.GetSize(),
          CS::Graphics::ShaderVarArrayHelper::maFail);
    }
    if (var)
    {
      iTextureWrapper* wrap;
      var->GetValue (wrap);
      if (wrap) 
      {
	wrap->Visit ();
	textureHandles[j] = wrap->GetTextureHandle ();
      } else 
	var->GetValue (textureHandles[j]);
    } else
      textureHandles[j] = 0;
    texCompare[j] = thispass->textures[j].texCompare;
  }
  g3d->SetTextureState (textureUnits, textureHandles, (int)textureCount);
  g3d->SetTextureComparisonModes (textureUnits, texCompare, (int)textureCount);

  modes = *mesh;
  if (thispass->alphaMode.autoAlphaMode)
  {
    iTextureHandle* tex = 0;
    if (thispass->alphaMode.autoModeTexture != csInvalidStringID)
    {
      if (thispass->alphaMode.autoModeTexture < (CS::StringIDValue)stack.GetSize ())
      {
        csShaderVariable* var = 0;
        var = csGetShaderVariableFromStack (stack, thispass->alphaMode.autoModeTexture);
        if (var)
          var->GetValue (tex);
      }
    }
    if (tex != 0)
      modes.alphaType = tex->GetAlphaType ();
    else
      modes.alphaType = csAlphaMode::alphaNone;
  }
  else
    modes.alphaType = thispass->alphaMode.alphaType;
  // Override mixmode, if requested
  uint originalMixMode = modes.mixmode;
  if ((thispass->mixMode & CS_MIXMODE_TYPE_MASK) != CS_FX_MESH)
    modes.mixmode = thispass->mixMode;
  modes.alphaToCoverage = thispass->alphaToCoverage;
  if ((thispass->atcMixMode & CS_MIXMODE_TYPE_MASK) != CS_FX_MESH)
    modes.atcMixmode = thispass->atcMixMode;
  
  modes.alphaTest = thispass->alphaTestOpt;

  modes.cullMode = thispass->cullMode;
  
  /* Grab mixmode alpha from _original_ mixmode.
     This is more sensible than possibly taking it from the shader, which
     would mean hardcoding the alpha in the shader. Not only are there other
     ways to do that, taking the alpha from the original mixmode allows to
     quickly make a mesh semi-transparent or such. */
  float alpha = 1.0f;
  if (originalMixMode & CS_FX_MASK_ALPHA)
  {
    alpha = 1.0f - (float)(originalMixMode & CS_FX_MASK_ALPHA) / 255.0f;
  }
  parent->shadermgr->GetVariableAdd (
    parent->compiler->string_mixmode_alpha)->SetValue (alpha);
  modes.zoffset = thispass->zoffset;

  if(thispass->program) thispass->program->SetupState (mesh, modes, stack);

  // pseudo instancing setup
  SetupInstances (modes, thispass, stack);

  return true;
}

bool csXMLShaderTech::TeardownPass ()
{
  ShaderPass* thispass = &passes[currentPass];

  if(thispass->vproc) thispass->vproc->ResetState ();
  if(thispass->program) thispass->program->ResetState ();

  return true;
}

void csXMLShaderTech::SetupInstances (csRenderMeshModes& modes, 
                                      ShaderPass *thispass,
                                      const csShaderVariableStack& stack)
{
  const csArray<ShaderPass::InstanceMapping>& instances_binds =
    thispass->instances_binds;
  if (instances_binds.GetSize() == 0)
  {
    /* No instancing binds, disable instances */
    modes.doInstancing = false;
    return;
  }

  size_t numVars = 0;
  size_t numSVs = 0;
  size_t numInsts = 0;
  GetInstParamsTargets().Empty ();
  GetInstOuterVars().Empty ();
  
  {
    csShaderVariable* instsNumSV = csGetShaderVariableFromStack (stack, thispass->instancesNumVar);
    if (instsNumSV)
    {
      int n;
      instsNumSV->GetValue (n);
      numInsts = n;
    }
  }

  // Pass one: collect number of instances, SVs per instance
  for (size_t i = 0; i < instances_binds.GetSize(); i++)
  {
    csShaderVariable* var = 0;
    var = csGetShaderVariableFromStack (stack, instances_binds[i].variable);
    GetInstOuterVars().Push (var);
    if (var == 0) continue;
    
    size_t varElems = (size_t)~0;
    if (var->GetType() == csShaderVariable::RENDERBUFFER)
    {
      // Buffer variable: use buffer as parameter data
      iRenderBuffer* buf;
      var->GetValue (buf);
      varElems = buf ? buf->GetElementCount() : 0;
    }
    else if (var->GetType() == csShaderVariable::ARRAY)
    {
      // Array variable: use array elements as parameter data
      varElems = var->GetArraySize();
      numSVs++;
    }
    else
      // SVs of other types are passed directly to instancing
      numSVs++;
    if (varElems != (size_t)~0)
    {
      if (numInsts == 0)
	// First SV array seen, use length as initial instance count
	numInsts = varElems;
      else
	// Use lowest of all array sizes as instance count
	numInsts = csMin (varElems, numInsts);
      // An empty array means no instances, so we can break out
      if (varElems == 0)
      {
	numVars = 0;
	break;
      }
    }
    
    GetInstParamsTargets().Push (instances_binds[i].destination);
    numVars++;
  }
  
  // Pass two: fill arrays
  GetInstParams().SetSize (numInsts * numSVs);
  GetInstParamPtrs().SetSize (numInsts, nullptr);
  GetInstParamBuffers().SetSize (numVars - numSVs, nullptr);
  
  size_t bindNum = 0;	// Counts actual parameter
  size_t svNum = 0;	// Counts parameter provided as SVs
  for (size_t i = 0; i < instances_binds.GetSize(); i++)
  {
    csShaderVariable* var = GetInstOuterVars()[i];
    if (var == 0) continue;

    if (var->GetType() == csShaderVariable::RENDERBUFFER)
    {
      iRenderBuffer* buf;
      var->GetValue (buf);
      GetInstParamBuffers()[bindNum] = buf;
    }
    else
    {
      csShaderVariable** bindPtr = GetInstParams().GetArray() + svNum;
      svNum++;
      if (var->GetType() == csShaderVariable::ARRAY)
      {
	/* If a shader var contains an array, the elements of the array are used as
	   the parameters for each instance.
	 */
	size_t varElems = var->GetArraySize();
	for (size_t instNum = 0; instNum < numInsts; instNum++)
	{
	  size_t n = csMin (varElems, instNum);
	  *bindPtr = var->GetArrayElement (n);
	  bindPtr += numSVs;
	}
      }
      else
      {
	/* Otherwise, the shader var is replicated across all instances. */
	for (size_t instNum = 0; instNum < numInsts; instNum++)
	{
	  *bindPtr = var;
	  bindPtr += numSVs;
	}
      }
    }
    bindNum++;
  }

  if (numSVs > 0)
  {
    for (size_t i = 0; i < numInsts; i++)
    {
      GetInstParamPtrs()[i] = GetInstParams().GetArray() + numSVs*i;
    }
  }

  modes.instParamNum = numVars;
  modes.instParamsTargets = GetInstParamsTargets().GetArray(); 
  modes.instanceNum = numInsts;
  modes.instParams = (numSVs > 0) ? GetInstParamPtrs().GetArray() : nullptr;
  modes.instParamBuffers = (numSVs < numVars) ? GetInstParamBuffers().GetArray() : nullptr;
  modes.doInstancing = true;
}

void csXMLShaderTech::GetUsedShaderVars (csBitArray& bits, uint userFlags) const
{
  for (size_t pass = 0; pass < passesCount; pass++)
  {
    ShaderPass* thispass = &passes[pass];

    if (((userFlags & iShader::svuVProc) != 0) && thispass->vproc)
    {
      thispass->vproc->GetUsedShaderVars (bits);
    }

    if ((userFlags & iShader::svuBuffers) != 0)
    {
      for (size_t i = 0; i < thispass->custommapping_attrib.GetSize (); i++)
      {
	CS::ShaderVarStringID id = thispass->custommapping_id[i];
	if ((id != CS::InvalidShaderVarStringID) && (bits.GetSize() > id))
	{
	  bits.SetBit (id);
	}
      }
    }
    if ((userFlags & iShader::svuTextures) != 0)
    {
      for (size_t j = 0; j < thispass->textures.GetSize(); j++)
      {
	CS::ShaderVarStringID id = thispass->textures[j].tex.id;
	if ((id != CS::InvalidShaderVarStringID) && (bits.GetSize() > id))
	{
	  bits.SetBit (id);
	}
	id = thispass->textures[j].fallback.id;
	if ((id != CS::InvalidShaderVarStringID) && (bits.GetSize() > id))
	{
	  bits.SetBit (id);
	}
      }
    }

    // TODO: may request vars from a non-flagged shader
    if (thispass->program &&
        (((userFlags & iShader::svuVP) != 0) ||
         ((userFlags & iShader::svuFP) != 0)))
    {
      thispass->program->GetUsedShaderVars (bits);
    }
  }
}

int csXMLShaderTech::GetPassNumber (ShaderPass* pass)
{
  if ((pass >= passes) && (pass < passes + passesCount))
  {
    return pass - passes;
  }
  else
    return -1;
}

void csXMLShaderTech::SetFailReason (const char* reason, ...)
{
  va_list args;
  va_start (args, reason);
  fail_reason.FormatV (reason, args);
  va_end (args);
}

}
CS_PLUGIN_NAMESPACE_END(XMLShader)
