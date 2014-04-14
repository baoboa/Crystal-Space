<!--
  Copyright (C) 2008 by Frank Richter

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
-->
<include>
  <?SIfNDef __SHADERCOND_TOOLS_INC__ ?>
  <?Define __SHADERCOND_TOOLS_INC__ ?>
  
  <?Template Lighting_Variablemaps?>
    <!-- Cg shader plugin magic makes arrays work -->
    <variablemap variable="light count" 
		 destination="lightProps_count" />
    <variablemap variable="light position world" 
		 destination="lightProps_posWorld" />
    <variablemap variable="light diffuse" 
		 destination="lightProps_colorDiffuse" />
    <variablemap variable="light specular" 
		 destination="lightProps_colorSpecular" />
    <variablemap variable="light attenuation" 
		 destination="lightProps_attenuationVec" />
    <variablemap variable="light inner falloff" 
		 destination="lightProps_falloffInner" />
    <variablemap variable="light outer falloff" 
		 destination="lightProps_falloffOuter" />
    <variablemap variable="light transform world" 
		 destination="lightProps_transform" />
    <variablemap variable="light transform world inverse" 
		 destination="lightProps_transformInv" />
  <?Endtemplate?>
  
  <?Template Lighting_ShadowMap_Variablemaps?>
    <variablemap variable="light shadow map projection" 
		 destination="lightPropsSM_shadowMapTF" />
    <variablemap variable="light shadow map pixel size" 
		 destination="lightPropsSM_shadowMapPixels" />
    <variablemap variable="light shadow map unscale" 
		 destination="lightPropsSM_shadowMapUnscale" />
  <?Endtemplate?>
  
<!--  
  Copying properties for opacity shadow maps
-->
  <?Template Lighting_OpacityMap_Variablemaps?> 
    <variablemap variable="light shadow map projection" 
		 destination="lightPropsOM_opacityMapTF" />
    <variablemap variable="light numSplits" 
		 destination="lightPropsOM_opacityMapNumSplits" />		       
    <variablemap variable="light shadow map size" 
		 destination="lightPropsOM_size" />		      
  <?Endtemplate?>  
  
  <?SEndIf?>
</include>
