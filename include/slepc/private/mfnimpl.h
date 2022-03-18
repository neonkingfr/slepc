/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#if !defined(SLEPCMFNIMPL_H)
#define SLEPCMFNIMPL_H

#include <slepcmfn.h>
#include <slepc/private/slepcimpl.h>

SLEPC_EXTERN PetscBool MFNRegisterAllCalled;
SLEPC_EXTERN PetscBool MFNMonitorRegisterAllCalled;
SLEPC_EXTERN PetscErrorCode MFNRegisterAll(void);
SLEPC_EXTERN PetscErrorCode MFNMonitorRegisterAll(void);
SLEPC_EXTERN PetscLogEvent MFN_SetUp,MFN_Solve;

typedef struct _MFNOps *MFNOps;

struct _MFNOps {
  PetscErrorCode (*solve)(MFN,Vec,Vec);
  PetscErrorCode (*setup)(MFN);
  PetscErrorCode (*setfromoptions)(PetscOptionItems*,MFN);
  PetscErrorCode (*publishoptions)(MFN);
  PetscErrorCode (*destroy)(MFN);
  PetscErrorCode (*reset)(MFN);
  PetscErrorCode (*view)(MFN,PetscViewer);
};

/*
     Maximum number of monitors you can run with a single MFN
*/
#define MAXMFNMONITORS 5

/*
   Defines the MFN data structure.
*/
struct _p_MFN {
  PETSCHEADER(struct _MFNOps);
  /*------------------------- User parameters ---------------------------*/
  Mat            A;                /* the problem matrix */
  FN             fn;               /* which function to compute */
  PetscInt       max_it;           /* maximum number of iterations */
  PetscInt       ncv;              /* number of basis vectors */
  PetscReal      tol;              /* tolerance */
  PetscBool      errorifnotconverged;    /* error out if MFNSolve() does not converge */

  /*-------------- User-provided functions and contexts -----------------*/
  PetscErrorCode (*monitor[MAXMFNMONITORS])(MFN,PetscInt,PetscReal,void*);
  PetscErrorCode (*monitordestroy[MAXMFNMONITORS])(void**);
  void           *monitorcontext[MAXMFNMONITORS];
  PetscInt       numbermonitors;

  /*----------------- Child objects and working data -------------------*/
  BV             V;                /* set of basis vectors */
  Mat            AT;               /* the transpose of A, used in MFNSolveTranspose */
  PetscInt       nwork;            /* number of work vectors */
  Vec            *work;            /* work vectors */
  void           *data;            /* placeholder for solver-specific stuff */

  /* ----------------------- Status variables -------------------------- */
  PetscInt       its;              /* number of iterations so far computed */
  PetscInt       nv;               /* size of current Schur decomposition */
  PetscReal      errest;           /* error estimate */
  PetscReal      bnorm;            /* computed norm of right-hand side in current solve */
  PetscBool      transpose_solve;  /* solve transpose system instead */
  PetscInt       setupcalled;
  MFNConvergedReason reason;
};

/*
   MFN_CreateDenseMat - Creates a dense Mat of size k unless it already has that size
*/
static inline PetscErrorCode MFN_CreateDenseMat(PetscInt k,Mat *A)
{
  PetscBool      create=PETSC_FALSE;
  PetscInt       m,n;

  PetscFunctionBegin;
  if (!*A) create=PETSC_TRUE;
  else {
    CHKERRQ(MatGetSize(*A,&m,&n));
    if (m!=k || n!=k) {
      CHKERRQ(MatDestroy(A));
      create=PETSC_TRUE;
    }
  }
  if (create) {
    CHKERRQ(MatCreateSeqDense(PETSC_COMM_SELF,k,k,NULL,A));
  }
  PetscFunctionReturn(0);
}

/*
   MFN_CreateVec - Creates a Vec of size k unless it already has that size
*/
static inline PetscErrorCode MFN_CreateVec(PetscInt k,Vec *v)
{
  PetscBool      create=PETSC_FALSE;
  PetscInt       n;

  PetscFunctionBegin;
  if (!*v) create=PETSC_TRUE;
  else {
    CHKERRQ(VecGetSize(*v,&n));
    if (n!=k) {
      CHKERRQ(VecDestroy(v));
      create=PETSC_TRUE;
    }
  }
  if (create) {
    CHKERRQ(VecCreateSeq(PETSC_COMM_SELF,k,v));
  }
  PetscFunctionReturn(0);
}

#endif
