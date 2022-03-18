/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   PEP routines related to the solution process

   References:

       [1] C. Campos and J.E. Roman, "Parallel iterative refinement in
           polynomial eigenvalue problems", Numer. Linear Algebra Appl.
           23(4):730-745, 2016.
*/

#include <slepc/private/pepimpl.h>       /*I "slepcpep.h" I*/
#include <slepc/private/bvimpl.h>
#include <petscdraw.h>

static PetscBool  cited = PETSC_FALSE;
static const char citation[] =
  "@Article{slepc-pep-refine,\n"
  "   author = \"C. Campos and J. E. Roman\",\n"
  "   title = \"Parallel iterative refinement in polynomial eigenvalue problems\",\n"
  "   journal = \"Numer. Linear Algebra Appl.\",\n"
  "   volume = \"23\",\n"
  "   number = \"4\",\n"
  "   pages = \"730--745\",\n"
  "   year = \"2016,\"\n"
  "   doi = \"https://doi.org/10.1002/nla.2052\"\n"
  "}\n";

PetscErrorCode PEPComputeVectors(PEP pep)
{
  PetscFunctionBegin;
  PEPCheckSolved(pep,1);
  if (pep->state==PEP_STATE_SOLVED && pep->ops->computevectors) {
    CHKERRQ((*pep->ops->computevectors)(pep));
  }
  pep->state = PEP_STATE_EIGENVECTORS;
  PetscFunctionReturn(0);
}

PetscErrorCode PEPExtractVectors(PEP pep)
{
  PetscFunctionBegin;
  PEPCheckSolved(pep,1);
  if (pep->state==PEP_STATE_SOLVED && pep->ops->extractvectors) {
    CHKERRQ((*pep->ops->extractvectors)(pep));
  }
  PetscFunctionReturn(0);
}

