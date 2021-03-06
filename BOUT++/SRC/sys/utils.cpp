/**************************************************************************
 * Memory allocation routines
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

#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

real *rvector(int size)
{
  return (real*) malloc(sizeof(real)*size);
}

real *rvresize(real *v, int newsize)
{
  return (real*) realloc(v, sizeof(real)*newsize);
}

int *ivector(int size)
{
  return (int*) malloc(sizeof(int)*size);
}

int *ivresize(int *v, int newsize)
{
  return (int*) realloc(v, sizeof(int)*newsize);
}

real **rmatrix(int xsize, int ysize)
{
  long i;
  real **m;
  
  if((m = (real**) malloc(xsize*sizeof(real*))) == (real**) NULL) {
    printf("Error: could not allocate memory:%d\n", xsize);
    exit(1);
  }

  if((m[0] = (real*) malloc(xsize*ysize*sizeof(real))) == (real*) NULL) {
    printf("Error: could not allocate memory\n");
    exit(1);
  }
  for(i=1;i!=xsize;i++) {
    m[i] = m[i-1] + ysize;
  }

  return(m);
}

int **imatrix(int xsize, int ysize)
{
  long i;
  int **m;
  
  if((m = (int**) malloc(xsize*sizeof(int*))) == (int**) NULL) {
    printf("Error: could not allocate memory:%d\n", xsize);
    exit(1);
  }

  if((m[0] = (int*) malloc(xsize*ysize*sizeof(int))) == (int*) NULL) {
    printf("Error: could not allocate memory\n");
    exit(1);
  }
  for(i=1;i!=xsize;i++) {
    m[i] = m[i-1] + ysize;
  }

  return(m);
}

void free_rmatrix(real **m)
{
  free(m[0]);
  free(m);
}

void free_imatrix(int **m)
{
  free(m[0]);
  free(m);
}

real ***r3tensor(int nrow, int ncol, int ndep)
{
  int i,j;
  real ***t;

  /* allocate pointers to pointers to rows */
  t=(real ***) malloc((size_t)(nrow*sizeof(real**)));

  /* allocate pointers to rows and set pointers to them */
  t[0]=(real **) malloc((size_t)(nrow*ncol*sizeof(real*)));

  /* allocate rows and set pointers to them */
  t[0][0]=(real *) malloc((size_t)(nrow*ncol*ndep*sizeof(real)));

  for(j=1;j!=ncol;j++) t[0][j]=t[0][j-1]+ndep;
  for(i=1;i!=nrow;i++) {
    t[i]=t[i-1]+ncol;
    t[i][0]=t[i-1][0]+ncol*ndep;
    for(j=1;j!=ncol;j++) t[i][j]=t[i][j-1]+ndep;
  }

  /* return pointer to array of pointers to rows */
  return t;
}

dcomplex **cmatrix(int nrow, int ncol)
{
  dcomplex **m;
  int i;

  m = new dcomplex*[nrow];
  m[0] = new dcomplex[nrow*ncol];
  for(i=1;i<nrow;i++)
    m[i] = m[i-1] + ncol;

  return m;
}

void free_cmatrix(dcomplex** m)
{
  delete[] m[0];
  delete[] m;
}

real SQ(real x)
{
  return(x*x);
}

int ROUND(real x)
{
  return (x > 0.0) ? (int) (x + 0.5) : (int) (x - 0.5);
}

void SWAP(real &a, real &b)
{
  real tmp;
  
  tmp = a;
  a = b;
  b = tmp;
}

void SWAP(dcomplex &a, dcomplex &b)
{
  dcomplex tmp;
  
  tmp = a;
  a = b;
  b = tmp;
}

bool is_pow2(int x)
{
  return x && !((x-1) & x);
}

/*
// integer power
real operator^(real lhs, int n)
{
  real result;
  
  if(n == 0)
    return 1.0;
  
  if(n < 0) {
    lhs = 1.0 / lhs;
    n *= -1;
  }
  
  result = 1.0;
  
  while(n > 1) {
    if( (n & 1) == 0 ) {
      lhs *= lhs;
      n /= 2;
    }else {
      result *= lhs;
      n--;
    }
  }
  result *= lhs;

  return result;
}

real operator^(real lhs, const real &rhs)
{
  return pow(lhs, rhs);
}
*/

/**************************************************************************
 * String routines
 **************************************************************************/

/// Allocate memory for a copy of given string
char* copy_string(const char* s)
{
  char *s2;
  int n;
  
  if(s == NULL)
    return NULL;

  n = strlen(s);
  s2 = (char*) malloc(n+1);
  strcpy(s2, s);
  return s2;
}

/// Concatenate a string. This is a mildly nasty hack, and not thread-safe.
/// Simplifies some code though.
char *strconcat(const char* left, const char *right)
{
  static char buffer[128];
  
  snprintf(buffer, 128, "%s%s", left, right);
  return buffer;
}

/**************************************************************************
 * Matrix inversion
 **************************************************************************/

/// LU decomposition 
/*!
 * LU decompose an nxn complex matrix. Adapted from numerical recipes
 */
/*
void ludcmp(dcomplex **a, int n, int *indx)
{
  int i, imax, j, k;
  
}
*/
