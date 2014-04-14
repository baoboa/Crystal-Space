/*
  Copyright (C) 2011 by Jorrit Tyberghein
  Copyright (C) 2005 by Christopher Nelson

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"
#include "csgeom/box.h"
#include "cstool/pen.h"
#include "csutil/stringarray.h"
#include "ivideo/fontserv.h"
#include "ivideo/graph2d.h"

#define MESH_TYPE(a) ((flags & CS_PEN_FILL) ? a : pen_width>1 ? CS_MESHTYPE_QUADS : CS_MESHTYPE_LINESTRIP)

//--------------------------------------------------------------------

bool csPenCache::IsMergeable (csSimpleRenderMesh* mesh1,
    csSimpleRenderMesh* mesh2)
{
  if (mesh1->meshtype != mesh2->meshtype) return false;
  if (mesh1->texture != mesh2->texture) return false;
  if (mesh1->shader != mesh2->shader) return false;
  if (mesh1->dynDomain != mesh2->dynDomain) return false;
  if (mesh1->alphaType.autoAlphaMode != mesh2->alphaType.autoAlphaMode) return false;
  if (mesh1->alphaType.alphaType != mesh2->alphaType.alphaType) return false;
  if (mesh1->z_buf_mode != mesh2->z_buf_mode) return false;
  if (mesh1->mixmode != mesh2->mixmode) return false;
  if (mesh1->object2world.GetO2T () != mesh2->object2world.GetO2T ()) return false;
  if (mesh1->object2world.GetO2TTranslation () != mesh2->object2world.GetO2TTranslation ()) return false;
  return true;
}

void csPenCache::MergeMesh (csSimpleRenderMesh* mesh)
{
  size_t idx = meshes.GetSize ()-1;
  csSimpleRenderMesh& last = meshes[idx].mesh;
  last.meshtype = mesh->meshtype;
  last.texture = mesh->texture;
  last.shader = mesh->shader;
  last.dynDomain = mesh->dynDomain;
  last.alphaType = mesh->alphaType;
  last.z_buf_mode = mesh->z_buf_mode;
  last.mixmode = mesh->mixmode;
  last.object2world = mesh->object2world;

  size_t vtoffset = meshes[idx].vertexCount;
  meshes[idx].vertexCount += mesh->vertexCount;
  for (size_t i = 0 ; i < mesh->vertexCount ; i++)
  {
    vertices.Push (mesh->vertices[i]);
    if (mesh->texcoords) texcoords.Push (mesh->texcoords[i]);
    else texcoords.Push (csVector2 (0, 0));
    if (mesh->colors) colors.Push (mesh->colors[i]);
    else colors.Push (csVector4 (1.0f, 1.0f, 1.0f, 1.0f));
  }
  meshes[idx].indexCount += mesh->indexCount;
  for (size_t i = 0 ; i < mesh->indexCount ; i++)
    vertexIndices.Push (mesh->indices[i] + vtoffset);
}

void csPenCache::Render (iGraphics3D* g3d)
{
  for (size_t i = 0 ; i < meshes.GetSize () ; i++)
  {
    csSimpleRenderMesh& mesh = meshes[i].mesh;

    mesh.vertices = vertices.GetArray () + meshes[i].offsetVertices;
    mesh.vertexCount = meshes[i].vertexCount;
    mesh.colors = colors.GetArray () + meshes[i].offsetVertices;
    mesh.texcoords = texcoords.GetArray () + meshes[i].offsetVertices;

    mesh.indices = vertexIndices.GetArray () + meshes[i].offsetIndices;
    mesh.indexCount = meshes[i].indexCount;

    g3d->DrawSimpleMesh (mesh, meshes[i].flags);
  }
}

void csPenCache::PushMesh (csSimpleRenderMesh* mesh, csSimpleMeshFlags flags)
{
  if (meshes.GetSize () <= 0 ||
      meshes[meshes.GetSize ()-1].flags != flags ||
      !IsMergeable (&meshes[meshes.GetSize ()-1].mesh, mesh))
  {
    PRMesh prm;
    prm.flags = flags;
    prm.offsetVertices = vertices.GetSize ();
    prm.offsetIndices = vertexIndices.GetSize ();
    meshes.Push (prm);
  }
  MergeMesh (mesh);
}

void csPenCache::Clear ()
{
  meshes.DeleteAll ();
  vertices.DeleteAll ();
  vertexIndices.DeleteAll ();
  colors.DeleteAll ();
  texcoords.DeleteAll ();
}

void csPenCache::SetTransform (const csReversibleTransform& trans)
{
  for (size_t i = 0 ; i < meshes.GetSize () ; i++)
    meshes[i].mesh.object2world = trans;
}

//--------------------------------------------------------------------

csPen::csPen (iGraphics2D *_g2d, iGraphics3D *_g3d) : g3d (_g3d), g2d(_g2d), pen_width(1.0), flags(0), gen_tex_coords(false)
{
  mesh.object2world.Identity();
  mesh.mixmode = CS_FX_ALPHA;
  tt.Set(0,0,0);
  penCache = 0;
}

csPen::~csPen ()
{
}

void csPen::Start ()
{
  poly.MakeEmpty();
  poly_idx.MakeEmpty();
  colors.SetSize (0);
  texcoords.SetSize (0);
  line_points.SetSize (0);
  gen_tex_coords=false;
}

void csPen::AddVertex (float x, float y, bool force_add)
{
  if (force_add || (flags & CS_PEN_FILL) || !(pen_width>1))
  {
    poly_idx.AddVertex((int)poly.AddVertex(x,y,0));
    colors.Push(color);

    // Generate texture coordinates.
    if (gen_tex_coords && (flags & CS_PEN_TEXTURE_ONLY))
      AddTexCoord(x / sh_w, y / sh_h);
  }
  else
  {
    point p = {x,y};

    if (line_points.GetSize ())
      AddThickPoints(line_points.Top().x, line_points.Top().y, x, y);

    line_points.Push(p);
  }
}

void csPen::AddTexCoord(float x, float y)
{
  texcoords.Push(csVector2(x,y));
}

void csPen::SetupMesh ()
{
  mesh.vertices = poly.GetVertices ();
  mesh.vertexCount = (uint)poly.GetVertexCount ();

  mesh.indices = (uint *)poly_idx.GetVertexIndices ();
  mesh.indexCount = (uint)poly_idx.GetVertexCount ();

  mesh.colors = colors.GetArray ();
  mesh.texcoords = texcoords.GetArray();
  if (flags & CS_PEN_TEXTURE_ONLY)
  {
    mesh.texture=tex;
  }
  else
  {
    mesh.texture=0;
  }

  //mesh.alphaType = alphaSmooth;
  //mesh.mixmode = CS_FX_COPY | CS_FX_ALPHA; // CS_FX_FLAT
}

void csPen::DrawMesh (csRenderMeshType mesh_type)
{
  mesh.meshtype = mesh_type;
  if (penCache)
    penCache->PushMesh (&mesh, csSimpleMeshScreenspace);
  else
    g3d->DrawSimpleMesh (mesh, csSimpleMeshScreenspace);
}

void csPen::SetMixMode (uint mode)
{
  mesh.mixmode = mode;
}

void csPen::SetAutoTexture(float w, float h)
{
  sh_w = w;
  sh_h = h;
  gen_tex_coords=true;
}

void csPen::AddThickPoints(float fx1, float fy1, float fx2, float fy2)
{
  float angle = atan2(fy2-fy1, fx2-fx1);

  float a1 = angle - HALF_PI;
  float ca1 = cos(a1)*pen_width, sa1 = sin(a1)*pen_width;
  bool first = line_points.GetSize ()<2;

// 	AddVertex(fx1+ca1, fy1+sa1, true);
// 	AddVertex(fx2+ca1, fy2+sa1, true);
//
// 	AddVertex(fx2-ca1, fy2-sa1, true);
// 	AddVertex(fx1-ca1, fy1-sa1, true);
//

  if (first) AddVertex(fx1+ca1, fy1+sa1, true);
  else	   AddVertex(last[0].x, last[0].y, true);

  AddVertex(fx2+ca1, fy2+sa1, true);
  AddVertex(fx2-ca1, fy2-sa1, true);

  if (first) AddVertex(fx1-ca1, fy1-sa1, true);
  else	   AddVertex(last[1].x, last[1].y, true);

  last[0].x=fx2+ca1; last[0].y=fy2+sa1;
  last[1].x=fx2-ca1; last[1].y=fy2-sa1;
}

void csPen::SetFlag (uint flag)
{
  flags |= flag;
}


void csPen::ClearFlag (uint flag)
{
  flags &= (~flag);
}

void csPen::SetColor (float r, float g, float b, float a)
{
  color.x=r;
  color.y=g;
  color.z=b;
  color.w=a;
}

void csPen::SetColor(const csColor4 &c)
{
  color.x=c.red;
  color.y=c.green;
  color.z=c.blue;
  color.w=c.alpha;
}

void csPen::SetTexture (iTextureHandle* _tex)
{
  tex=_tex;
}

void csPen::SwapColors()
{
  csVector4 tmp;

  tmp=color;
  color=alt_color;
  alt_color=tmp;
}

void csPen::SetPenWidth(float width)
{
  pen_width=width;
}

void csPen::ClearTransform()
{
  mesh.object2world.Identity();
  tt = csVector3(0,0,0);
}

void csPen::PushTransform()
{
  transforms.Push(mesh.object2world);
  translations.Push(tt);
}

void csPen::PopTransform()
{
  ClearTransform();

  mesh.object2world*=transforms.Top();
  transforms.Pop();

  tt=translations.Top();
  translations.Pop();
}

void csPen::SetOrigin(const csVector3 &o)
{
  mesh.object2world.SetOrigin(o);
}

void csPen::SetTransform (const csReversibleTransform& trans)
{
  mesh.object2world = trans;
}

void csPen::Translate (const csVector3 &t)
{
  csTransform tr;

  tr.Translate(t);
  mesh.object2world*=tr;

  tt+=t;
}

void csPen::Rotate (const float &a)
{
  csZRotMatrix3 rm(a);
  csTransform tr(rm, csVector3(0));
  mesh.object2world*=tr;
}

void csPen::DrawThickLine (int x1, int y1, int x2, int y2)
{
  Start();

  AddThickPoints(x1,y1,x2,y2);

  SetupMesh();
  DrawMesh(CS_MESHTYPE_QUADS);
}

void csPen::DrawThickLines (const csArray<csPenCoordinatePair>& pairs)
{
  Start();

  for (size_t i = 0 ; i < pairs.GetSize () ; i++)
    AddThickPoints(pairs[i].c1.x,pairs[i].c1.y,pairs[i].c2.x,pairs[i].c2.y);

  SetupMesh();
  DrawMesh(CS_MESHTYPE_QUADS);
}

bool csPen::ClipLine (int& x1, int& y1, int& x2, int& y2)
{
  if (x1 == x2 && y1 == y2) return false;
  if (x1 < 0 && x2 < 0) return false;
  if (y1 < 0 && y2 < 0) return false;
  int sw = g2d->GetWidth ();
  if (x1 >= sw && x2 >= sw) return false;
  int sh = g2d->GetHeight ();
  if (y1 >= sh && y2 >= sh) return false;

  float dist;

  if (x1 < 0 || x2 < 0)
  {
    dist = - float (x1) / float (x2 - x1);
    int ix = 0;
    int iy = y1 + dist * (y2 - y1);
    if (x1 < 0) { x1 = ix; y1 = iy; }
    else { x2 = ix; y2 = iy; }
  }
  if (x1 >= sw || x2 >= sw)
  {
    dist = float (- x1 + sw - 1) / float (x2 - x1);
    int ix = sw-1;
    int iy = y1 + dist * (y2 - y1);
    if (x1 >= sw) { x1 = ix; y1 = iy; }
    else { x2 = ix; y2 = iy; }
  }
  if (y1 < 0 || y2 < 0)
  {
    dist = float (y1) / float (y1 - y2);
    int ix = x1 + dist * (x2 - x1);
    int iy = 0;
    if (y1 < 0) { x1 = ix; y1 = iy; }
    else { x2 = ix; y2 = iy; }
  }
  if (y1 >= sh || y2 >= sh)
  {
    dist = float (y1 - sh + 1) / float (y1 - y2);
    int ix = x1 + dist * (x2 - x1);
    int iy = sh-1;
    if (y1 >= sh) { x1 = ix; y1 = iy; }
    else { x2 = ix; y2 = iy; }
  }

  return true;
}

void csPen::DrawLine (int x1, int y1, int x2, int y2)
{
  if (!ClipLine (x1, y1, x2, y2)) return;
  if (pen_width>1) { DrawThickLine(x1,y1,x2,y2); return; }

  Start ();
  AddVertex (x1,y1);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  AddVertex (x2,y2);

  SetupMesh ();
  DrawMesh (CS_MESHTYPE_LINES);
}

void csPen::DrawLines (const csArray<csPenCoordinatePair>& pairs)
{
  if (pen_width>1) { DrawThickLines (pairs); return; }

  Start ();
  for (size_t i = 0 ; i < pairs.GetSize () ; i++)
  {
    AddVertex (pairs[i].c1.x,pairs[i].c1.y);

    if (flags & CS_PEN_SWAPCOLORS) SwapColors();

    AddVertex (pairs[i].c2.x,pairs[i].c2.y);

    if (flags & CS_PEN_SWAPCOLORS) SwapColors();
  }

  SetupMesh ();
  DrawMesh (CS_MESHTYPE_LINES);
}

void csPen::DrawPoint (int x1, int y1)
{
  Start ();
  AddVertex (x1,y1);

  SetupMesh ();
  DrawMesh (CS_MESHTYPE_POINTS);
}

/** Draws a rectangle. */
void csPen::DrawRect (int x1, int y1, int x2, int y2)
{
  Start ();
  SetAutoTexture(x2-x1, y2-y1);

  AddVertex (x1, y1);
  AddVertex (x2, y1);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  AddVertex (x2, y2);
  AddVertex (x1, y2);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  if (!(flags & CS_PEN_FILL)) AddVertex (x1, y1);

  SetupMesh ();
  DrawMesh (MESH_TYPE(CS_MESHTYPE_QUADS));
}

