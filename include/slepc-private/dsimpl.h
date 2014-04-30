/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2013, Universitat Politecnica de Valencia, Spain

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

#if !defined(_DSIMPL)
#define _DSIMPL

#include <slepcds.h>
#include <slepc-private/slepcimpl.h>

PETSC_EXTERN PetscLogEvent DS_Solve,DS_Function,DS_Vectors,DS_Other;
PETSC_INTERN const char *DSMatName[];

typedef struct _DSOps *DSOps;

struct _DSOps {
  PetscErrorCode (*allocate)(DS,PetscInt);
  PetscErrorCode (*view)(DS,PetscViewer);
  PetscErrorCode (*vectors)(DS,DSMatType,PetscInt*,PetscReal*);
  PetscErrorCode (*solve[DS_MAX_SOLVE])(DS,PetscScalar*,PetscScalar*);
  PetscErrorCode (*computefun[SLEPC_FUNCTION_LAST][DS_MAX_FUN])(DS);
  PetscErrorCode (*sort)(DS,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*,PetscInt*);
  PetscErrorCode (*truncate)(DS,PetscInt);
  PetscErrorCode (*update)(DS);
  PetscErrorCode (*cond)(DS,PetscReal*);
  PetscErrorCode (*transharm)(DS,PetscScalar,PetscReal,PetscBool,PetscScalar*,PetscReal*);
  PetscErrorCode (*transrks)(DS,PetscScalar);
  PetscErrorCode (*normalize)(DS,DSMatType,PetscInt);
};

struct _p_DS {
  PETSCHEADER(struct _DSOps);
  PetscInt       method;             /* identifies the variant to be used */
  PetscInt       funmethod;          /* to choose among methods for function evaluation */
  PetscBool      compact;            /* whether the matrices are stored in compact form */
  PetscBool      refined;            /* get refined vectors instead of regular vectors */
  PetscBool      extrarow;           /* assume the matrix dimension is (n+1) x n */
  PetscInt       ld;                 /* leading dimension */
  PetscInt       l;                  /* number of locked (inactive) leading columns */
  PetscInt       n;                  /* current dimension */
  PetscInt       m;                  /* current column dimension (for SVD only) */
  PetscInt       k;                  /* intermediate dimension (e.g. position of arrow) */
  PetscInt       t;                  /* length of decomposition when it was truncated */
  DSStateType    state;              /* the current state */
  PetscScalar    *mat[DS_NUM_MAT];   /* the matrices */
  PetscReal      *rmat[DS_NUM_MAT];  /* the matrices (real) */
  PetscInt       *perm;              /* permutation */
  PetscInt       nf;                 /* number of functions in f[] */
  FN             f[DS_NUM_EXTRA];    /* functions provided via DSSetFN() */
  PetscScalar    *work;
  PetscReal      *rwork;
  PetscBLASInt   *iwork;
  PetscInt       lwork,lrwork,liwork;
  /*-------------- User-provided functions and contexts -----------------*/
  PetscErrorCode (*comparison)(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*);
  void           *comparisonctx;
};

PETSC_INTERN PetscErrorCode DSAllocateMat_Private(DS,DSMatType);
PETSC_INTERN PetscErrorCode DSAllocateMatReal_Private(DS,DSMatType);
PETSC_INTERN PetscErrorCode DSAllocateWork_Private(DS,PetscInt,PetscInt,PetscInt);
PETSC_INTERN PetscErrorCode DSViewMat_Private(DS,PetscViewer,DSMatType);
PETSC_INTERN PetscErrorCode DSSortEigenvalues_Private(DS,PetscScalar*,PetscScalar*,PetscInt*,PetscBool);
PETSC_INTERN PetscErrorCode DSSortEigenvaluesReal_Private(DS,PetscReal*,PetscInt*);
PETSC_INTERN PetscErrorCode DSPermuteColumns_Private(DS,PetscInt,PetscInt,DSMatType,PetscInt*);
PETSC_INTERN PetscErrorCode DSPermuteRows_Private(DS,PetscInt,PetscInt,DSMatType,PetscInt*);
PETSC_INTERN PetscErrorCode DSPermuteBoth_Private(DS,PetscInt,PetscInt,DSMatType,DSMatType,PetscInt*);
PETSC_INTERN PetscErrorCode DSCopyMatrix_Private(DS,DSMatType,DSMatType);
PETSC_INTERN PetscErrorCode DSSetIdentity(DS,DSMatType);
PETSC_INTERN PetscErrorCode DSComputeMatrix(DS,PetscScalar,PetscBool,DSMatType);
PETSC_EXTERN PetscErrorCode DSOrthogonalize(DS,DSMatType,PetscInt,PetscInt*);
PETSC_EXTERN PetscErrorCode DSPseudoOrthogonalize(DS,DSMatType,PetscInt,PetscReal*,PetscInt*,PetscReal*);

PETSC_INTERN PetscErrorCode DSGHIEPOrthogEigenv(DS,DSMatType,PetscScalar*,PetscScalar*,PetscBool);
PETSC_INTERN PetscErrorCode DSGHIEPComplexEigs(DS,PetscInt,PetscInt,PetscScalar*,PetscScalar*);
PETSC_INTERN PetscErrorCode DSGHIEPInverseIteration(DS,PetscScalar*,PetscScalar*);
PETSC_INTERN PetscErrorCode DSIntermediate_GHIEP(DS);
PETSC_INTERN PetscErrorCode DSSwitchFormat_GHIEP(DS,PetscBool);
PETSC_INTERN PetscErrorCode DSGHIEPRealBlocks(DS);

PETSC_INTERN PetscErrorCode DSSolve_GHIEP_HZ(DS,PetscScalar*,PetscScalar*);
PETSC_INTERN PetscErrorCode DSSolve_GHIEP_DQDS_II(DS,PetscScalar*,PetscScalar*);

#endif
