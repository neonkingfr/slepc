/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   SVD routines for setting up the solver
*/

#include <slepc/private/svdimpl.h>      /*I "slepcsvd.h" I*/

/*@
   SVDSetOperators - Set the matrices associated with the singular value problem.

   Collective on svd

   Input Parameters:
+  svd - the singular value solver context
.  A   - the matrix associated with the singular value problem
-  B   - the second matrix in the case of GSVD

   Level: beginner

.seealso: SVDSolve(), SVDGetOperators()
@*/
PetscErrorCode SVDSetOperators(SVD svd,Mat A,Mat B)
{
  PetscInt       Ma,Na,Mb,Nb,ma,na,mb,nb,M0,N0,m0,n0;
  PetscBool      samesize=PETSC_TRUE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidHeaderSpecific(A,MAT_CLASSID,2);
  if (B) PetscValidHeaderSpecific(B,MAT_CLASSID,3);
  PetscCheckSameComm(svd,1,A,2);
  if (B) PetscCheckSameComm(svd,1,B,3);

  /* Check matrix sizes */
  CHKERRQ(MatGetSize(A,&Ma,&Na));
  CHKERRQ(MatGetLocalSize(A,&ma,&na));
  if (svd->OP) {
    CHKERRQ(MatGetSize(svd->OP,&M0,&N0));
    CHKERRQ(MatGetLocalSize(svd->OP,&m0,&n0));
    if (M0!=Ma || N0!=Na || m0!=ma || n0!=na) samesize = PETSC_FALSE;
  }
  if (B) {
    CHKERRQ(MatGetSize(B,&Mb,&Nb));
    CHKERRQ(MatGetLocalSize(B,&mb,&nb));
    PetscCheck(Na==Nb,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_WRONG,"Different number of columns in A (%" PetscInt_FMT ") and B (%" PetscInt_FMT ")",Na,Nb);
    PetscCheck(na==nb,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_WRONG,"Different local column size in A (%" PetscInt_FMT ") and B (%" PetscInt_FMT ")",na,nb);
    if (svd->OPb) {
      CHKERRQ(MatGetSize(svd->OPb,&M0,&N0));
      CHKERRQ(MatGetLocalSize(svd->OPb,&m0,&n0));
      if (M0!=Mb || N0!=Nb || m0!=mb || n0!=nb) samesize = PETSC_FALSE;
    }
  }

  CHKERRQ(PetscObjectReference((PetscObject)A));
  if (B) CHKERRQ(PetscObjectReference((PetscObject)B));
  if (svd->state && !samesize) {
    CHKERRQ(SVDReset(svd));
  } else {
    CHKERRQ(MatDestroy(&svd->OP));
    CHKERRQ(MatDestroy(&svd->OPb));
    CHKERRQ(MatDestroy(&svd->A));
    CHKERRQ(MatDestroy(&svd->B));
    CHKERRQ(MatDestroy(&svd->AT));
    CHKERRQ(MatDestroy(&svd->BT));
  }
  svd->nrma = 0.0;
  svd->nrmb = 0.0;
  svd->OP   = A;
  svd->OPb  = B;
  svd->state = SVD_STATE_INITIAL;
  PetscFunctionReturn(0);
}

/*@
   SVDGetOperators - Get the matrices associated with the singular value problem.

   Collective on svd

   Input Parameter:
.  svd - the singular value solver context

   Output Parameters:
+  A  - the matrix associated with the singular value problem
-  B  - the second matrix in the case of GSVD

   Level: intermediate

.seealso: SVDSolve(), SVDSetOperators()
@*/
PetscErrorCode SVDGetOperators(SVD svd,Mat *A,Mat *B)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (A) *A = svd->OP;
  if (B) *B = svd->OPb;
  PetscFunctionReturn(0);
}