/** Draws a mitered rectangle. The miter value should be between 0.0 and 1.0, and determines how
* much of the corner is mitered off and beveled. */
void csPen::DrawMiteredRect (int x1, int y1, int x2, int y2,
                             uint miter)
{
  if (miter == 0)
  {
    DrawRect (x1,y1,x2,y2);
    return;
  }


  uint width = x2-x1;
  uint height = y2-y1;

  uint center_x = x1+(width>>1);
  uint center_y = y1+(height>>1);

  uint y_miter = miter;
  uint x_miter = miter;

  float ym_1 = y1 + y_miter;
  float ym_2 = y2 - y_miter;
  float xm_1 = x1 + x_miter;
  float xm_2 = x2 - x_miter;

  Start ();
  SetAutoTexture(x2-x1, y2-y1);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();
  if (flags & CS_PEN_FILL)AddVertex(center_x, center_y);

  AddVertex (x1, ym_2);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  AddVertex (x1, ym_1);
  AddVertex (xm_1, y1);
  AddVertex (xm_2, y1);
  AddVertex (x2, ym_1);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  AddVertex (x2, ym_2);
  AddVertex (xm_2, y2);
  AddVertex (xm_1, y2);
  AddVertex (x1, ym_2);

  SetupMesh ();
  DrawMesh (MESH_TYPE(CS_MESHTYPE_TRIANGLEFAN));
}

