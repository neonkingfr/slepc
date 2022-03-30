/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   SVD routines related to the solution process
*/

#include <slepc/private/svdimpl.h>   /*I "slepcsvd.h" I*/

/*
  SVDComputeVectors_Left - Compute left singular vectors as U=A*V.
  Only done if the leftbasis flag is false. Assumes V is available.
 */
PetscErrorCode SVDComputeVectors_Left(SVD svd)
{
  Vec            tl;
  PetscInt       oldsize;

  PetscFunctionBegin;
  if (!svd->leftbasis) {
    /* generate left singular vectors on U */
    if (!svd->U) PetscCall(SVDGetBV(svd,NULL,&svd->U));
    PetscCall(BVGetSizes(svd->U,NULL,NULL,&oldsize));
    if (!oldsize) {
      if (!((PetscObject)(svd->U))->type_name) PetscCall(BVSetType(svd->U,((PetscObject)(svd->V))->type_name));
      PetscCall(MatCreateVecsEmpty(svd->A,NULL,&tl));
      PetscCall(BVSetSizesFromVec(svd->U,tl,svd->ncv));
      PetscCall(VecDestroy(&tl));
    }
    PetscCall(BVSetActiveColumns(svd->V,0,svd->nconv));
    PetscCall(BVSetActiveColumns(svd->U,0,svd->nconv));
    PetscCall(BVMatMult(svd->V,svd->A,svd->U));
    PetscCall(BVOrthogonalize(svd->U,NULL));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode SVDComputeVectors(SVD svd)
{
  PetscFunctionBegin;
  SVDCheckSolved(svd,1);
  if (svd->state==SVD_STATE_SOLVED && svd->ops->computevectors) PetscCall((*svd->ops->computevectors)(svd));
  svd->state = SVD_STATE_VECTORS;
  PetscFunctionReturn(0);
}

/*@
   SVDSolve - Solves the singular value problem.

   Collective on svd

   Input Parameter:
.  svd - singular value solver context obtained from SVDCreate()

   Options Database Keys:
+  -svd_view - print information about the solver used
.  -svd_view_mat0 - view the first matrix (A)
.  -svd_view_mat1 - view the second matrix (B)
.  -svd_view_vectors - view the computed singular vectors
.  -svd_view_values - view the computed singular values
.  -svd_converged_reason - print reason for convergence, and number of iterations
.  -svd_error_absolute - print absolute errors of each singular triplet
.  -svd_error_relative - print relative errors of each singular triplet
-  -svd_error_norm     - print errors relative to the matrix norms of each singular triplet

   Notes:
   All the command-line options listed above admit an optional argument specifying
   the viewer type and options. For instance, use '-svd_view_mat0 binary:amatrix.bin'
   to save the A matrix to a binary file, '-svd_view_values draw' to draw the computed
   singular values graphically, or '-svd_error_relative :myerr.m:ascii_matlab' to save
   the errors in a file that can be executed in Matlab.

   Level: beginner

.seealso: SVDCreate(), SVDSetUp(), SVDDestroy()
@*/
PetscErrorCode SVDSolve(SVD svd)
{
  PetscInt       i,*workperm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (svd->state>=SVD_STATE_SOLVED) PetscFunctionReturn(0);
  PetscCall(PetscLogEventBegin(SVD_Solve,svd,0,0,0));

  /* call setup */
  PetscCall(SVDSetUp(svd));
  svd->its = 0;
  svd->nconv = 0;
  for (i=0;i<svd->ncv;i++) {
    svd->sigma[i]  = 0.0;
    svd->errest[i] = 0.0;
    svd->perm[i]   = i;
  }
  PetscCall(SVDViewFromOptions(svd,NULL,"-svd_view_pre"));

  switch (svd->problem_type) {
    case SVD_STANDARD:
      PetscCall((*svd->ops->solve)(svd));
      break;
    case SVD_GENERALIZED:
      PetscCall((*svd->ops->solveg)(svd));
      break;
  }
  svd->state = SVD_STATE_SOLVED;

  /* sort singular triplets */
  if (svd->which == SVD_SMALLEST) PetscCall(PetscSortRealWithPermutation(svd->nconv,svd->sigma,svd->perm));
  else {
    PetscCall(PetscMalloc1(svd->nconv,&workperm));
    for (i=0;i<svd->nconv;i++) workperm[i] = i;
    PetscCall(PetscSortRealWithPermutation(svd->nconv,svd->sigma,workperm));
    for (i=0;i<svd->nconv;i++) svd->perm[i] = workperm[svd->nconv-i-1];
    PetscCall(PetscFree(workperm));
  }
  PetscCall(PetscLogEventEnd(SVD_Solve,svd,0,0,0));

  /* various viewers */
  PetscCall(SVDViewFromOptions(svd,NULL,"-svd_view"));
  PetscCall(SVDConvergedReasonViewFromOptions(svd));
  PetscCall(SVDErrorViewFromOptions(svd));
  PetscCall(SVDValuesViewFromOptions(svd));
  PetscCall(SVDVectorsViewFromOptions(svd));
  PetscCall(MatViewFromOptions(svd->OP,(PetscObject)svd,"-svd_view_mat0"));
  if (svd->isgeneralized) PetscCall(MatViewFromOptions(svd->OPb,(PetscObject)svd,"-svd_view_mat1"));

  /* Remove the initial subspaces */
  svd->nini = 0;
  svd->ninil = 0;
  PetscFunctionReturn(0);
}

/*@
   SVDGetIterationNumber - Gets the current iteration number. If the
   call to SVDSolve() is complete, then it returns the number of iterations
   carried out by the solution method.

   Not Collective

   Input Parameter:
.  svd - the singular value solver context

   Output Parameter:
.  its - number of iterations

   Note:
   During the i-th iteration this call returns i-1. If SVDSolve() is
   complete, then parameter "its" contains either the iteration number at
   which convergence was successfully reached, or failure was detected.
   Call SVDGetConvergedReason() to determine if the solver converged or
   failed and why.

   Level: intermediate

.seealso: SVDGetConvergedReason(), SVDSetTolerances()
@*/
PetscErrorCode SVDGetIterationNumber(SVD svd,PetscInt *its)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidIntPointer(its,2);
  *its = svd->its;
  PetscFunctionReturn(0);
}

/*@
   SVDGetConvergedReason - Gets the reason why the SVDSolve() iteration was
   stopped.

   Not Collective

   Input Parameter:
.  svd - the singular value solver context

   Output Parameter:
.  reason - negative value indicates diverged, positive value converged
   (see SVDConvergedReason)

   Options Database Key:
.  -svd_converged_reason - print the reason to a viewer

   Notes:
   Possible values for reason are
+  SVD_CONVERGED_TOL - converged up to tolerance
.  SVD_CONVERGED_USER - converged due to a user-defined condition
.  SVD_CONVERGED_MAXIT - reached the maximum number of iterations with SVD_CONV_MAXIT criterion
.  SVD_DIVERGED_ITS - required more than max_it iterations to reach convergence
-  SVD_DIVERGED_BREAKDOWN - generic breakdown in method

   Can only be called after the call to SVDSolve() is complete.

   Level: intermediate

.seealso: SVDSetTolerances(), SVDSolve(), SVDConvergedReason
@*/
PetscErrorCode SVDGetConvergedReason(SVD svd,SVDConvergedReason *reason)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidIntPointer(reason,2);
  SVDCheckSolved(svd,1);
  *reason = svd->reason;
  PetscFunctionReturn(0);
}

