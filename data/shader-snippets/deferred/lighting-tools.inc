<?xml version="1.0" encoding="utf-8" ?>
<!--
  Copyright (C) 2008 by Frank Richter
                2012 by Matthieu Kraus

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
    <variablemap variable="light position view" 
		 destination="lightPosition" />
    <variablemap variable="light direction view" 
		 destination="lightDirection" />
    <variablemap variable="light diffuse" 
		 destination="lightDiffuse" />
    <variablemap variable="light specular" 
		 destination="lightSpecular" />
    <variablemap variable="light attenuation" 
		 destination="lightAttenuation" />
    <variablemap variable="light inner falloff" 
		 destination="lightInnerFalloff" />
    <variablemap variable="light outer falloff" 
		 destination="lightOuterFalloff" />
  <?Endtemplate?>
  
  <?SEndIf?>
</include>