/** Draws a rounded rectangle. The roundness value should be between 0.0 and 1.0, and determines how
  * much of the corner is rounded off. */
void csPen::DrawRoundedRect (int x1, int y1, int x2, int y2,
                             uint roundness)
{
  if (roundness == 0)
  {
    DrawRect (x1,y1,x2,y2);
    return;
  }

  float width = x2-x1;
  float height = y2-y1;

  float center_x = x1+(width/2);
  float center_y = y1+(height/2);


  float y_round = roundness;
  float x_round = roundness;
  float delta = 0.0384f;

  Start();
  SetAutoTexture(width, height);

  float angle;

  if ((flags & CS_PEN_FILL))AddVertex(center_x, center_y);

  for(angle=(HALF_PI)*3.0f; angle>PI; angle-=delta)
  {
    AddVertex (x1+x_round+(cosf (angle)*x_round), y2-y_round-(sinf (angle)*y_round));
  }

  AddVertex (x1, y2-y_round);
  AddVertex (x1, y1+y_round);

  for(angle=PI; angle>HALF_PI; angle-=delta)
  {
	  AddVertex (x1+x_round+(cosf (angle)*x_round), y1+y_round-(sinf (angle)*y_round));
  }

  AddVertex (x1+x_round, y1);
  AddVertex (x2-x_round, y1);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  for(angle=HALF_PI; angle>0; angle-=delta)
  {
    AddVertex (x2-x_round+(cosf (angle)*x_round), y1+y_round-(sinf (angle)*y_round));
  }

  AddVertex (x2, y1+y_round);
  AddVertex (x2, y2-y_round);

  for(angle=TWO_PI; angle>HALF_PI*3.0; angle-=delta)
  {
    AddVertex (x2-x_round+(cosf (angle)*x_round), y2-y_round-(sinf (angle)*y_round));
  }

  AddVertex (x2-x_round, y2);
  AddVertex (x1+x_round, y2);

  if (flags & CS_PEN_SWAPCOLORS) SwapColors();

  SetupMesh ();
  DrawMesh (MESH_TYPE(CS_MESHTYPE_TRIANGLEFAN));
}