/*@
   PEPSolve - Solves the polynomial eigensystem.

   Collective on pep

   Input Parameter:
.  pep - eigensolver context obtained from PEPCreate()

   Options Database Keys:
+  -pep_view - print information about the solver used
.  -pep_view_matk binary - save any of the coefficient matrices (Ak) to the
                default binary viewer (replace k by an integer from 0 to nmat-1)
.  -pep_view_vectors binary - save the computed eigenvectors to the default binary viewer
.  -pep_view_values - print computed eigenvalues
.  -pep_converged_reason - print reason for convergence, and number of iterations
.  -pep_error_absolute - print absolute errors of each eigenpair
.  -pep_error_relative - print relative errors of each eigenpair
-  -pep_error_backward - print backward errors of each eigenpair

   Level: beginner

.seealso: PEPCreate(), PEPSetUp(), PEPDestroy(), PEPSetTolerances()
@*/
PetscErrorCode PEPSolve(PEP pep)
{
  PetscInt       i,k;
  PetscBool      flg,islinear;
  char           str[16];

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  if (pep->state>=PEP_STATE_SOLVED) PetscFunctionReturn(0);
  CHKERRQ(PetscLogEventBegin(PEP_Solve,pep,0,0,0));

  /* call setup */
  CHKERRQ(PEPSetUp(pep));
  pep->nconv = 0;
  pep->its   = 0;
  k = pep->lineariz? pep->ncv: pep->ncv*(pep->nmat-1);
  for (i=0;i<k;i++) {
    pep->eigr[i]   = 0.0;
    pep->eigi[i]   = 0.0;
    pep->errest[i] = 0.0;
    pep->perm[i]   = i;
  }
  CHKERRQ(PEPViewFromOptions(pep,NULL,"-pep_view_pre"));
  CHKERRQ(RGViewFromOptions(pep->rg,NULL,"-rg_view"));

  /* Call solver */
  CHKERRQ((*pep->ops->solve)(pep));
  PetscCheck(pep->reason,PetscObjectComm((PetscObject)pep),PETSC_ERR_PLIB,"Internal error, solver returned without setting converged reason");
  pep->state = PEP_STATE_SOLVED;

  /* Only the first nconv columns contain useful information */
  CHKERRQ(BVSetActiveColumns(pep->V,0,pep->nconv));

  CHKERRQ(PetscObjectTypeCompare((PetscObject)pep,PEPLINEAR,&islinear));
  if (!islinear) {
    CHKERRQ(STPostSolve(pep->st));
    /* Map eigenvalues back to the original problem */
    CHKERRQ(STGetTransform(pep->st,&flg));
    if (flg && pep->ops->backtransform) {
      CHKERRQ((*pep->ops->backtransform)(pep));
    }
  }

#if !defined(PETSC_USE_COMPLEX)
  /* reorder conjugate eigenvalues (positive imaginary first) */
  for (i=0;i<pep->nconv-1;i++) {
    if (pep->eigi[i] != 0) {
      if (pep->eigi[i] < 0) {
        pep->eigi[i] = -pep->eigi[i];
        pep->eigi[i+1] = -pep->eigi[i+1];
        /* the next correction only works with eigenvectors */
        CHKERRQ(PEPComputeVectors(pep));
        CHKERRQ(BVScaleColumn(pep->V,i+1,-1.0));
      }
      i++;
    }
  }
#endif

  if (pep->refine!=PEP_REFINE_NONE) {
    CHKERRQ(PetscCitationsRegister(citation,&cited));
  }

  if (pep->refine==PEP_REFINE_SIMPLE && pep->rits>0 && pep->nconv>0) {
    CHKERRQ(PEPComputeVectors(pep));
    CHKERRQ(PEPNewtonRefinementSimple(pep,&pep->rits,pep->rtol,pep->nconv));
  }

  /* sort eigenvalues according to pep->which parameter */
  CHKERRQ(SlepcSortEigenvalues(pep->sc,pep->nconv,pep->eigr,pep->eigi,pep->perm));
  CHKERRQ(PetscLogEventEnd(PEP_Solve,pep,0,0,0));

  /* various viewers */
  CHKERRQ(PEPViewFromOptions(pep,NULL,"-pep_view"));
  CHKERRQ(PEPConvergedReasonViewFromOptions(pep));
  CHKERRQ(PEPErrorViewFromOptions(pep));
  CHKERRQ(PEPValuesViewFromOptions(pep));
  CHKERRQ(PEPVectorsViewFromOptions(pep));
  for (i=0;i<pep->nmat;i++) {
    CHKERRQ(PetscSNPrintf(str,sizeof(str),"-pep_view_mat%" PetscInt_FMT,i));
    CHKERRQ(MatViewFromOptions(pep->A[i],(PetscObject)pep,str));
  }

  /* Remove the initial subspace */
  pep->nini = 0;
  PetscFunctionReturn(0);
}

/*@
   PEPGetIterationNumber - Gets the current iteration number. If the
   call to PEPSolve() is complete, then it returns the number of iterations
   carried out by the solution method.

   Not Collective

   Input Parameter:
.  pep - the polynomial eigensolver context

   Output Parameter:
.  its - number of iterations

   Note:
   During the i-th iteration this call returns i-1. If PEPSolve() is
   complete, then parameter "its" contains either the iteration number at
   which convergence was successfully reached, or failure was detected.
   Call PEPGetConvergedReason() to determine if the solver converged or
   failed and why.

   Level: intermediate

.seealso: PEPGetConvergedReason(), PEPSetTolerances()
@*/
PetscErrorCode PEPGetIterationNumber(PEP pep,PetscInt *its)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  PetscValidIntPointer(its,2);
  *its = pep->its;
  PetscFunctionReturn(0);
}

