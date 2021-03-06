/**************************************************************************
 * Class for 2D X-Z slices
 *
 **************************************************************************
 * Copyright 2010 B.D.Dudson, S.Farley, M.V.Umansky, X.Q.Xu
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 * 
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************/

class FieldPerp;

#ifndef __FIELDPERP_H__
#define __FIELDPERP_H__

#include "field.h"
#include "field2d.h"
#include "field3d.h"

class FieldPerp : public Field {
 public:
  FieldPerp();
  FieldPerp(const FieldPerp& f); // Copy constructor
  ~FieldPerp();

  FieldPerp* clone() const;
  
  void Set(const Field3D &f, int y);

  void setData(real **d) {data = d;}
  real **getData() const { return data; }

  int getIndex() const { return yindex; }
  void setIndex(int y) { yindex = y; }

  void Allocate();

  // operators

  real* operator[](int jx) const;

  FieldPerp & operator=(const FieldPerp &rhs);
  FieldPerp & operator=(const real rhs);

  FieldPerp & operator+=(const FieldPerp &rhs);
  FieldPerp & operator+=(const Field3D &rhs);
  FieldPerp & operator+=(const Field2D &rhs);
  FieldPerp & operator+=(const real rhs);
  
  FieldPerp & operator-=(const FieldPerp &rhs);
  FieldPerp & operator-=(const Field3D &rhs);
  FieldPerp & operator-=(const Field2D &rhs);
  FieldPerp & operator-=(const real rhs);

  FieldPerp & operator*=(const FieldPerp &rhs);
  FieldPerp & operator*=(const Field3D &rhs);
  FieldPerp & operator*=(const Field2D &rhs);
  FieldPerp & operator*=(const real rhs);

  FieldPerp & operator/=(const FieldPerp &rhs);
  FieldPerp & operator/=(const Field3D &rhs);
  FieldPerp & operator/=(const Field2D &rhs);
  FieldPerp & operator/=(const real rhs);

  FieldPerp & operator^=(const FieldPerp &rhs);
  FieldPerp & operator^=(const Field3D &rhs);
  FieldPerp & operator^=(const Field2D &rhs);
  FieldPerp & operator^=(const real rhs);

  // Binary operators

  const FieldPerp operator+(const FieldPerp &other) const;
  const FieldPerp operator+(const Field3D &other) const;
  const FieldPerp operator+(const Field2D &other) const;
  
  const FieldPerp operator-(const FieldPerp &other) const;
  const FieldPerp operator-(const Field3D &other) const;
  const FieldPerp operator-(const Field2D &other) const;

  const FieldPerp operator*(const FieldPerp &other) const;
  const FieldPerp operator*(const Field3D &other) const;
  const FieldPerp operator*(const Field2D &other) const;
  const FieldPerp operator*(const real rhs) const;

  const FieldPerp operator/(const FieldPerp &other) const;
  const FieldPerp operator/(const Field3D &other) const;
  const FieldPerp operator/(const Field2D &other) const;
  const FieldPerp operator/(const real rhs) const;

  const FieldPerp operator^(const FieldPerp &other) const;
  const FieldPerp operator^(const Field3D &other) const;
  const FieldPerp operator^(const Field2D &other) const;
  const FieldPerp operator^(const real rhs) const;

  // Stencils

  void SetStencil(bstencil *fval, bindex *bx) const;
  void SetXStencil(stencil &fval, const bindex &bx, CELL_LOC loc = CELL_DEFAULT) const;
  void SetYStencil(stencil &fval, const bindex &bx, CELL_LOC loc = CELL_DEFAULT) const;
  void SetZStencil(stencil &fval, const bindex &bx, CELL_LOC loc = CELL_DEFAULT) const;
  
 private:
  
  real interp_z(int jx, int jz0, real zoffset, int order) const;

  int yindex;
  
  real **data;

  // Data stack: Blocks of memory for this class
  static int nblocks, max_blocks;
  static real ***block; // Pointer to blocks of memory

  void alloc_data();
  void free_data();
};

// Non-member overloaded operators

const FieldPerp operator*(const real lhs, const FieldPerp &rhs);
const FieldPerp operator/(const real lhs, const FieldPerp &rhs);
const FieldPerp operator^(const real lhs, const FieldPerp &rhs);

#endif