/**
 * Draws an elliptical arc from start angle to end angle.  Angle must be
 * specified in radians. The arc will be made to fit in the given box.
 * If you want a circular arc, make sure the box is a square.  If you
 * want a full circle or ellipse, specify 0 as the start angle and 2*PI
 * as the end angle.
 */
void csPen::DrawArc (int x1, int y1, int x2, int y2,
    float start_angle, float end_angle)
{
  // Check to make sure that the arc is not in a negative box.
  if (x2<x1) { x2^=x1; x1^=x2; x2^=x1; }
  if (y2<y1) { y2^=y1; y1^=y2; y2^=y1; }

  // If start angle and end_angle are too close, abort.
  if (fabs(end_angle-start_angle) < 0.0001) return;

  float width = x2-x1;
  float height = y2-y1;

  if (width==0 || height==0) return;

  float x_radius = width/2;
  float y_radius = height/2;

  float center_x = x1+(x_radius);
  float center_y = y1+(y_radius);

  // Set the delta to be two degrees.
  float delta = 0.0384f;
  float angle;

  Start();
  SetAutoTexture(width, height);

  if ((flags & CS_PEN_FILL)) AddVertex(center_x, center_y);

  for(angle=start_angle; angle<=end_angle; angle+=delta)
  {
    AddVertex(center_x+(cos(angle)*x_radius), center_y+(sin(angle)*y_radius));
  }

  SetupMesh ();
  DrawMesh (MESH_TYPE(CS_MESHTYPE_TRIANGLEFAN));
}