/*@
   PEPGetConverged - Gets the number of converged eigenpairs.

   Not Collective

   Input Parameter:
.  pep - the polynomial eigensolver context

   Output Parameter:
.  nconv - number of converged eigenpairs

   Note:
   This function should be called after PEPSolve() has finished.

   Level: beginner

.seealso: PEPSetDimensions(), PEPSolve(), PEPGetEigenpair()
@*/
PetscErrorCode PEPGetConverged(PEP pep,PetscInt *nconv)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  PetscValidIntPointer(nconv,2);
  PEPCheckSolved(pep,1);
  *nconv = pep->nconv;
  PetscFunctionReturn(0);
}

/*@
   PEPGetConvergedReason - Gets the reason why the PEPSolve() iteration was
   stopped.

   Not Collective

   Input Parameter:
.  pep - the polynomial eigensolver context

   Output Parameter:
.  reason - negative value indicates diverged, positive value converged

   Notes:

   Possible values for reason are
+  PEP_CONVERGED_TOL - converged up to tolerance
.  PEP_CONVERGED_USER - converged due to a user-defined condition
.  PEP_DIVERGED_ITS - required more than max_it iterations to reach convergence
.  PEP_DIVERGED_BREAKDOWN - generic breakdown in method
-  PEP_DIVERGED_SYMMETRY_LOST - pseudo-Lanczos was not able to keep symmetry

   Can only be called after the call to PEPSolve() is complete.

   Level: intermediate

.seealso: PEPSetTolerances(), PEPSolve(), PEPConvergedReason
@*/
PetscErrorCode PEPGetConvergedReason(PEP pep,PEPConvergedReason *reason)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  PetscValidPointer(reason,2);
  PEPCheckSolved(pep,1);
  *reason = pep->reason;
  PetscFunctionReturn(0);
}

/*@C
   PEPGetEigenpair - Gets the i-th solution of the eigenproblem as computed by
   PEPSolve(). The solution consists in both the eigenvalue and the eigenvector.

   Logically Collective on pep

   Input Parameters:
+  pep - polynomial eigensolver context
-  i   - index of the solution

   Output Parameters:
+  eigr - real part of eigenvalue
.  eigi - imaginary part of eigenvalue
.  Vr   - real part of eigenvector
-  Vi   - imaginary part of eigenvector

   Notes:
   It is allowed to pass NULL for Vr and Vi, if the eigenvector is not
   required. Otherwise, the caller must provide valid Vec objects, i.e.,
   they must be created by the calling program with e.g. MatCreateVecs().

   If the eigenvalue is real, then eigi and Vi are set to zero. If PETSc is
   configured with complex scalars the eigenvalue is stored
   directly in eigr (eigi is set to zero) and the eigenvector in Vr (Vi is
   set to zero). In any case, the user can pass NULL in Vr or Vi if one of
   them is not required.

   The index i should be a value between 0 and nconv-1 (see PEPGetConverged()).
   Eigenpairs are indexed according to the ordering criterion established
   with PEPSetWhichEigenpairs().

   Level: beginner

.seealso: PEPSolve(), PEPGetConverged(), PEPSetWhichEigenpairs()
@*/
PetscErrorCode PEPGetEigenpair(PEP pep,PetscInt i,PetscScalar *eigr,PetscScalar *eigi,Vec Vr,Vec Vi)
{
  PetscInt       k;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  PetscValidLogicalCollectiveInt(pep,i,2);
  if (Vr) { PetscValidHeaderSpecific(Vr,VEC_CLASSID,5); PetscCheckSameComm(pep,1,Vr,5); }
  if (Vi) { PetscValidHeaderSpecific(Vi,VEC_CLASSID,6); PetscCheckSameComm(pep,1,Vi,6); }
  PEPCheckSolved(pep,1);
  PetscCheck(i>=0,PetscObjectComm((PetscObject)pep),PETSC_ERR_ARG_OUTOFRANGE,"The index cannot be negative");
  PetscCheck(i<pep->nconv,PetscObjectComm((PetscObject)pep),PETSC_ERR_ARG_OUTOFRANGE,"The index can be nconv-1 at most, see PEPGetConverged()");

  CHKERRQ(PEPComputeVectors(pep));
  k = pep->perm[i];

  /* eigenvalue */
#if defined(PETSC_USE_COMPLEX)
  if (eigr) *eigr = pep->eigr[k];
  if (eigi) *eigi = 0;
#else
  if (eigr) *eigr = pep->eigr[k];
  if (eigi) *eigi = pep->eigi[k];
#endif

  /* eigenvector */
  CHKERRQ(BV_GetEigenvector(pep->V,k,pep->eigi[k],Vr,Vi));
  PetscFunctionReturn(0);
}

