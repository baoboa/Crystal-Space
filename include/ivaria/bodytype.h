/*
    Copyright (C) 2012 by Jorrit Tyberghein

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


#ifndef __CS_IVARIA_BODYTYPE_H__
#define __CS_IVARIA_BODYTYPE_H__

/**\file
 * Physics interfaces
 */

enum csColliderGeometryType
{
  NO_GEOMETRY,                  /*!< No geometry has been defined */
  BOX_COLLIDER_GEOMETRY,        /*!< Box geometry */
  PLANE_COLLIDER_GEOMETRY,      /*!< Plane geometry */
  TRIMESH_COLLIDER_GEOMETRY,    /*!< Concave mesh geometry */
  CONVEXMESH_COLLIDER_GEOMETRY, /*!< Convex mesh geometry */
  CYLINDER_COLLIDER_GEOMETRY,   /*!< Cylinder geometry */
  CAPSULE_COLLIDER_GEOMETRY,    /*!< Capsule geometry */
  SPHERE_COLLIDER_GEOMETRY      /*!< Sphere geometry */
};


#endif // __CS_IVARIA_BODYTYPE_H__