void csPen::DrawTriangle (int x1, int y1, int x2, int y2, int x3, int y3)
{
  Start();

  AddVertex(x1, y1); AddTexCoord(0,0);
  AddVertex(x2, y2); AddTexCoord(0,1);
  AddVertex(x3, y3); AddTexCoord(1,1);

  if (!(flags & CS_PEN_FILL)) AddVertex(x1, y1);

  SetupMesh ();
  DrawMesh (MESH_TYPE(CS_MESHTYPE_TRIANGLES));
}

void csPen::Write(iFont *font, int x1, int y1, const char *text)
{
  if (font==0) return;

  int the_color = g2d->FindRGB(static_cast<int>(color.x*255),
		 	       static_cast<int>(color.y*255),
			       static_cast<int>(color.z*255),
			       static_cast<int>(color.w*255));

  csVector3 pos(x1,y1,0);

  pos += tt;

  g2d->Write(font, (int)pos.x, (int)pos.y, the_color, -1, text);
}

void csPen::WriteLines (iFont *font, int x1, int y1, const csStringArray& lines)
{
  if (font==0) return;

  int the_color = g2d->FindRGB(static_cast<int>(color.x*255),
		 	       static_cast<int>(color.y*255),
			       static_cast<int>(color.z*255),
			       static_cast<int>(color.w*255));

  csVector3 pos(x1,y1,0);

  pos += tt;

  int txtHeight = font->GetTextHeight ();

  for (size_t i = 0 ; i < lines.GetSize () ; i++)
  {
    g2d->Write(font, (int)pos.x, (int)pos.y, the_color, -1, lines.Get (i));
    pos.y += txtHeight;
  }
}

