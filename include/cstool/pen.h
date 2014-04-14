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

#ifndef __CS_CSTOOL_PEN_H__
#define __CS_CSTOOL_PEN_H__

/**\file
 * Vector shape drawing.
 */

#include "csgeom/poly3d.h"
#include "csgeom/polyidx.h"
#include "csgeom/vector4.h"
#include "csgeom/vector2.h"
#include "csutil/cscolor.h"
#include "csutil/dirtyaccessarray.h"
#include "csutil/ref.h"
#include "csutil/refarr.h"
#include "csutil/memfile.h"

#include "ivideo/graph2d.h"
#include "ivideo/graph3d.h"
#include "ivideo/texture.h"

struct iFont;

enum CS_PEN_TEXT_ALIGN
{
  CS_PEN_TA_TOP,
  CS_PEN_TA_BOT,
  CS_PEN_TA_LEFT,
  CS_PEN_TA_RIGHT,
  CS_PEN_TA_CENTER
};

enum CS_PEN_FLAGS
{
  CS_PEN_FILL = 1,
  CS_PEN_SWAPCOLORS = 2,
  CS_PEN_TEXTURE_ONLY = 4,
  CS_PEN_TEXTURE = 5 /* fill | 4 */
};
 
/**
 * A pen coordinate.
 */
struct csPenCoordinate
{
  int x, y;
  csPenCoordinate (int x, int y) : x (x), y (y) { }
};

/**
 * A coordinate pair
 */
struct csPenCoordinatePair
{
  csPenCoordinate c1, c2;
  csPenCoordinatePair (int x1, int y1, int x2, int y2) :
    c1 (x1, y1), c2 (x2, y2) { }
};

class csPen;

/**
 * A pen cache. This class can be used to remember a series of
 * commands so that they can be rendered faster.
 */
class CS_CRYSTALSPACE_EXPORT csPenCache
{
private:
  struct PRMesh
  {
    csSimpleRenderMesh mesh;
    csSimpleMeshFlags flags;
    size_t offsetVertices;
    size_t offsetIndices;
    size_t vertexCount;
    size_t indexCount;
    PRMesh () : offsetVertices (0), offsetIndices (0),
      vertexCount (0), indexCount (0) { }
  };

  csArray<PRMesh> meshes;

  /** The vertices that we're generating. */
  csDirtyAccessArray<csVector3> vertices;

  /** The vertex indices that we're generating. */
  csDirtyAccessArray<uint> vertexIndices;

  /** The color array generated for verts as we render. */
  csDirtyAccessArray<csVector4> colors;

  /** The texture coordinates are generated as we render too. */
  csDirtyAccessArray<csVector2> texcoords;

  /**
   * Test if two rendermeshes can be merged.
   */
  bool IsMergeable (csSimpleRenderMesh* mesh1, csSimpleRenderMesh* mesh2);

  /**
   * Merge the render mesh with the last one.
   */
  void MergeMesh (csSimpleRenderMesh* mesh);

public:
  /**
   * Push a new mesh to this renderer. If this mesh is compatible
   * with the previous one then they will be merged.
   */
  void PushMesh (csSimpleRenderMesh* mesh, csSimpleMeshFlags flags);

  /**
   * Render this cached pen.
   */
  void Render (iGraphics3D* g3d);

  /**
   * Clear this cache.
   */
  void Clear ();

  /**
   * Set a transform.
   */
  void SetTransform (const csReversibleTransform& trans);
};


/**
 * A pen implementation with which you can do various
 * kinds of 2D rendering.
 */
class CS_CRYSTALSPACE_EXPORT csPen
{
private:
  /** The 3d context for drawing. */
  csRef<iGraphics3D> g3d;

  /** The 2d context for drawing. */
  csRef<iGraphics2D> g2d;

  /** The mesh that we reuse in developing the shapes we're making. */
  csSimpleRenderMesh mesh;

  /** The polygon index that we're generating. */
  csPolyIndexed poly_idx;

  /** The polygon that we're generating. */
  csPoly3D poly;

  /** The color we use. */
  csVector4 color;

  /** The alternate color we might use. */
  csVector4 alt_color;

  /** The texture handle that we might use. */
  csRef<iTextureHandle> tex;

  /** The translation we keep for text. */
  csVector3 tt;

  /** The color array generated for verts as we render. */
  csDirtyAccessArray<csVector4> colors;