/*@
   SVDGetConverged - Gets the number of converged singular values.

   Not Collective

   Input Parameter:
.  svd - the singular value solver context

   Output Parameter:
.  nconv - number of converged singular values

   Note:
   This function should be called after SVDSolve() has finished.

   Level: beginner

.seealso: SVDSetDimensions(), SVDSolve(), SVDGetSingularTriplet()
@*/
PetscErrorCode SVDGetConverged(SVD svd,PetscInt *nconv)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidIntPointer(nconv,2);
  SVDCheckSolved(svd,1);
  *nconv = svd->nconv;
  PetscFunctionReturn(0);
}

/*@C
   SVDGetSingularTriplet - Gets the i-th triplet of the singular value decomposition
   as computed by SVDSolve(). The solution consists in the singular value and its left
   and right singular vectors.

   Not Collective, but vectors are shared by all processors that share the SVD

   Input Parameters:
+  svd - singular value solver context
-  i   - index of the solution

   Output Parameters:
+  sigma - singular value
.  u     - left singular vector
-  v     - right singular vector

   Note:
   Both u or v can be NULL if singular vectors are not required.
   Otherwise, the caller must provide valid Vec objects, i.e.,
   they must be created by the calling program with e.g. MatCreateVecs().

   The index i should be a value between 0 and nconv-1 (see SVDGetConverged()).
   Singular triplets are indexed according to the ordering criterion established
   with SVDSetWhichSingularTriplets().

   In the case of GSVD, the solution consists in three vectors u,v,x that are
   returned as follows. Vector x is returned in the right singular vector
   (argument v) and has length equal to the number of columns of A and B.
   The other two vectors are returned stacked on top of each other [u;v] in
   the left singular vector argument, with length equal to m+n (number of rows
   of A plus number of rows of B).

   Level: beginner

.seealso: SVDSolve(), SVDGetConverged(), SVDSetWhichSingularTriplets()
@*/
PetscErrorCode SVDGetSingularTriplet(SVD svd,PetscInt i,PetscReal *sigma,Vec u,Vec v)
{
  PetscInt       M,N;
  Vec            w;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveInt(svd,i,2);
  SVDCheckSolved(svd,1);
  if (u) { PetscValidHeaderSpecific(u,VEC_CLASSID,4); PetscCheckSameComm(svd,1,u,4); }
  if (v) { PetscValidHeaderSpecific(v,VEC_CLASSID,5); PetscCheckSameComm(svd,1,v,5); }
  PetscCheck(i>=0,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"The index cannot be negative");
  PetscCheck(i<svd->nconv,PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"The index can be nconv-1 at most, see SVDGetConverged()");
  if (sigma) *sigma = svd->sigma[svd->perm[i]];
  if (u || v) {
    if (!svd->isgeneralized) {
      PetscCall(MatGetSize(svd->OP,&M,&N));
      if (M<N) { w = u; u = v; v = w; }
    }
    PetscCall(SVDComputeVectors(svd));
    if (u) PetscCall(BVCopyVec(svd->U,svd->perm[i],u));
    if (v) PetscCall(BVCopyVec(svd->V,svd->perm[i],v));
  }
  PetscFunctionReturn(0);
}