void csPen::WriteBoxed (iFont *font, int x1, int y1, int x2, int y2,
    uint h_align, uint v_align, const char *text)
{
  if (font==0) return;

  uint x, y;
  int w, h;

  // Get the maximum dimensions of the text.
  font->GetDimensions(text, w, h);

  // Figure out the correct starting point in the box for horizontal alignment.
  switch(h_align)
  {
    case CS_PEN_TA_LEFT:
    default:
      x=x1;
    break;

    case CS_PEN_TA_RIGHT:
      x=x2-w;
    break;

    case CS_PEN_TA_CENTER:
      x=x1+((x2-x1)>>1) - (w>>1);
    break;
  }

  // Figure out the correct starting point in the box for vertical alignment.
  switch(v_align)
  {
    case CS_PEN_TA_TOP:
    default:
      y=y1;
    break;

    case CS_PEN_TA_BOT:
      y=y2-h;
    break;

    case CS_PEN_TA_CENTER:
      y=y1+((y2-y1)>>1) - (h>>1);
    break;
  }

  Write(font, x, y, text);
}

void csPen::WriteLinesBoxed(iFont *font, int x1, int y1, int x2, int y2,
    uint h_align, uint v_align, const csStringArray& lines)
{
  if (font==0) return;

  uint x, y;
  int w, h;

  // Get the maximum dimensions of the text.
  int txtHeight = font->GetTextHeight ();
  h = txtHeight * lines.GetSize ();
  w = 0;
  for (size_t i = 0 ; i < lines.GetSize () ; i++)
  {
    int ww, hh;
    font->GetDimensions(lines.Get (i), ww, hh);
    if (ww > w) w = ww;
  }

  // Figure out the correct starting point in the box for horizontal alignment.
  switch(h_align)
  {
    case CS_PEN_TA_LEFT:
    default:
      x=x1;
      break;
    case CS_PEN_TA_RIGHT:
      x=x2-w;
      break;
    case CS_PEN_TA_CENTER:
      x=x1+((x2-x1)>>1) - (w>>1);
      break;
  }

  // Figure out the correct starting point in the box for vertical alignment.
  switch(v_align)
  {
    case CS_PEN_TA_TOP:
    default:
      y=y1;
      break;
    case CS_PEN_TA_BOT:
      y=y2-h;
      break;
    case CS_PEN_TA_CENTER:
      y=y1+((y2-y1)>>1) - (h>>1);
      break;
  }

  for (size_t i = 0 ; i < lines.GetSize () ; i++)
  {
    int xx = x;
    if (h_align)
    {
      int ww, hh;
      font->GetDimensions(lines.Get (i), ww, hh);
      xx = x1+((x2-x1)>>1) - (ww>>1);
    }
    Write(font, xx, y, lines.Get (i));
    y += txtHeight;
  }
}

// --------------------------------------------------------------------------

csPen3D::csPen3D (iGraphics2D *_g2d, iGraphics3D *_g3d) :
  g3d (_g3d), g2d(_g2d)
{
  mesh.object2world.Identity();
  mesh.mixmode = CS_FX_ALPHA;
  penCache = 0;
}

csPen3D::~csPen3D ()
{
}

void csPen3D::SetMixMode (uint mode)
{
  mesh.mixmode = mode;
}

void csPen3D::SetColor (float r, float g, float b, float a)
{
  color.x = r;
  color.y = g;
  color.z = b;
  color.w = a;
}

void csPen3D::SetColor (const csColor4 &c)
{
  color.x = c.red;
  color.y = c.green;
  color.z = c.blue;
  color.w = c.alpha;
}

void csPen3D::Start ()
{
  poly.MakeEmpty();
  poly_idx.MakeEmpty();
  colors.SetSize (0);
  texcoords.SetSize (0);
}

void csPen3D::AddVertex (const csVector3& v)
{
  poly_idx.AddVertex((int)poly.AddVertex(local2object * v));
  colors.Push(color);
}

void csPen3D::SetupMesh ()
{
  mesh.vertices = poly.GetVertices ();
  mesh.vertexCount = (uint)poly.GetVertexCount ();

  mesh.indices = (uint *)poly_idx.GetVertexIndices ();
  mesh.indexCount = (uint)poly_idx.GetVertexCount ();

  mesh.colors = colors.GetArray ();
  mesh.texcoords = texcoords.GetArray();
}