  /** The texture coordinates are generated as we render too. */
  csDirtyAccessArray<csVector2> texcoords;

  /** The array that stores the transformation stack. */
  csArray<csReversibleTransform> transforms;

  /** The array that stores the translation stack for text. */
  csArray<csVector3> translations;

  /** The width of the pen for non-filled shapes. */
  float pen_width;

  /** The flags currently set. */
  uint flags;

  /** The type of a point. */
  struct point
  {
    float x,y;
  };

  /** Used to keep track of points when drawing thick lines. */
  csArray<point> line_points;

  /** The last two calculated points. */
  point last[2];

  /** For most shapes, we can set the width and height, and enable
   * automatic texture coordinate generation. */
  float sh_w, sh_h;

  /** When this is set, automatic texture coordinate generation will occur
   * in add vertex. */
  bool gen_tex_coords;

  /** A cache that can be used to store intermediate results */
  csPenCache* penCache;

protected:
  /**
   * Initializes our working objects.
   */
  void Start();

  /**
   * Adds a vertex.
   * @param x X coord
   * @param y Y coord
   * @param force_add Forces the coordinate to be added as a vertex, instead
   *  of trying to be smart and make it a thick vertex.
   */
  void AddVertex (float x, float y, bool force_add=false);

  /** Adds a texture coordinate.
   * @param x The texture's x coord.
   * @param y The texture's y coord.
   */
  inline void AddTexCoord (float x, float y);

  /**
   * Worker, sets up the mesh with the vertices, color and other information.
   */
  void SetupMesh();

  /**
   * Worker, draws the mesh.
   */
  void DrawMesh (csRenderMeshType mesh_type);

  /**
   * Worker, sets up the pen to do auto texturing.
   */
  void SetAutoTexture (float w, float h);

  /**
   * Worker, adds thick line points.  A thick point is created when the
   * pen width is greater than 1.  It uses polygons to simulate thick
   * lines.
   */
  void AddThickPoints (float x1, float y1, float x2, float y2);

public:
  csPen(iGraphics2D *_g2d, iGraphics3D *_g3d);
  ~csPen();

  /**
   * Set an active pen cache. After this call all drawing calls to
   * this pen will be cached on the cache until you clear the pen
   * cache again.
   * At that point the pen cache can be used to do cached drawings.
   * Note that the pen cache does not work for text.
   * Also note that the pen cache can be updated later with new
   * commands if you set it again onto a pen. It is also safe to
   * use the same pencache with different pens.
   */
  void SetActiveCache (csPenCache* cache) { penCache = cache; }

  /**
   * Sets the given flag.
   * @param flag The flag to set.
   */
  void SetFlag (uint flag);
  /**
   * Clears the given flag.
   * @param flag The flag to clear.
   */
  void ClearFlag (uint flag);

  /**
   * Sets the given mix (blending) mode.
   * @param mode The mixmode to set.
   */
  void SetMixMode (uint mode);

  /**
   * Sets the current color.
   * @param r The red component.
   * @param g The green component.
   * @param b The blue component.
   * @param a The alpha component.
   */
  void SetColor (float r, float g, float b, float a);
  void SetColor (const csColor4 &color);

  /**
   * Sets the texture handle.
   * @param tex A reference to the texture to use.
   */
  void SetTexture (iTextureHandle* tex);

  /**
   * Swaps the current color and the alternate color.
   */
  void SwapColors();

  /**
   * Sets the width of the pen for line drawing.
   */
  void SetPenWidth(float width);

  /**
   * Clears the current transform, resets to identity.
   */
  void ClearTransform();

  /**
   * Pushes the current transform onto the stack. *
   */
  void PushTransform();

  /**
   * Pops the transform stack. The top of the stack becomes the current
   * transform.
   */
  void PopTransform();

  /**
   * Sets the origin of the coordinate system.
   */
  void SetOrigin(const csVector3 &o);

  /**
   * Translates by the given vector
   */
  void Translate(const csVector3 &t);

  /**
   * Rotates by the given angle.
   */
  void Rotate(const float &a);

  /**
   * Set a transform.
   */
  void SetTransform (const csReversibleTransform& trans);

  /**
   * Clip a line to the given canvas size.
   * Returns true the line is not empty.
   */
  bool ClipLine (int& x1, int& y1, int& x2, int& y2);

