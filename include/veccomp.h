/*
   User interface for vectors composed by vectors. 

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2009, Universidad Politecnica de Valencia, Spain

   This file is part of SLEPc.
      
   SLEPc is free software: you can redistribute it and/or modify it under  the
   terms of version 3 of the GNU Lesser General Public License as published by
   the Free Software Foundation.

   SLEPc  is  distributed in the hope that it will be useful, but WITHOUT  ANY 
   WARRANTY;  without even the implied warranty of MERCHANTABILITY or  FITNESS 
   FOR  A  PARTICULAR PURPOSE. See the GNU Lesser General Public  License  for 
   more details.

   You  should have received a copy of the GNU Lesser General  Public  License
   along with SLEPc. If not, see <http://www.gnu.org/licenses/>.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#if !defined(__SLEPCVECCOMP_H)
#define __SLEPCVECCOMP_H
PETSC_EXTERN_CXX_BEGIN

#define VECCOMP  "comp"

PetscErrorCode PETSCVEC_DLLEXPORT VecRegister_Comp(const char path[]);
PetscErrorCode PETSCVEC_DLLEXPORT VecCreateComp(MPI_Comm comm, PetscInt *Nx,
                                                PetscInt n, const VecType t,
                                                Vec Vparent, Vec *V);
PetscErrorCode PETSCVEC_DLLEXPORT VecCreateCompWithVecs(Vec *x, PetscInt n,
                                                        Vec Vparent, Vec *V);
PetscErrorCode VecCompGetVecs(Vec win, const Vec **x, PetscInt *n);
PetscErrorCode VecCompSetVecs(Vec win, Vec *x, PetscInt n);

PETSC_EXTERN_CXX_END
#endif