void csPen3D::DrawMesh (csRenderMeshType mesh_type)
{
  mesh.meshtype = mesh_type;
  if (penCache)
    penCache->PushMesh (&mesh, (csSimpleMeshFlags)0);
  else
    g3d->DrawSimpleMesh (mesh, (csSimpleMeshFlags)0);
}

void csPen3D::SetTransform (const csReversibleTransform& trans)
{
  mesh.object2world = trans;
}

void csPen3D::DrawLine (const csVector3& v1, const csVector3& v2)
{
  Start ();
  AddVertex (v1);
  AddVertex (v2);

  SetupMesh ();
  DrawMesh (CS_MESHTYPE_LINES);
}

void csPen3D::DrawLines (const csArray<csPen3DCoordinatePair>& pairs)
{
  Start ();
  for (size_t i = 0 ; i < pairs.GetSize () ; i++)
  {
    AddVertex (pairs[i].c1);
    AddVertex (pairs[i].c2);
  }

  SetupMesh ();
  DrawMesh (CS_MESHTYPE_LINES);
}

void csPen3D::DrawBox (const csBox3& b)
{
  csVector3 xyz = b.GetCorner (CS_BOX_CORNER_xyz);
  csVector3 Xyz = b.GetCorner (CS_BOX_CORNER_Xyz);
  csVector3 xYz = b.GetCorner (CS_BOX_CORNER_xYz);
  csVector3 xyZ = b.GetCorner (CS_BOX_CORNER_xyZ);
  csVector3 XYz = b.GetCorner (CS_BOX_CORNER_XYz);
  csVector3 XyZ = b.GetCorner (CS_BOX_CORNER_XyZ);
  csVector3 xYZ = b.GetCorner (CS_BOX_CORNER_xYZ);
  csVector3 XYZ = b.GetCorner (CS_BOX_CORNER_XYZ);
  csArray<csPen3DCoordinatePair> pairs;
  pairs.Push (csPen3DCoordinatePair (xyz, Xyz));
  pairs.Push (csPen3DCoordinatePair (Xyz, XYz));
  pairs.Push (csPen3DCoordinatePair (XYz, xYz));
  pairs.Push (csPen3DCoordinatePair (xYz, xyz));
  pairs.Push (csPen3DCoordinatePair (xyZ, XyZ));
  pairs.Push (csPen3DCoordinatePair (XyZ, XYZ));
  pairs.Push (csPen3DCoordinatePair (XYZ, xYZ));
  pairs.Push (csPen3DCoordinatePair (xYZ, xyZ));
  pairs.Push (csPen3DCoordinatePair (xyz, xyZ));
  pairs.Push (csPen3DCoordinatePair (xYz, xYZ));
  pairs.Push (csPen3DCoordinatePair (Xyz, XyZ));
  pairs.Push (csPen3DCoordinatePair (XYz, XYZ));
  DrawLines (pairs);
}

/**
 * Draws an elliptical arc from start angle to end angle.  Angle must be
 * specified in radians. The arc will be made to fit in the given box.
 * If you want a circular arc, make sure the box is a square.  If you
 * want a full circle or ellipse, specify 0 as the start angle and 2*PI
 * as the end angle.
 */