/*@
   SVDSetUp - Sets up all the internal data structures necessary for the
   execution of the singular value solver.

   Collective on svd

   Input Parameter:
.  svd   - singular value solver context

   Notes:
   This function need not be called explicitly in most cases, since SVDSolve()
   calls it. It can be useful when one wants to measure the set-up time
   separately from the solve time.

   Level: developer

.seealso: SVDCreate(), SVDSolve(), SVDDestroy()
@*/
PetscErrorCode SVDSetUp(SVD svd)
{
  PetscBool      flg;
  PetscInt       M,N,P=0,k,maxnsol;
  SlepcSC        sc;
  Vec            *T;
  BV             bv;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (svd->state) PetscFunctionReturn(0);
  CHKERRQ(PetscLogEventBegin(SVD_SetUp,svd,0,0,0));

  /* reset the convergence flag from the previous solves */
  svd->reason = SVD_CONVERGED_ITERATING;

  /* Set default solver type (SVDSetFromOptions was not called) */
  if (!((PetscObject)svd)->type_name) {
    CHKERRQ(SVDSetType(svd,SVDCROSS));
  }
  if (!svd->ds) CHKERRQ(SVDGetDS(svd,&svd->ds));

  /* check matrices */
  PetscCheck(svd->OP,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_WRONGSTATE,"SVDSetOperators() must be called first");

  /* Set default problem type */
  if (!svd->problem_type) {
    if (svd->OPb) {
      CHKERRQ(SVDSetProblemType(svd,SVD_GENERALIZED));
    } else {
      CHKERRQ(SVDSetProblemType(svd,SVD_STANDARD));
    }
  } else if (!svd->OPb && svd->isgeneralized) {
    CHKERRQ(PetscInfo(svd,"Problem type set as generalized but no matrix B was provided; reverting to a standard singular value problem\n"));
    svd->isgeneralized = PETSC_FALSE;
    svd->problem_type = SVD_STANDARD;
  } else PetscCheck(!svd->OPb || svd->isgeneralized,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_INCOMP,"Inconsistent SVD state: the problem type does not match the number of matrices");

  /* determine how to handle the transpose */
  svd->expltrans = PETSC_TRUE;
  if (svd->impltrans) svd->expltrans = PETSC_FALSE;
  else {
    CHKERRQ(MatHasOperation(svd->OP,MATOP_TRANSPOSE,&flg));
    if (!flg) svd->expltrans = PETSC_FALSE;
    else {
      CHKERRQ(PetscObjectTypeCompareAny((PetscObject)svd,&flg,SVDLAPACK,SVDSCALAPACK,SVDELEMENTAL,""));
      if (flg) svd->expltrans = PETSC_FALSE;
    }
  }

  /* get matrix dimensions */
  CHKERRQ(MatGetSize(svd->OP,&M,&N));
  if (svd->isgeneralized) {
    CHKERRQ(MatGetSize(svd->OPb,&P,NULL));
    PetscCheck(M+P>=N,PetscObjectComm((PetscObject)svd),PETSC_ERR_SUP,"The case when [A;B] has less rows than columns is not supported");
  }

  /* build transpose matrix */
  CHKERRQ(MatDestroy(&svd->A));
  CHKERRQ(MatDestroy(&svd->AT));
  CHKERRQ(PetscObjectReference((PetscObject)svd->OP));
  if (svd->expltrans) {
    if (svd->isgeneralized || M>=N) {
      svd->A = svd->OP;
      CHKERRQ(MatHermitianTranspose(svd->OP,MAT_INITIAL_MATRIX,&svd->AT));
    } else {
      CHKERRQ(MatHermitianTranspose(svd->OP,MAT_INITIAL_MATRIX,&svd->A));
      svd->AT = svd->OP;
    }
  } else {
    if (svd->isgeneralized || M>=N) {
      svd->A = svd->OP;
      CHKERRQ(MatCreateHermitianTranspose(svd->OP,&svd->AT));
    } else {
      CHKERRQ(MatCreateHermitianTranspose(svd->OP,&svd->A));
      svd->AT = svd->OP;
    }
  }

  /* build transpose matrix B for GSVD */
  if (svd->isgeneralized) {
    CHKERRQ(MatDestroy(&svd->B));
    CHKERRQ(MatDestroy(&svd->BT));
    CHKERRQ(PetscObjectReference((PetscObject)svd->OPb));
    if (svd->expltrans) {
      svd->B = svd->OPb;
      CHKERRQ(MatHermitianTranspose(svd->OPb,MAT_INITIAL_MATRIX,&svd->BT));
    } else {
      svd->B = svd->OPb;
      CHKERRQ(MatCreateHermitianTranspose(svd->OPb,&svd->BT));
    }
  }

  if (!svd->isgeneralized && M<N) {
    /* swap initial vectors */
    if (svd->nini || svd->ninil) {
      T=svd->ISL; svd->ISL=svd->IS; svd->IS=T;
      k=svd->ninil; svd->ninil=svd->nini; svd->nini=k;
    }
    /* swap basis vectors */
    if (!svd->swapped) {  /* only the first time in case of multiple calls */
      bv=svd->V; svd->V=svd->U; svd->U=bv;
      svd->swapped = PETSC_TRUE;
    }
  }

  maxnsol = svd->isgeneralized? PetscMin(PetscMin(M,N),P): PetscMin(M,N);
  svd->ncv = PetscMin(svd->ncv,maxnsol);
  svd->nsv = PetscMin(svd->nsv,maxnsol);
  PetscCheck(svd->ncv==PETSC_DEFAULT || svd->nsv<=svd->ncv,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"nsv bigger than ncv");

  /* relative convergence criterion is not allowed in GSVD */
  if (svd->conv==(SVDConv)-1) CHKERRQ(SVDSetConvergenceTest(svd,svd->isgeneralized?SVD_CONV_NORM:SVD_CONV_REL));
  PetscCheck(!svd->isgeneralized || svd->conv!=SVD_CONV_REL,PetscObjectComm((PetscObject)svd),PETSC_ERR_SUP,"Relative convergence criterion is not allowed in GSVD");

  /* initialization of matrix norm (stardard case only, for GSVD it is done inside setup()) */
  if (!svd->isgeneralized && svd->conv==SVD_CONV_NORM && !svd->nrma) CHKERRQ(MatNorm(svd->OP,NORM_INFINITY,&svd->nrma));

  /* call specific solver setup */
  CHKERRQ((*svd->ops->setup)(svd));

  /* set tolerance if not yet set */
  if (svd->tol==PETSC_DEFAULT) svd->tol = SLEPC_DEFAULT_TOL;

  /* fill sorting criterion context */
  CHKERRQ(DSGetSlepcSC(svd->ds,&sc));
  sc->comparison    = (svd->which==SVD_LARGEST)? SlepcCompareLargestReal: SlepcCompareSmallestReal;
  sc->comparisonctx = NULL;
  sc->map           = NULL;
  sc->mapobj        = NULL;

  /* process initial vectors */
  if (svd->nini<0) {
    k = -svd->nini;
    PetscCheck(k<=svd->ncv,PetscObjectComm((PetscObject)svd),PETSC_ERR_USER_INPUT,"The number of initial vectors is larger than ncv");
    CHKERRQ(BVInsertVecs(svd->V,0,&k,svd->IS,PETSC_TRUE));
    CHKERRQ(SlepcBasisDestroy_Private(&svd->nini,&svd->IS));
    svd->nini = k;
  }
  if (svd->ninil<0) {
    k = 0;
    if (svd->leftbasis) {
      k = -svd->ninil;
      PetscCheck(k<=svd->ncv,PetscObjectComm((PetscObject)svd),PETSC_ERR_USER_INPUT,"The number of left initial vectors is larger than ncv");
      CHKERRQ(BVInsertVecs(svd->U,0,&k,svd->ISL,PETSC_TRUE));
    } else {
      CHKERRQ(PetscInfo(svd,"Ignoring initial left vectors\n"));
    }
    CHKERRQ(SlepcBasisDestroy_Private(&svd->ninil,&svd->ISL));
    svd->ninil = k;
  }

  CHKERRQ(PetscLogEventEnd(SVD_SetUp,svd,0,0,0));
  svd->state = SVD_STATE_SETUP;
  PetscFunctionReturn(0);
}

