/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2011, Universitat Politecnica de Valencia, Spain

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

#if !defined(__SLEPCDS_H)
#define __SLEPCDS_H
#include "slepcsys.h"

#define DS_MAX_SOLVE 6

PETSC_EXTERN PetscErrorCode DSInitializePackage(const char[]);
/*S
    DS - Direct solver (or dense system), to represent low-dimensional
    eigenproblems that must be solved within iterative solvers. This is an
    auxiliary object and is not normally needed by application programmers.

    Level: advanced

.seealso:  DSCreate()
S*/
typedef struct _p_DS* DS;

/*E
    DSType - String with the name of the type of direct solver. Roughly,
    there are as many types as problem types are available within SLEPc.

    Level: advanced

.seealso: DSSetType(), DS
E*/
#define DSType            char*
#define DSHEP             "hep"
#define DSNHEP            "nhep"
#define DSGHEP            "ghep"
#define DSGHIEP           "ghiep"
#define DSGNHEP           "gnhep"
#define DSSVD             "svd"
#define DSQEP             "qep"

/* Logging support */
PETSC_EXTERN PetscClassId DS_CLASSID;

/*E
    DSStateType - to indicate in which state the direct solver is

    Level: advanced

.seealso: DSSetState()
E*/
typedef enum { DS_STATE_RAW,
               DS_STATE_INTERMEDIATE,
               DS_STATE_CONDENSED,
               DS_STATE_SORTED } DSStateType;

/*E
    DSMatType - to refer to one of the matrices stored internally in DS

    Notes:
    The matrices preferently refer to:
+   DS_MAT_A  - first matrix of eigenproblem/singular value problem
.   DS_MAT_B  - second matrix of a generalized eigenproblem
.   DS_MAT_C  - third matrix of a quadratic eigenproblem
.   DS_MAT_T  - tridiagonal matrix
.   DS_MAT_D  - diagonal matrix
.   DS_MAT_Q  - orthogonal matrix of (right) Schur vectors
.   DS_MAT_Z  - orthogonal matrix of left Schur vectors
.   DS_MAT_X  - right eigenvectors
.   DS_MAT_Y  - left eigenvectors
.   DS_MAT_U  - left singular vectors
.   DS_MAT_VT - right singular vectors
-   DS_MAT_W  - workspace matrix

    All matrices can have space to hold ld x ld elements, except for
    DS_MAT_T that has space for 3 x ld elements (ld = leading dimension)
    and DS_MAT_D that has space for just ld elements.

    Level: advanced

.seealso: DSAllocate(), DSGetArray(), DSGetArrayReal(), DSVectors()
E*/
typedef enum { DS_MAT_A,
               DS_MAT_B,
               DS_MAT_C,
               DS_MAT_T,
               DS_MAT_D,
               DS_MAT_Q,
               DS_MAT_Z,
               DS_MAT_X,
               DS_MAT_Y,
               DS_MAT_U,
               DS_MAT_VT,
               DS_MAT_W,
               DS_NUM_MAT } DSMatType;

PETSC_EXTERN PetscErrorCode DSCreate(MPI_Comm,DS*);
PETSC_EXTERN PetscErrorCode DSSetType(DS,const DSType);
PETSC_EXTERN PetscErrorCode DSGetType(DS,const DSType*);
PETSC_EXTERN PetscErrorCode DSSetOptionsPrefix(DS,const char *);
PETSC_EXTERN PetscErrorCode DSAppendOptionsPrefix(DS,const char *);
PETSC_EXTERN PetscErrorCode DSGetOptionsPrefix(DS,const char *[]);
PETSC_EXTERN PetscErrorCode DSSetFromOptions(DS);
PETSC_EXTERN PetscErrorCode DSView(DS,PetscViewer);
PETSC_EXTERN PetscErrorCode DSDestroy(DS*);
PETSC_EXTERN PetscErrorCode DSReset(DS);