void csPen3D::DrawArc (const csVector3& c1, const csVector3& c2,
    int axis, float start_angle, float end_angle)
{
  // If start angle and end_angle are too close, abort.
  if (fabs(end_angle-start_angle) < 0.0001) return;

  float x1, y1, x2, y2;
  switch (axis)
  {
    case CS_AXIS_X: x1 = c1.y; y1 = c1.z; x2 = c2.y; y2 = c2.z; break;
    case CS_AXIS_Y: x1 = c1.x; y1 = c1.z; x2 = c2.x; y2 = c2.z; break;
    case CS_AXIS_Z: x1 = c1.x; y1 = c1.y; x2 = c2.x; y2 = c2.y; break;
    default: CS_ASSERT (false); return;
  }

  // Check to make sure that the arc is not in a negative box.
  if (x2 < x1) { float x = x1; x1 = x2; x2 = x; }
  if (y2 < y1) { float y = y1; y1 = y2; y2 = y; }

  float width = x2-x1;
  float height = y2-y1;

  if (fabs (width) < .0001 || fabs (height) < .0001) return;

  float x_radius = width/2;
  float y_radius = height/2;

  float center_x = x1+(x_radius);
  float center_y = y1+(y_radius);

  // Set the delta to be two degrees.
  float delta = 0.0384f;

  Start ();

  //if ((flags & CS_PEN_FILL)) AddVertex(center_x, center_y);

  for (float angle=start_angle; angle<=end_angle; angle+=delta)
  {
    float rx = center_x+(cos(angle)*x_radius);
    float ry = center_y+(sin(angle)*y_radius);
    csVector3 v;
    switch (axis)
    {
      case CS_AXIS_X: v.x = c1.x; v.y = rx; v.z = ry; break;
      case CS_AXIS_Y: v.x = rx; v.y = c1.y; v.z = ry; break;
      case CS_AXIS_Z: v.x = rx; v.y = ry; v.z = c1.z; break;
      default: CS_ASSERT (false);
    }
    AddVertex (v);
  }

  SetupMesh ();
  DrawMesh (CS_MESHTYPE_LINESTRIP);
}

static void SetXYOnVector (csVector3& v, int axis,
    float rx, float ry, const csVector3& orig)
{
  switch (axis)
  {
    case CS_AXIS_X: v.x = orig.x; v.y = rx; v.z = ry; break;
    case CS_AXIS_Y: v.x = rx; v.y = orig.y; v.z = ry; break;
    case CS_AXIS_Z: v.x = rx; v.y = ry; v.z = orig.z; break;
    default: CS_ASSERT (false);
  }
}

static void Get4PointsOnArc (const csVector3& c1, const csVector3& c2,
    int axis, csVector3& v1, csVector3& v2, csVector3& v3, csVector3& v4)
{
  float x1, y1, x2, y2;
  switch (axis)
  {
    case CS_AXIS_X: x1 = c1.y; y1 = c1.z; x2 = c2.y; y2 = c2.z; break;
    case CS_AXIS_Y: x1 = c1.x; y1 = c1.z; x2 = c2.x; y2 = c2.z; break;
    case CS_AXIS_Z: x1 = c1.x; y1 = c1.y; x2 = c2.x; y2 = c2.y; break;
    default: CS_ASSERT (false); return;
  }

  float width = x2-x1;
  float height = y2-y1;

  float x_radius = width/2;
  float y_radius = height/2;
  float center_x = x1 + x_radius;
  float center_y = y1 + y_radius;

  SetXYOnVector (v1, axis, center_x+x_radius, center_y, c1);	// 0
  SetXYOnVector (v2, axis, center_x, center_y+y_radius, c1);	// PI/2
  SetXYOnVector (v3, axis, center_x-x_radius, center_y, c1);	// PI
  SetXYOnVector (v4, axis, center_x, center_y-y_radius, c1);	// 1.5*PI
}

void csPen3D::DrawCylinder (const csBox3& box, int axis)
{
  csVector3 c1, c2;
  csVector3 minv1, minv2, minv3, minv4;
  csVector3 maxv1, maxv2, maxv3, maxv4;
  float m;

  m = box.GetMin (axis);
  c1[axis] = m;
  c2[axis] = m;
  m = box.GetMin ((axis+1)%3);
  c1[(axis+1)%3] = m;
  m = box.GetMax ((axis+1)%3);
  c2[(axis+1)%3] = m;
  m = box.GetMin ((axis+2)%3);
  c1[(axis+2)%3] = m;
  m = box.GetMax ((axis+2)%3);
  c2[(axis+2)%3] = m;
  DrawArc (c1, c2, axis);

  Get4PointsOnArc (c1, c2, axis, minv1, minv2, minv3, minv4);

  m = box.GetMax (axis);
  c1[axis] = m;
  c2[axis] = m;
  DrawArc (c1, c2, axis);

  Get4PointsOnArc (c1, c2, axis, maxv1, maxv2, maxv3, maxv4);

  DrawLine (minv1, maxv1);
  DrawLine (minv2, maxv2);
  DrawLine (minv3, maxv3);
  DrawLine (minv4, maxv4);
}