/*@
   PEPGetErrorEstimate - Returns the error estimate associated to the i-th
   computed eigenpair.

   Not Collective

   Input Parameters:
+  pep - polynomial eigensolver context
-  i   - index of eigenpair

   Output Parameter:
.  errest - the error estimate

   Notes:
   This is the error estimate used internally by the eigensolver. The actual
   error bound can be computed with PEPComputeError(). See also the users
   manual for details.

   Level: advanced

.seealso: PEPComputeError()
@*/
PetscErrorCode PEPGetErrorEstimate(PEP pep,PetscInt i,PetscReal *errest)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  PetscValidRealPointer(errest,3);
  PEPCheckSolved(pep,1);
  PetscCheck(i>=0,PetscObjectComm((PetscObject)pep),PETSC_ERR_ARG_OUTOFRANGE,"The index cannot be negative");
  PetscCheck(i<pep->nconv,PetscObjectComm((PetscObject)pep),PETSC_ERR_ARG_OUTOFRANGE,"The index can be nconv-1 at most, see PEPGetConverged()");
  *errest = pep->errest[pep->perm[i]];
  PetscFunctionReturn(0);
}

/*
   PEPComputeResidualNorm_Private - Computes the norm of the residual vector
   associated with an eigenpair.

   Input Parameters:
     kr,ki - eigenvalue
     xr,xi - eigenvector
     z     - array of 4 work vectors (z[2],z[3] not referenced in complex scalars)
*/
PetscErrorCode PEPComputeResidualNorm_Private(PEP pep,PetscScalar kr,PetscScalar ki,Vec xr,Vec xi,Vec *z,PetscReal *norm)
{
  Mat            *A=pep->A;
  PetscInt       i,nmat=pep->nmat;
  PetscScalar    t[20],*vals=t,*ivals=NULL;
  Vec            u,w;
#if !defined(PETSC_USE_COMPLEX)
  Vec            ui,wi;
  PetscReal      ni;
  PetscBool      imag;
  PetscScalar    it[20];
#endif

  PetscFunctionBegin;
  u = z[0]; w = z[1];
  CHKERRQ(VecSet(u,0.0));
#if !defined(PETSC_USE_COMPLEX)
  ui = z[2]; wi = z[3];
  ivals = it;
#endif
  if (nmat>20) {
    CHKERRQ(PetscMalloc1(nmat,&vals));
#if !defined(PETSC_USE_COMPLEX)
    CHKERRQ(PetscMalloc1(nmat,&ivals));
#endif
  }
  CHKERRQ(PEPEvaluateBasis(pep,kr,ki,vals,ivals));
#if !defined(PETSC_USE_COMPLEX)
  if (ki == 0 || PetscAbsScalar(ki) < PetscAbsScalar(kr*PETSC_MACHINE_EPSILON))
    imag = PETSC_FALSE;
  else {
    imag = PETSC_TRUE;
    CHKERRQ(VecSet(ui,0.0));
  }
#endif
  for (i=0;i<nmat;i++) {
    if (vals[i]!=0.0) {
      CHKERRQ(MatMult(A[i],xr,w));
      CHKERRQ(VecAXPY(u,vals[i],w));
    }
#if !defined(PETSC_USE_COMPLEX)
    if (imag) {
      if (ivals[i]!=0 || vals[i]!=0) {
        CHKERRQ(MatMult(A[i],xi,wi));
        if (vals[i]==0) {
          CHKERRQ(MatMult(A[i],xr,w));
        }
      }
      if (ivals[i]!=0) {
        CHKERRQ(VecAXPY(u,-ivals[i],wi));
        CHKERRQ(VecAXPY(ui,ivals[i],w));
      }
      if (vals[i]!=0) {
        CHKERRQ(VecAXPY(ui,vals[i],wi));
      }
    }
#endif
  }
  CHKERRQ(VecNorm(u,NORM_2,norm));
#if !defined(PETSC_USE_COMPLEX)
  if (imag) {
    CHKERRQ(VecNorm(ui,NORM_2,&ni));
    *norm = SlepcAbsEigenvalue(*norm,ni);
  }
#endif
  if (nmat>20) {
    CHKERRQ(PetscFree(vals));
#if !defined(PETSC_USE_COMPLEX)
    CHKERRQ(PetscFree(ivals));
#endif
  }
  PetscFunctionReturn(0);
}