/*
   SVDComputeResidualNorms_Standard - Computes the norms of the left and
   right residuals associated with the i-th computed singular triplet.

   Input Parameters:
     sigma - singular value
     u,v   - singular vectors
     x,y   - two work vectors with the same dimensions as u,v
*/
static PetscErrorCode SVDComputeResidualNorms_Standard(SVD svd,PetscReal sigma,Vec u,Vec v,Vec x,Vec y,PetscReal *norm1,PetscReal *norm2)
{
  PetscInt       M,N;

  PetscFunctionBegin;
  /* norm1 = ||A*v-sigma*u||_2 */
  if (norm1) {
    PetscCall(MatMult(svd->OP,v,x));
    PetscCall(VecAXPY(x,-sigma,u));
    PetscCall(VecNorm(x,NORM_2,norm1));
  }
  /* norm2 = ||A^T*u-sigma*v||_2 */
  if (norm2) {
    PetscCall(MatGetSize(svd->OP,&M,&N));
    if (M<N) PetscCall(MatMult(svd->A,u,y));
    else PetscCall(MatMult(svd->AT,u,y));
    PetscCall(VecAXPY(y,-sigma,v));
    PetscCall(VecNorm(y,NORM_2,norm2));
  }
  PetscFunctionReturn(0);
}

/*
   SVDComputeResidualNorms_Generalized - In GSVD, compute the residual norms
   norm1 = ||s^2*A'*u-c*B'*B*x||_2 and norm2 = ||c^2*B'*v-s*A'*A*x||_2.

   Input Parameters:
     sigma - singular value
     uv    - left singular vectors [u;v]
     x     - right singular vector
     y,z   - two work vectors with the same dimension as x
*/
static PetscErrorCode SVDComputeResidualNorms_Generalized(SVD svd,PetscReal sigma,Vec uv,Vec x,Vec y,Vec z,PetscReal *norm1,PetscReal *norm2)
{
  Vec            u,v,ax,bx,nest,aux[2];
  PetscReal      c,s;

  PetscFunctionBegin;
  PetscCall(MatCreateVecs(svd->OP,NULL,&u));
  PetscCall(MatCreateVecs(svd->OPb,NULL,&v));
  aux[0] = u;
  aux[1] = v;
  PetscCall(VecCreateNest(PetscObjectComm((PetscObject)svd),2,NULL,aux,&nest));
  PetscCall(VecCopy(uv,nest));

  s = 1.0/PetscSqrtReal(1.0+sigma*sigma);
  c = sigma*s;

  /* norm1 = ||s^2*A'*u-c*B'*B*x||_2 */
  if (norm1) {
    PetscCall(VecDuplicate(v,&bx));
    PetscCall(MatMultHermitianTranspose(svd->OP,u,z));
    PetscCall(MatMult(svd->OPb,x,bx));
    PetscCall(MatMultHermitianTranspose(svd->OPb,bx,y));
    PetscCall(VecAXPBY(y,s*s,-c,z));
    PetscCall(VecNorm(y,NORM_2,norm1));
    PetscCall(VecDestroy(&bx));
  }
  /* norm2 = ||c^2*B'*v-s*A'*A*x||_2 */
  if (norm2) {
    PetscCall(VecDuplicate(u,&ax));
    PetscCall(MatMultHermitianTranspose(svd->OPb,v,z));
    PetscCall(MatMult(svd->OP,x,ax));
    PetscCall(MatMultHermitianTranspose(svd->OP,ax,y));
    PetscCall(VecAXPBY(y,c*c,-s,z));
    PetscCall(VecNorm(y,NORM_2,norm2));
    PetscCall(VecDestroy(&ax));
  }

  PetscCall(VecDestroy(&v));
  PetscCall(VecDestroy(&u));
  PetscCall(VecDestroy(&nest));
  PetscFunctionReturn(0);
}