  /**
   * Draws a single line.
   */
  void DrawLine (int x1, int y1, int x2, int y2);
  void DrawLine (const csPenCoordinatePair& coords)
  {
    DrawLine (coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y);
  }
  void DrawThickLine (int x1, int y1, int x2, int y2);

  /**
   * Draws a series of lines.
   */
  void DrawLines (const csArray<csPenCoordinatePair>& pairs);
  void DrawThickLines (const csArray<csPenCoordinatePair>& pairs);

  /**
   * Draws a single point.
   */
  void DrawPoint (int x1, int y2);
  void DrawPoint (const csPenCoordinate& c) { DrawPoint (c.x, c.y); }

  /**
   * Draws a rectangle.
   */
  void DrawRect (int x1, int y1, int x2, int y2);
  void DrawRect (const csPenCoordinatePair& coords)
  {
    DrawRect (coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y);
  }

  /**
   * Draws a mitered rectangle. The miter value should be between 0.0 and 1.0,
   * and determines how much of the corner is mitered off and beveled.
   */
  void DrawMiteredRect (int x1, int y1, int x2, int y2,
    uint miter);
  void DrawMiteredRect (const csPenCoordinatePair& coords,
    uint miter)
  {
    DrawMiteredRect (coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y,
	miter);
  }

  /**
   * Draws a rounded rectangle. The roundness value should be between
   * 0.0 and 1.0, and determines how much of the corner is rounded off.
   */
  void DrawRoundedRect (int x1, int y1, int x2, int y2,
    uint roundness);
  void DrawRoundedRect (const csPenCoordinatePair& coords,
    uint roundness)
  {
    DrawRoundedRect (coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y,
	roundness);
  }

  /**
   * Draws an elliptical arc from start angle to end angle.  Angle must be
   * specified in radians. The arc will be made to fit in the given box.
   * If you want a circular arc, make sure the box is a square.  If you want
   * a full circle or ellipse, specify 0 as the start angle and 2*PI as the
   * end angle.
   */
  void DrawArc (int x1, int y1, int x2, int y2,
  	float start_angle = 0, float end_angle = 6.2831853);
  void DrawArc (const csPenCoordinatePair& coords,
  	float start_angle = 0, float end_angle = 6.2831853)
  {
    DrawArc (coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y,
	start_angle, end_angle);
  }

  /**
   * Draws a triangle around the given vertices.
   */
  void DrawTriangle (int x1, int y1, int x2, int y2, int x3, int y3);
  void DrawTriangle (const csPenCoordinate& c1,
      const csPenCoordinate& c2, const csPenCoordinate& c3)
  {
    DrawTriangle (c1.x, c1.y, c2.x, c2.y, c3.x, c3.y);
  }

  /**
   * Writes text in the given font at the given location.
   */
  void Write (iFont *font, int x1, int y1, const char *text);
  void Write (iFont *font, const csPenCoordinate& c, const char *text)
  {
    Write (font, c.x, c.y, text);
  }
  /**
   * Writes multiple lines of text in the given font at the given location.
   */
  void WriteLines (iFont *font, int x1, int y1, const csStringArray& lines);
  void WriteLines (iFont *font, const csPenCoordinate& c,
      const csStringArray& lines)
  {
    WriteLines (font, c.x, c.y, lines);
  }

  /**
   * Writes text in the given font, in the given box.  The alignment
   * specified in h_align and v_align determine how it should be aligned.
   */
  void WriteBoxed (iFont *font, int x1, int y1, int x2, int y2,
    uint h_align, uint v_align, const char *text);
  void WriteBoxed (iFont *font, const csPenCoordinatePair& coords,
    uint h_align, uint v_align, const char *text)
  {
    WriteBoxed (font, coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y,
	h_align, v_align, text);
  }

  /**
   * Writes multiple lines of text in the given font, in the given box.
   * The alignment specified in h_align and v_align determine how it should
   * be aligned.
   */
  void WriteLinesBoxed (iFont *font, int x1, int y1, int x2, int y2,
    uint h_align, uint v_align, const csStringArray& lines);
  void WriteLinesBoxed (iFont *font, const csPenCoordinatePair& coords,
    uint h_align, uint v_align, const csStringArray& lines)
  {
    WriteLinesBoxed (font, coords.c1.x, coords.c1.y, coords.c2.x, coords.c2.y,
	h_align, v_align, lines);
  }
};

