/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#if !defined(SLEPCSVDIMPL_H)
#define SLEPCSVDIMPL_H

#include <slepcsvd.h>
#include <slepc/private/slepcimpl.h>

SLEPC_EXTERN PetscBool SVDRegisterAllCalled;
SLEPC_EXTERN PetscBool SVDMonitorRegisterAllCalled;
SLEPC_EXTERN PetscErrorCode SVDRegisterAll(void);
SLEPC_EXTERN PetscErrorCode SVDMonitorRegisterAll(void);
SLEPC_EXTERN PetscLogEvent SVD_SetUp,SVD_Solve;

typedef struct _SVDOps *SVDOps;

struct _SVDOps {
  PetscErrorCode (*solve)(SVD);
  PetscErrorCode (*solveg)(SVD);
  PetscErrorCode (*setup)(SVD);
  PetscErrorCode (*setfromoptions)(PetscOptionItems*,SVD);
  PetscErrorCode (*publishoptions)(SVD);
  PetscErrorCode (*destroy)(SVD);
  PetscErrorCode (*reset)(SVD);
  PetscErrorCode (*view)(SVD,PetscViewer);
  PetscErrorCode (*computevectors)(SVD);
};

/*
     Maximum number of monitors you can run with a single SVD
*/
#define MAXSVDMONITORS 5

typedef enum { SVD_STATE_INITIAL,
               SVD_STATE_SETUP,
               SVD_STATE_SOLVED,
               SVD_STATE_VECTORS } SVDStateType;

/*
   To check for unsupported features at SVDSetUp_XXX()
*/
typedef enum { SVD_FEATURE_CONVERGENCE=16,  /* convergence test selected by user */
               SVD_FEATURE_STOPPING=32      /* stopping test */
             } SVDFeatureType;

/*
   Defines the SVD data structure.
*/
struct _p_SVD {
  PETSCHEADER(struct _SVDOps);
  /*------------------------- User parameters ---------------------------*/
  Mat            OP,OPb;           /* problem matrices */
  PetscInt       max_it;           /* max iterations */
  PetscInt       nsv;              /* number of requested values */
  PetscInt       ncv;              /* basis size */
  PetscInt       mpd;              /* maximum dimension of projected problem */
  PetscInt       nini,ninil;       /* number of initial vecs (negative means not copied yet) */
  PetscReal      tol;              /* tolerance */
  SVDConv        conv;             /* convergence test */
  SVDStop        stop;             /* stopping test */
  SVDWhich       which;            /* which singular values are computed */
  SVDProblemType problem_type;     /* which kind of problem to be solved */
  PetscBool      impltrans;        /* implicit transpose mode */
  PetscBool      trackall;         /* whether all the residuals must be computed */

  /*-------------- User-provided functions and contexts -----------------*/
  PetscErrorCode (*converged)(SVD,PetscReal,PetscReal,PetscReal*,void*);
  PetscErrorCode (*convergeduser)(SVD,PetscReal,PetscReal,PetscReal*,void*);
  PetscErrorCode (*convergeddestroy)(void*);
  PetscErrorCode (*stopping)(SVD,PetscInt,PetscInt,PetscInt,PetscInt,SVDConvergedReason*,void*);
  PetscErrorCode (*stoppinguser)(SVD,PetscInt,PetscInt,PetscInt,PetscInt,SVDConvergedReason*,void*);
  PetscErrorCode (*stoppingdestroy)(void*);
  void           *convergedctx;
  void           *stoppingctx;
  PetscErrorCode (*monitor[MAXSVDMONITORS])(SVD,PetscInt,PetscInt,PetscReal*,PetscReal*,PetscInt,void*);
  PetscErrorCode (*monitordestroy[MAXSVDMONITORS])(void**);
  void           *monitorcontext[MAXSVDMONITORS];
  PetscInt       numbermonitors;

  /*----------------- Child objects and working data -------------------*/
  DS             ds;               /* direct solver object */
  BV             U,V;              /* left and right singular vectors */
  SlepcSC        sc;               /* sorting criterion data */
  Mat            A,B;              /* problem matrices */
  Mat            AT,BT;            /* transposed matrices */
  Vec            *IS,*ISL;         /* placeholder for references to user initial space */
  PetscReal      *sigma;           /* singular values */
  PetscReal      *errest;          /* error estimates */
  PetscInt       *perm;            /* permutation for singular value ordering */
  PetscInt       nworkl,nworkr;    /* number of work vectors */
  Vec            *workl,*workr;    /* work vectors */
  void           *data;            /* placeholder for solver-specific stuff */

  /* ----------------------- Status variables -------------------------- */
  SVDStateType   state;            /* initial -> setup -> solved -> vectors */
  PetscInt       nconv;            /* number of converged values */
  PetscInt       its;              /* iteration counter */
  PetscBool      leftbasis;        /* if U is filled by the solver */
  PetscBool      swapped;          /* the U and V bases have been swapped (M<N) */
  PetscBool      expltrans;        /* explicit transpose created */
  PetscBool      isgeneralized;
  SVDConvergedReason reason;
};