/*@C
   SVDSetInitialSpaces - Specify two basis of vectors that constitute the initial
   right and/or left spaces.

   Collective on svd

   Input Parameters:
+  svd   - the singular value solver context
.  nr    - number of right vectors
.  isr   - set of basis vectors of the right initial space
.  nl    - number of left vectors
-  isl   - set of basis vectors of the left initial space

   Notes:
   The initial right and left spaces are rough approximations to the right and/or
   left singular subspaces from which the solver starts to iterate.
   It is not necessary to provide both sets of vectors.

   Some solvers start to iterate on a single vector (initial vector). In that case,
   the other vectors are ignored.

   These vectors do not persist from one SVDSolve() call to the other, so the
   initial space should be set every time.

   The vectors do not need to be mutually orthonormal, since they are explicitly
   orthonormalized internally.

   Common usage of this function is when the user can provide a rough approximation
   of the wanted singular space. Then, convergence may be faster.

   Level: intermediate

.seealso: SVDSetUp()
@*/
PetscErrorCode SVDSetInitialSpaces(SVD svd,PetscInt nr,Vec isr[],PetscInt nl,Vec isl[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveInt(svd,nr,2);
  PetscValidLogicalCollectiveInt(svd,nl,4);
  PetscCheck(nr>=0,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Argument nr cannot be negative");
  if (nr>0) {
    PetscValidPointer(isr,3);
    PetscValidHeaderSpecific(*isr,VEC_CLASSID,3);
  }
  PetscCheck(nl>=0,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Argument nl cannot be negative");
  if (nl>0) {
    PetscValidPointer(isl,5);
    PetscValidHeaderSpecific(*isl,VEC_CLASSID,5);
  }
  CHKERRQ(SlepcBasisReference_Private(nr,isr,&svd->nini,&svd->IS));
  CHKERRQ(SlepcBasisReference_Private(nl,isl,&svd->ninil,&svd->ISL));
  if (nr>0 || nl>0) svd->state = SVD_STATE_INITIAL;
  PetscFunctionReturn(0);
}

/*
  SVDSetDimensions_Default - Set reasonable values for ncv, mpd if not set
  by the user. This is called at setup.
 */
PetscErrorCode SVDSetDimensions_Default(SVD svd)
{
  PetscInt       N,M,P,maxnsol;

  PetscFunctionBegin;
  CHKERRQ(MatGetSize(svd->OP,&M,&N));
  maxnsol = PetscMin(M,N);
  if (svd->isgeneralized) {
    CHKERRQ(MatGetSize(svd->OPb,&P,NULL));
    maxnsol = PetscMin(maxnsol,P);
  }
  if (svd->ncv!=PETSC_DEFAULT) { /* ncv set */
    PetscCheck(svd->ncv>=svd->nsv,PetscObjectComm((PetscObject)svd),PETSC_ERR_USER_INPUT,"The value of ncv must be at least nsv");
  } else if (svd->mpd!=PETSC_DEFAULT) { /* mpd set */
    svd->ncv = PetscMin(maxnsol,svd->nsv+svd->mpd);
  } else { /* neither set: defaults depend on nsv being small or large */
    if (svd->nsv<500) svd->ncv = PetscMin(maxnsol,PetscMax(2*svd->nsv,10));
    else {
      svd->mpd = 500;
      svd->ncv = PetscMin(maxnsol,svd->nsv+svd->mpd);
    }
  }
  if (svd->mpd==PETSC_DEFAULT) svd->mpd = svd->ncv;
  PetscFunctionReturn(0);
}

/*@
   SVDAllocateSolution - Allocate memory storage for common variables such
   as the singular values and the basis vectors.

   Collective on svd

   Input Parameters:
+  svd   - eigensolver context
-  extra - number of additional positions, used for methods that require a
           working basis slightly larger than ncv

   Developer Notes:
   This is SLEPC_EXTERN because it may be required by user plugin SVD
   implementations.

   This is called at setup after setting the value of ncv and the flag leftbasis.

   Level: developer

.seealso: SVDSetUp()
@*/
PetscErrorCode SVDAllocateSolution(SVD svd,PetscInt extra)
{
  PetscInt       oldsize,requested;
  Vec            tr,tl;

  PetscFunctionBegin;
  requested = svd->ncv + extra;

  /* oldsize is zero if this is the first time setup is called */
  CHKERRQ(BVGetSizes(svd->V,NULL,NULL,&oldsize));

  /* allocate sigma */
  if (requested != oldsize || !svd->sigma) {
    CHKERRQ(PetscFree3(svd->sigma,svd->perm,svd->errest));
    CHKERRQ(PetscMalloc3(requested,&svd->sigma,requested,&svd->perm,requested,&svd->errest));
    CHKERRQ(PetscLogObjectMemory((PetscObject)svd,PetscMax(0,requested-oldsize)*(2*sizeof(PetscReal)+sizeof(PetscInt))));
  }
  /* allocate V */
  if (!svd->V) CHKERRQ(SVDGetBV(svd,&svd->V,NULL));
  if (!oldsize) {
    if (!((PetscObject)(svd->V))->type_name) {
      CHKERRQ(BVSetType(svd->V,BVSVEC));
    }
    CHKERRQ(MatCreateVecsEmpty(svd->A,&tr,NULL));
    CHKERRQ(BVSetSizesFromVec(svd->V,tr,requested));
    CHKERRQ(VecDestroy(&tr));
  } else {
    CHKERRQ(BVResize(svd->V,requested,PETSC_FALSE));
  }
  /* allocate U */
  if (svd->leftbasis && !svd->isgeneralized) {
    if (!svd->U) CHKERRQ(SVDGetBV(svd,NULL,&svd->U));
    if (!oldsize) {
      if (!((PetscObject)(svd->U))->type_name) {
        CHKERRQ(BVSetType(svd->U,BVSVEC));
      }
      CHKERRQ(MatCreateVecsEmpty(svd->A,NULL,&tl));
      CHKERRQ(BVSetSizesFromVec(svd->U,tl,requested));
      CHKERRQ(VecDestroy(&tl));
    } else {
      CHKERRQ(BVResize(svd->U,requested,PETSC_FALSE));
    }
  } else if (svd->isgeneralized) {  /* left basis for the GSVD */
    if (!svd->U) CHKERRQ(SVDGetBV(svd,NULL,&svd->U));
    if (!oldsize) {
      if (!((PetscObject)(svd->U))->type_name) {
        CHKERRQ(BVSetType(svd->U,BVSVEC));
      }
      CHKERRQ(SVDCreateLeftTemplate(svd,&tl));
      CHKERRQ(BVSetSizesFromVec(svd->U,tl,requested));
      CHKERRQ(VecDestroy(&tl));
    } else {
      CHKERRQ(BVResize(svd->U,requested,PETSC_FALSE));
    }
  }
  PetscFunctionReturn(0);
}
