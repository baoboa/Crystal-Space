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
<![CDATA[

#ifndef _LIGHTFUNCS_GLSLI_
#define _LIGHTFUNCS_GLSLI_

float Attenuation_Linear (float d, float invLightRadius)
{
  return saturate (1.0 - d * invLightRadius);
}

float Attenuation_Inverse (float d)
{
  return 1.0/d;
}

float Attenuation_Realistic (float d)
{
  return 1.0/(d*d);
}

float Attenuation_CLQ (float d, float3 coeff)
{
  return 1.0/dot (float3 (1.0, d, d*d), coeff);
}

float Light_Spot (float3 surfToLight, float3 lightDir, float falloffInner, float falloffOuter)
{
  return smoothstep (falloffOuter, falloffInner, dot (surfToLight, lightDir));
}

float Attenuate (float d, float4 coeff)
{
  float attn;
  float invAttnRadius = coeff.w;
  if (invAttnRadius > 0.0)
    attn = Attenuation_Linear(d, invAttnRadius);
  else
    attn = Attenuation_CLQ(d, coeff.xyz);
  return attn;
}

// Light view-space position
uniform float3 lightPosition;
// Light view-space direction
uniform float3 lightDirection;
// Diffuse color
uniform float3 lightDiffuse;
// Specular color
uniform float3 lightSpecular;
// Attenuation vector (XYZ are CLQ coefficients; W is light radius)
uniform float4 lightAttenuation;
// Cosine of inner falloff angle
uniform float lightInnerFalloff;
// Cosine of outerr falloff angle
uniform float lightOuterFalloff;

#endif // _LIGHTFUNCS_GLSLI_
 
]]>
</include>