PETSC_EXTERN PetscErrorCode DSAllocate(DS,PetscInt);
PETSC_EXTERN PetscErrorCode DSGetLeadingDimension(DS,PetscInt*);
PETSC_EXTERN PetscErrorCode DSSetState(DS,DSStateType);
PETSC_EXTERN PetscErrorCode DSGetState(DS,DSStateType*);
PETSC_EXTERN PetscErrorCode DSSetDimensions(DS,PetscInt,PetscInt,PetscInt,PetscInt);
PETSC_EXTERN PetscErrorCode DSGetDimensions(DS,PetscInt*,PetscInt*,PetscInt*,PetscInt*);
PETSC_EXTERN PetscErrorCode DSTruncate(DS,PetscInt);
PETSC_EXTERN PetscErrorCode DSSetMethod(DS,PetscInt);
PETSC_EXTERN PetscErrorCode DSGetMethod(DS,PetscInt*);
PETSC_EXTERN PetscErrorCode DSSetCompact(DS,PetscBool);
PETSC_EXTERN PetscErrorCode DSGetCompact(DS,PetscBool*);
PETSC_EXTERN PetscErrorCode DSSetExtraRow(DS,PetscBool);
PETSC_EXTERN PetscErrorCode DSGetExtraRow(DS,PetscBool*);
PETSC_EXTERN PetscErrorCode DSSetRefined(DS,PetscBool);
PETSC_EXTERN PetscErrorCode DSGetRefined(DS,PetscBool*);
PETSC_EXTERN PetscErrorCode DSGetArray(DS,DSMatType,PetscScalar *a[]);
PETSC_EXTERN PetscErrorCode DSRestoreArray(DS,DSMatType,PetscScalar *a[]);
PETSC_EXTERN PetscErrorCode DSGetArrayReal(DS,DSMatType,PetscReal *a[]);
PETSC_EXTERN PetscErrorCode DSRestoreArrayReal(DS,DSMatType,PetscReal *a[]);
PETSC_EXTERN PetscErrorCode DSVectors(DS,DSMatType,PetscInt*,PetscReal*);
PETSC_EXTERN PetscErrorCode DSSolve(DS,PetscScalar*,PetscScalar*);
PETSC_EXTERN PetscErrorCode DSSort(DS,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*,PetscInt*);
PETSC_EXTERN PetscErrorCode DSSetEigenvalueComparison(DS,PetscErrorCode (*)(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*),void*);
PETSC_EXTERN PetscErrorCode DSGetEigenvalueComparison(DS,PetscErrorCode (**)(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*),void**);
PETSC_EXTERN PetscErrorCode DSUpdateExtraRow(DS);
PETSC_EXTERN PetscErrorCode DSCond(DS,PetscReal*);
PETSC_EXTERN PetscErrorCode DSTranslateHarmonic(DS,PetscScalar,PetscReal,PetscBool,PetscScalar*,PetscReal*);
PETSC_EXTERN PetscErrorCode DSTranslateRKS(DS,PetscScalar);
PETSC_EXTERN PetscErrorCode DSNormalize(DS,DSMatType,PetscInt);
PETSC_EXTERN PetscErrorCode DSSetIdentity(DS,DSMatType);
PETSC_EXTERN PetscErrorCode DSOrthogonalize(DS,DSMatType,PetscInt,PetscInt*);
PETSC_EXTERN PetscErrorCode DSPseudoOrthogonalize(DS,DSMatType,PetscInt,PetscReal*,PetscInt*,PetscReal*);

PETSC_EXTERN PetscFList DSList;
PETSC_EXTERN PetscBool  DSRegisterAllCalled;
PETSC_EXTERN PetscErrorCode DSRegisterAll(const char[]);
PETSC_EXTERN PetscErrorCode DSRegister(const char[],const char[],const char[],PetscErrorCode(*)(DS));
PETSC_EXTERN PetscErrorCode DSRegisterDestroy(void);

/*MC
   DSRegisterDynamic - Adds a direct solver to the DS package.

   Synopsis:
   PetscErrorCode DSRegisterDynamic(const char *name,const char *path,const char *name_create,PetscErrorCode (*routine_create)(DS))

   Not collective

   Input Parameters:
+  name - name of a new user-defined DS
.  path - path (either absolute or relative) the library containing this solver
.  name_create - name of routine to create context
-  routine_create - routine to create context

   Notes:
   DSRegisterDynamic() may be called multiple times to add several user-defined
   direct solvers.

   If dynamic libraries are used, then the fourth input argument (routine_create)
   is ignored.

   Level: advanced

.seealso: DSRegisterDestroy(), DSRegisterAll()
M*/
#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
#define DSRegisterDynamic(a,b,c,d) DSRegister(a,b,c,0)
#else
#define DSRegisterDynamic(a,b,c,d) DSRegister(a,b,c,d)
#endif

#endif