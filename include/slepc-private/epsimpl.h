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

#if !defined(_EPSIMPL)
#define _EPSIMPL

#include <slepceps.h>
#include <slepc-private/slepcimpl.h>

PETSC_EXTERN PetscLogEvent EPS_SetUp,EPS_Solve;

typedef struct _EPSOps *EPSOps;

struct _EPSOps {
  PetscErrorCode  (*solve)(EPS);
  PetscErrorCode  (*setup)(EPS);
  PetscErrorCode  (*setfromoptions)(EPS);
  PetscErrorCode  (*publishoptions)(EPS);
  PetscErrorCode  (*destroy)(EPS);
  PetscErrorCode  (*reset)(EPS);
  PetscErrorCode  (*view)(EPS,PetscViewer);
  PetscErrorCode  (*backtransform)(EPS);
  PetscErrorCode  (*computevectors)(EPS);
};

/*
     Maximum number of monitors you can run with a single EPS
*/
#define MAXEPSMONITORS 5

/*
   Defines the EPS data structure.
*/
struct _p_EPS {
  PETSCHEADER(struct _EPSOps);
  /*------------------------- User parameters ---------------------------*/
  PetscInt       max_it;           /* maximum number of iterations */
  PetscInt       nev;              /* number of eigenvalues to compute */
  PetscInt       ncv;              /* number of basis vectors */
  PetscInt       mpd;              /* maximum dimension of projected problem */
  PetscInt       nini,ninil;       /* number of initial vectors (negative means not copied yet) */
  PetscInt       nds;              /* number of basis vectors of deflation space */
  PetscScalar    target;           /* target value */
  PetscReal      tol;              /* tolerance */
  EPSConv        conv;             /* convergence test */
  EPSWhich       which;            /* which part of the spectrum to be sought */
  PetscBool      leftvecs;         /* if left eigenvectors are requested */
  PetscReal      inta,intb;        /* interval [a,b] for spectrum slicing */
  EPSProblemType problem_type;     /* which kind of problem to be solved */
  EPSExtraction  extraction;       /* which kind of extraction to be applied */
  EPSBalance     balance;          /* the balancing method */
  PetscInt       balance_its;      /* number of iterations of the balancing method */
  PetscReal      balance_cutoff;   /* cutoff value for balancing */
  PetscBool      trueres;          /* whether the true residual norm must be computed */
  PetscBool      trackall;         /* whether all the residuals must be computed */

  /*-------------- User-provided functions and contexts -----------------*/
  PetscErrorCode (*comparison)(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*);
  PetscErrorCode (*converged)(EPS,PetscScalar,PetscScalar,PetscReal,PetscReal*,void*);
  PetscErrorCode (*arbitrary)(PetscScalar,PetscScalar,Vec,Vec,PetscScalar*,PetscScalar*,void*);
  void           *comparisonctx;
  void           *convergedctx;
  void           *arbitraryctx;
  PetscErrorCode (*monitor[MAXEPSMONITORS])(EPS,PetscInt,PetscInt,PetscScalar*,PetscScalar*,PetscReal*,PetscInt,void*);
  PetscErrorCode (*monitordestroy[MAXEPSMONITORS])(void**);
  void           *monitorcontext[MAXEPSMONITORS];
  PetscInt       numbermonitors;

  /*----------------- Child objects and working data -------------------*/
  ST             st;               /* spectral transformation object */
  DS             ds;               /* direct solver object */
  BV             V;                /* set of basis vectors and computed eigenvectors */
  BV             W;                /* set of left basis vectors and computed left eigenvectors */
  PetscRandom    rand;             /* random number generator */
  Vec            D;                /* diagonal matrix for balancing */
  Vec            *IS,*ISL;         /* references to user-provided initial space */
  Vec            *defl;            /* references to user-provided deflation space */
  PetscScalar    *eigr,*eigi;      /* real and imaginary parts of eigenvalues */
  PetscReal      *errest;          /* error estimates */
  PetscReal      *errest_left;     /* left error estimates */
  PetscScalar    *rr,*ri;          /* values computed by user's arbitrary selection function */
  PetscInt       *perm;            /* permutation for eigenvalue ordering */
  PetscInt       nwork;            /* number of work vectors */
  Vec            *work;            /* work vectors */
  void           *data;            /* placeholder for solver-specific stuff */

  /* ----------------------- Status variables --------------------------*/
  PetscInt       nconv;            /* number of converged eigenvalues */
  PetscInt       its;              /* number of iterations so far computed */
  PetscBool      evecsavailable;   /* computed eigenvectors */
  PetscInt       nv;               /* size of current Schur decomposition */
  PetscInt       n,nloc;           /* problem dimensions (global, local) */
  PetscReal      nrma,nrmb;        /* computed matrix norms */
  PetscInt       setupcalled;
  PetscBool      isgeneralized;
  PetscBool      ispositive;
  PetscBool      ishermitian;
  EPSConvergedReason reason;
};

PETSC_INTERN PetscErrorCode EPSReset_Default(EPS);
PETSC_INTERN PetscErrorCode EPSSetWhichEigenpairs_Default(EPS);
PETSC_INTERN PetscErrorCode EPSAllocateSolution(EPS,PetscInt);
PETSC_INTERN PetscErrorCode EPSSetDimensions_Default(EPS);
PETSC_INTERN PetscErrorCode EPSBackTransform_Default(EPS);
PETSC_INTERN PetscErrorCode EPSComputeVectors_Default(EPS);
PETSC_INTERN PetscErrorCode EPSComputeVectors_Hermitian(EPS);
PETSC_INTERN PetscErrorCode EPSComputeVectors_Schur(EPS);
PETSC_INTERN PetscErrorCode EPSComputeResidualNorm_Private(EPS,PetscScalar,PetscScalar,Vec,Vec,PetscReal*);
PETSC_INTERN PetscErrorCode EPSComputeRelativeError_Private(EPS,PetscScalar,PetscScalar,Vec,Vec,PetscReal*);
PETSC_INTERN PetscErrorCode EPSComputeRitzVector(EPS,PetscScalar*,PetscScalar*,BV,Vec,Vec);
PETSC_INTERN PetscErrorCode EPSGetStartVector(EPS,PetscInt,PetscBool*);
PETSC_INTERN PetscErrorCode EPSGetStartVectorLeft(EPS,PetscInt,PetscBool*);

/* Private functions of the solver implementations */

PETSC_INTERN PetscErrorCode EPSBasicArnoldi(EPS,PetscBool,PetscScalar*,PetscInt,PetscInt,PetscInt*,PetscReal*,PetscBool*);
PETSC_INTERN PetscErrorCode EPSDelayedArnoldi(EPS,PetscScalar*,PetscInt,Vec*,PetscInt,PetscInt*,Vec,PetscReal*,PetscBool*);
PETSC_INTERN PetscErrorCode EPSDelayedArnoldi1(EPS,PetscScalar*,PetscInt,Vec*,PetscInt,PetscInt*,Vec,PetscReal*,PetscBool*);
PETSC_INTERN PetscErrorCode EPSKrylovConvergence(EPS,PetscBool,PetscInt,PetscInt,PetscReal,PetscReal,PetscInt*);
PETSC_INTERN PetscErrorCode EPSFullLanczos(EPS,PetscReal*,PetscReal*,PetscInt,PetscInt*,PetscBool*);
PETSC_INTERN PetscErrorCode EPSBuildBalance_Krylov(EPS);

#endif