/*@
   PEPComputeError - Computes the error (based on the residual norm) associated
   with the i-th computed eigenpair.

   Collective on pep

   Input Parameters:
+  pep  - the polynomial eigensolver context
.  i    - the solution index
-  type - the type of error to compute

   Output Parameter:
.  error - the error

   Notes:
   The error can be computed in various ways, all of them based on the residual
   norm ||P(l)x||_2 where l is the eigenvalue and x is the eigenvector.
   See the users guide for additional details.

   Level: beginner

.seealso: PEPErrorType, PEPSolve(), PEPGetErrorEstimate()
@*/
PetscErrorCode PEPComputeError(PEP pep,PetscInt i,PEPErrorType type,PetscReal *error)
{
  Vec            xr,xi,w[4];
  PetscScalar    kr,ki;
  PetscReal      t,z=0.0;
  PetscInt       j;
  PetscBool      flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pep,PEP_CLASSID,1);
  PetscValidLogicalCollectiveInt(pep,i,2);
  PetscValidLogicalCollectiveEnum(pep,type,3);
  PetscValidRealPointer(error,4);
  PEPCheckSolved(pep,1);

  /* allocate work vectors */
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(PEPSetWorkVecs(pep,3));
  xi   = NULL;
  w[2] = NULL;
  w[3] = NULL;
#else
  CHKERRQ(PEPSetWorkVecs(pep,6));
  xi   = pep->work[3];
  w[2] = pep->work[4];
  w[3] = pep->work[5];
#endif
  xr   = pep->work[0];
  w[0] = pep->work[1];
  w[1] = pep->work[2];

  /* compute residual norms */
  CHKERRQ(PEPGetEigenpair(pep,i,&kr,&ki,xr,xi));
  CHKERRQ(PEPComputeResidualNorm_Private(pep,kr,ki,xr,xi,w,error));

  /* compute error */
  switch (type) {
    case PEP_ERROR_ABSOLUTE:
      break;
    case PEP_ERROR_RELATIVE:
      *error /= SlepcAbsEigenvalue(kr,ki);
      break;
    case PEP_ERROR_BACKWARD:
      /* initialization of matrix norms */
      if (!pep->nrma[pep->nmat-1]) {
        for (j=0;j<pep->nmat;j++) {
          CHKERRQ(MatHasOperation(pep->A[j],MATOP_NORM,&flg));
          PetscCheck(flg,PetscObjectComm((PetscObject)pep),PETSC_ERR_ARG_WRONG,"The computation of backward errors requires a matrix norm operation");
          CHKERRQ(MatNorm(pep->A[j],NORM_INFINITY,&pep->nrma[j]));
        }
      }
      t = SlepcAbsEigenvalue(kr,ki);
      for (j=pep->nmat-1;j>=0;j--) {
        z = z*t+pep->nrma[j];
      }
      *error /= z;
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)pep),PETSC_ERR_ARG_OUTOFRANGE,"Invalid error type");
  }
  PetscFunctionReturn(0);
}