/*@
   SVDComputeError - Computes the error (based on the residual norm) associated
   with the i-th singular triplet.

   Collective on svd

   Input Parameters:
+  svd  - the singular value solver context
.  i    - the solution index
-  type - the type of error to compute

   Output Parameter:
.  error - the error

   Notes:
   The error can be computed in various ways, all of them based on the residual
   norm obtained as sqrt(n1^2+n2^2) with n1 = ||A*v-sigma*u||_2 and
   n2 = ||A^T*u-sigma*v||_2, where sigma is the singular value, u is the left
   singular vector and v is the right singular vector.

   In the case of the GSVD, the two components of the residual norm are
   n1 = ||s^2*A'*u-c*B'*B*x||_2 and n2 = ||c^2*B'*v-s*A'*A*x||_2, where [u;v]
   are the left singular vectors and x is the right singular vector, with
   sigma=c/s.

   Level: beginner

.seealso: SVDErrorType, SVDSolve()
@*/
PetscErrorCode SVDComputeError(SVD svd,PetscInt i,SVDErrorType type,PetscReal *error)
{
  PetscReal      sigma,norm1,norm2;
  Vec            u=NULL,v=NULL,x=NULL,y=NULL;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveInt(svd,i,2);
  PetscValidLogicalCollectiveEnum(svd,type,3);
  PetscValidRealPointer(error,4);
  SVDCheckSolved(svd,1);

  /* allocate work vectors */
  switch (svd->problem_type) {
    case SVD_STANDARD:
      PetscCall(SVDSetWorkVecs(svd,2,2));
      u = svd->workl[0];
      v = svd->workr[0];
      x = svd->workl[1];
      y = svd->workr[1];
      break;
    case SVD_GENERALIZED:
      PetscCheck(type!=SVD_ERROR_RELATIVE,PetscObjectComm((PetscObject)svd),PETSC_ERR_SUP,"In GSVD the error should be either absolute or relative to the norms");
      PetscCall(SVDSetWorkVecs(svd,1,3));
      u = svd->workl[0];
      v = svd->workr[0];
      x = svd->workr[1];
      y = svd->workr[2];
      break;
  }

  /* compute residual norm and error */
  PetscCall(SVDGetSingularTriplet(svd,i,&sigma,u,v));
  switch (svd->problem_type) {
    case SVD_STANDARD:
      PetscCall(SVDComputeResidualNorms_Standard(svd,sigma,u,v,x,y,&norm1,&norm2));
      break;
    case SVD_GENERALIZED:
      PetscCall(SVDComputeResidualNorms_Generalized(svd,sigma,u,v,x,y,&norm1,&norm2));
      break;
  }
  *error = PetscSqrtReal(norm1*norm1+norm2*norm2);
  switch (type) {
    case SVD_ERROR_ABSOLUTE:
      break;
    case SVD_ERROR_RELATIVE:
      *error /= sigma;
      break;
    case SVD_ERROR_NORM:
      if (!svd->nrma) PetscCall(MatNorm(svd->OP,NORM_INFINITY,&svd->nrma));
      if (svd->isgeneralized && !svd->nrmb) PetscCall(MatNorm(svd->OPb,NORM_INFINITY,&svd->nrmb));
      *error /= PetscMax(svd->nrma,svd->nrmb);
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Invalid error type");
  }
  PetscFunctionReturn(0);
}