/**
 * A coordinate pair
 */
struct csPen3DCoordinatePair
{
  csVector3 c1, c2;
  csPen3DCoordinatePair (const csVector3& c1, const csVector3& c2) :
    c1 (c1), c2 (c2) { }
};

/**
 * A pen implementation with which you can do various
 * kinds of 3D line rendering.
 */
class CS_CRYSTALSPACE_EXPORT csPen3D
{
private:
  /** The 3d context for drawing */
  csRef<iGraphics3D> g3d;

  /** The 2d context for drawing */
  csRef<iGraphics2D> g2d;

  /** The mesh that we reuse in developing the shapes we're making */
  csSimpleRenderMesh mesh;

  /** The color we use */
  csVector4 color;

  /** The polygon index that we're generating */
  csPolyIndexed poly_idx;

  /** The polygon that we're generating */
  csPoly3D poly;

  /** The color array generated for verts as we render */
  csDirtyAccessArray<csVector4> colors;

  /** The texture coordinates are generated as we render too */
  csDirtyAccessArray<csVector2> texcoords;

  /** A cache that can be used to store intermediate results */
  csPenCache* penCache;

  /** Local to object transform */
  csReversibleTransform local2object;

protected:
  /**
   * Initializes our working objects.
   */
  void Start();

  /**
   * Adds a vertex.
   * @param v is the vertex
   */
  void AddVertex (const csVector3& v);

  /**
   * Worker, sets up the mesh with the vertices, color and other information.
   */
  void SetupMesh();

  /**
   * Worker, draws the mesh.
   */
  void DrawMesh (csRenderMeshType mesh_type);

public:
  csPen3D (iGraphics2D *_g2d, iGraphics3D *_g3d);
  ~csPen3D ();

  /**
   * Set an active pen cache. After this call all drawing calls to
   * this pen will be cached on the cache until you clear the pen
   * cache again.
   * At that point the pen cache can be used to do cached drawings.
   * Note that the pen cache does not work for text.
   * Also note that the pen cache can be updated later with new
   * commands if you set it again onto a pen. It is also safe to
   * use the same pencache with different pens.
   */
  void SetActiveCache (csPenCache* cache) { penCache = cache; }

  /**
   * Sets the given mix (blending) mode.
   * @param mode The mixmode to set.
   */
  void SetMixMode (uint mode);

  /**
   * Sets the current color.
   * @param r The red component.
   * @param g The green component.
   * @param b The blue component.
   * @param a The alpha component.
   */
  void SetColor (float r, float g, float b, float a);
  void SetColor (const csColor4 &color);

  /**
   * Set object to world transform. This is used only
   * when actually drawing.
   */
  void SetTransform (const csReversibleTransform& trans);

  /**
   * Set the local to object transform. This is used at the
   * time the primitives are rendered.
   */
  void SetLocal2ObjectTransform (const csReversibleTransform& trans)
  {
    local2object = trans;
  }

  /// Get the local to object transform.
  const csReversibleTransform& GetLocal2ObjectTransform () const
  {
    return local2object;
  }

  /**
   * Draws a single line.
   */
  void DrawLine (const csVector3& v1, const csVector3& v2);
  void DrawLine (const csPen3DCoordinatePair& coords)
  {
    DrawLine (coords.c1, coords.c2);
  }

  /**
   * Draws a series of lines.
   */
  void DrawLines (const csArray<csPen3DCoordinatePair>& pairs);

  /**
   * Draw a box.
   */
  void DrawBox (const csBox3& box);

  /**
   * Draws an elliptical arc from start angle to end angle.  Angle must be
   * specified in radians. The arc will be made to fit in the given plane
   * as defined by c1-c2 and on the specific axis. c1 and c2 must be on
   * the same plane as defined by axis.
   * If you want a circular arc, make sure the box is a square.  If you want
   * a full circle or ellipse, specify 0 as the start angle and 2*PI as the
   * end angle.
   */
  void DrawArc (const csVector3& c1, const csVector3& c2,
	int axis, float start_angle = 0, float end_angle = 6.2831853);

  /**
   * Draw a cylinder. The axis specifies the top and bottom of the
   * cylinder.
   */
  void DrawCylinder (const csBox3& box, int axis);
};


#endif