/*
    Macros to test valid SVD arguments
*/
#if !defined(PETSC_USE_DEBUG)

#define SVDCheckSolved(h,arg) do {(void)(h);} while (0)

#else

#define SVDCheckSolved(h,arg) \
  do { \
    if ((h)->state<SVD_STATE_SOLVED) SETERRQ1(PetscObjectComm((PetscObject)(h)),PETSC_ERR_ARG_WRONGSTATE,"Must call SVDSolve() first: Parameter #%d",arg); \
  } while (0)

#endif

/*
    Macros to check settings at SVDSetUp()
*/

/* SVDCheckStandard: the problem is not GSVD */
#define SVDCheckStandardCondition(svd,condition,msg) \
  do { \
    if (condition) { \
      if ((svd)->isgeneralized) SETERRQ2(PetscObjectComm((PetscObject)(svd)),PETSC_ERR_SUP,"The solver '%s'%s cannot be used for generalized problems",((PetscObject)(svd))->type_name,(msg)); \
    } \
  } while (0)
#define SVDCheckStandard(svd) SVDCheckStandardCondition(svd,PETSC_TRUE,"")

/* Check for unsupported features */
#define SVDCheckUnsupportedCondition(svd,mask,condition,msg) \
  do { \
    if (condition) { \
      if (((mask) & SVD_FEATURE_CONVERGENCE) && (svd)->converged!=SVDConvergedRelative) SETERRQ2(PetscObjectComm((PetscObject)(svd)),PETSC_ERR_SUP,"The solver '%s'%s only supports the default convergence test",((PetscObject)(svd))->type_name,(msg)); \
      if (((mask) & SVD_FEATURE_STOPPING) && (svd)->stopping!=SVDStoppingBasic) SETERRQ2(PetscObjectComm((PetscObject)(svd)),PETSC_ERR_SUP,"The solver '%s'%s only supports the default stopping test",((PetscObject)(svd))->type_name,(msg)); \
    } \
  } while (0)
#define SVDCheckUnsupported(svd,mask) SVDCheckUnsupportedCondition(svd,mask,PETSC_TRUE,"")

/* Check for ignored features */
#define SVDCheckIgnoredCondition(svd,mask,condition,msg) \
  do { \
    PetscErrorCode __ierr; \
    if (condition) { \
      if (((mask) & SVD_FEATURE_CONVERGENCE) && (svd)->converged!=SVDConvergedRelative) { __ierr = PetscInfo2((svd),"The solver '%s'%s ignores the convergence test settings\n",((PetscObject)(svd))->type_name,(msg)); } \
      if (((mask) & SVD_FEATURE_STOPPING) && (svd)->stopping!=SVDStoppingBasic) { __ierr = PetscInfo2((svd),"The solver '%s'%s ignores the stopping test settings\n",((PetscObject)(svd))->type_name,(msg)); } \
    } \
  } while (0)
#define SVDCheckIgnored(svd,mask) SVDCheckIgnoredCondition(svd,mask,PETSC_TRUE,"")

/*
   Create the template vector for the left basis in GSVD, as in
   MatCreateVecsEmpty(Z,NULL,&t) for Z=[A;B] without forming Z.
 */
PETSC_STATIC_INLINE PetscErrorCode SVDCreateLeftTemplate(SVD svd,Vec *t)
{
  PetscErrorCode ierr;
  PetscInt       M,P,m,p;
  Vec            v1,v2;
  VecType        vec_type;

  PetscFunctionBegin;
  ierr = MatCreateVecsEmpty(svd->OP,NULL,&v1);CHKERRQ(ierr);
  ierr = VecGetSize(v1,&M);CHKERRQ(ierr);
  ierr = VecGetLocalSize(v1,&m);CHKERRQ(ierr);
  ierr = VecGetType(v1,&vec_type);CHKERRQ(ierr);
  ierr = MatCreateVecsEmpty(svd->OPb,NULL,&v2);CHKERRQ(ierr);
  ierr = VecGetSize(v2,&P);CHKERRQ(ierr);
  ierr = VecGetLocalSize(v2,&p);CHKERRQ(ierr);
  ierr = VecCreate(PetscObjectComm((PetscObject)(v1)),t);CHKERRQ(ierr);
  ierr = VecSetType(*t,vec_type);CHKERRQ(ierr);
  ierr = VecSetSizes(*t,m+p,M+P);CHKERRQ(ierr);
  ierr = VecSetUp(*t);CHKERRQ(ierr);
  ierr = VecDestroy(&v1);CHKERRQ(ierr);
  ierr = VecDestroy(&v2);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

SLEPC_INTERN PetscErrorCode SVDTwoSideLanczos(SVD,PetscReal*,PetscReal*,BV,BV,PetscInt,PetscInt*,PetscBool*);
SLEPC_INTERN PetscErrorCode SVDSetDimensions_Default(SVD);
SLEPC_INTERN PetscErrorCode SVDComputeVectors(SVD);
SLEPC_INTERN PetscErrorCode SVDComputeVectors_Left(SVD);

#endif
