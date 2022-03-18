/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Inverse square root function  x^(-1/2)
*/

#include <slepc/private/fnimpl.h>      /*I "slepcfn.h" I*/
#include <slepcblaslapack.h>

PetscErrorCode FNEvaluateFunction_Invsqrt(FN fn,PetscScalar x,PetscScalar *y)
{
  PetscFunctionBegin;
  PetscCheck(x!=0.0,PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Function not defined in the requested value");
#if !defined(PETSC_USE_COMPLEX)
  PetscCheck(x>0.0,PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Function not defined in the requested value");
#endif
  *y = 1.0/PetscSqrtScalar(x);
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateDerivative_Invsqrt(FN fn,PetscScalar x,PetscScalar *y)
{
  PetscFunctionBegin;
  PetscCheck(x!=0.0,PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Derivative not defined in the requested value");
#if !defined(PETSC_USE_COMPLEX)
  PetscCheck(x>0.0,PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Derivative not defined in the requested value");
#endif
  *y = -1.0/(2.0*PetscPowScalarReal(x,1.5));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMat_Invsqrt_Schur(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0,ld,*ipiv,info;
  PetscScalar    *Ba,*Wa;
  PetscInt       m;
  Mat            W;

  PetscFunctionBegin;
  CHKERRQ(FN_AllocateWorkMat(fn,A,&W));
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&Ba));
  CHKERRQ(MatDenseGetArray(W,&Wa));
  /* compute B = sqrtm(A) */
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  ld = n;
  CHKERRQ(FNSqrtmSchur(fn,n,Ba,n,PETSC_FALSE));
  /* compute B = A\B */
  CHKERRQ(PetscMalloc1(ld,&ipiv));
  PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n,&n,Wa,&ld,ipiv,Ba,&ld,&info));
  SlepcCheckLapackInfo("gesv",info);
  CHKERRQ(PetscLogFlops(2.0*n*n*n/3.0+2.0*n*n*n));
  CHKERRQ(PetscFree(ipiv));
  CHKERRQ(MatDenseRestoreArray(W,&Wa));
  CHKERRQ(MatDenseRestoreArray(B,&Ba));
  CHKERRQ(FN_FreeWorkMat(fn,&W));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMatVec_Invsqrt_Schur(FN fn,Mat A,Vec v)
{
  PetscBLASInt   n=0,ld,*ipiv,info,one=1;
  PetscScalar    *Ba,*Wa;
  PetscInt       m;
  Mat            B,W;

  PetscFunctionBegin;
  CHKERRQ(FN_AllocateWorkMat(fn,A,&B));
  CHKERRQ(FN_AllocateWorkMat(fn,A,&W));
  CHKERRQ(MatDenseGetArray(B,&Ba));
  CHKERRQ(MatDenseGetArray(W,&Wa));
  /* compute B_1 = sqrtm(A)*e_1 */
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  ld = n;
  CHKERRQ(FNSqrtmSchur(fn,n,Ba,n,PETSC_TRUE));
  /* compute B_1 = A\B_1 */
  CHKERRQ(PetscMalloc1(ld,&ipiv));
  PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n,&one,Wa,&ld,ipiv,Ba,&ld,&info));
  SlepcCheckLapackInfo("gesv",info);
  CHKERRQ(PetscFree(ipiv));
  CHKERRQ(MatDenseRestoreArray(W,&Wa));
  CHKERRQ(MatDenseRestoreArray(B,&Ba));
  CHKERRQ(MatGetColumnVector(B,v,0));
  CHKERRQ(FN_FreeWorkMat(fn,&W));
  CHKERRQ(FN_FreeWorkMat(fn,&B));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMat_Invsqrt_DBP(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0;
  PetscScalar    *T;
  PetscInt       m;

  PetscFunctionBegin;
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&T));
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  CHKERRQ(FNSqrtmDenmanBeavers(fn,n,T,n,PETSC_TRUE));
  CHKERRQ(MatDenseRestoreArray(B,&T));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMat_Invsqrt_NS(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0;
  PetscScalar    *T;
  PetscInt       m;

  PetscFunctionBegin;
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&T));
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  CHKERRQ(FNSqrtmNewtonSchulz(fn,n,T,n,PETSC_TRUE));
  CHKERRQ(MatDenseRestoreArray(B,&T));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMat_Invsqrt_Sadeghi(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0,ld,*ipiv,info;
  PetscScalar    *Ba,*Wa;
  PetscInt       m;
  Mat            W;

  PetscFunctionBegin;
  CHKERRQ(FN_AllocateWorkMat(fn,A,&W));
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&Ba));
  CHKERRQ(MatDenseGetArray(W,&Wa));
  /* compute B = sqrtm(A) */
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  ld = n;
  CHKERRQ(FNSqrtmSadeghi(fn,n,Ba,n));
  /* compute B = A\B */
  CHKERRQ(PetscMalloc1(ld,&ipiv));
  PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n,&n,Wa,&ld,ipiv,Ba,&ld,&info));
  SlepcCheckLapackInfo("gesv",info);
  CHKERRQ(PetscLogFlops(2.0*n*n*n/3.0+2.0*n*n*n));
  CHKERRQ(PetscFree(ipiv));
  CHKERRQ(MatDenseRestoreArray(W,&Wa));
  CHKERRQ(MatDenseRestoreArray(B,&Ba));
  CHKERRQ(FN_FreeWorkMat(fn,&W));
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_CUDA)
PetscErrorCode FNEvaluateFunctionMat_Invsqrt_NS_CUDA(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0;
  PetscScalar    *Ba;
  PetscInt       m;

  PetscFunctionBegin;
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&Ba));
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  CHKERRQ(FNSqrtmNewtonSchulz_CUDA(fn,n,Ba,n,PETSC_TRUE));
  CHKERRQ(MatDenseRestoreArray(B,&Ba));
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_MAGMA)
PetscErrorCode FNEvaluateFunctionMat_Invsqrt_DBP_CUDAm(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0;
  PetscScalar    *T;
  PetscInt       m;

  PetscFunctionBegin;
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&T));
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  CHKERRQ(FNSqrtmDenmanBeavers_CUDAm(fn,n,T,n,PETSC_TRUE));
  CHKERRQ(MatDenseRestoreArray(B,&T));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMat_Invsqrt_Sadeghi_CUDAm(FN fn,Mat A,Mat B)
{
  PetscBLASInt   n=0,ld,*ipiv,info;
  PetscScalar    *Ba,*Wa;
  PetscInt       m;
  Mat            W;

  PetscFunctionBegin;
  CHKERRQ(FN_AllocateWorkMat(fn,A,&W));
  if (A!=B) CHKERRQ(MatCopy(A,B,SAME_NONZERO_PATTERN));
  CHKERRQ(MatDenseGetArray(B,&Ba));
  CHKERRQ(MatDenseGetArray(W,&Wa));
  /* compute B = sqrtm(A) */
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(PetscBLASIntCast(m,&n));
  ld = n;
  CHKERRQ(FNSqrtmSadeghi_CUDAm(fn,n,Ba,n));
  /* compute B = A\B */
  CHKERRQ(PetscMalloc1(ld,&ipiv));
  PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n,&n,Wa,&ld,ipiv,Ba,&ld,&info));
  SlepcCheckLapackInfo("gesv",info);
  CHKERRQ(PetscLogFlops(2.0*n*n*n/3.0+2.0*n*n*n));
  CHKERRQ(PetscFree(ipiv));
  CHKERRQ(MatDenseRestoreArray(W,&Wa));
  CHKERRQ(MatDenseRestoreArray(B,&Ba));
  CHKERRQ(FN_FreeWorkMat(fn,&W));
  PetscFunctionReturn(0);
}
#endif /* PETSC_HAVE_MAGMA */
#endif /* PETSC_HAVE_CUDA */

PetscErrorCode FNView_Invsqrt(FN fn,PetscViewer viewer)
{
  PetscBool      isascii;
  char           str[50];
  const char     *methodname[] = {
                  "Schur method for inv(A)*sqrtm(A)",
                  "Denman-Beavers (product form)",
                  "Newton-Schulz iteration",
                  "Sadeghi iteration"
#if defined(PETSC_HAVE_CUDA)
                 ,"Newton-Schulz iteration CUDA"
#if defined(PETSC_HAVE_MAGMA)
                 ,"Denman-Beavers (product form) CUDA/MAGMA",
                  "Sadeghi iteration CUDA/MAGMA"
#endif
#endif
  };
  const int      nmeth=sizeof(methodname)/sizeof(methodname[0]);

  PetscFunctionBegin;
  CHKERRQ(PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii));
  if (isascii) {
    if (fn->beta==(PetscScalar)1.0) {
      if (fn->alpha==(PetscScalar)1.0) {
        CHKERRQ(PetscViewerASCIIPrintf(viewer,"  Inverse square root: x^(-1/2)\n"));
      } else {
        CHKERRQ(SlepcSNPrintfScalar(str,sizeof(str),fn->alpha,PETSC_TRUE));
        CHKERRQ(PetscViewerASCIIPrintf(viewer,"  Inverse square root: (%s*x)^(-1/2)\n",str));
      }
    } else {
      CHKERRQ(SlepcSNPrintfScalar(str,sizeof(str),fn->beta,PETSC_TRUE));
      if (fn->alpha==(PetscScalar)1.0) {
        CHKERRQ(PetscViewerASCIIPrintf(viewer,"  Inverse square root: %s*x^(-1/2)\n",str));
      } else {
        CHKERRQ(PetscViewerASCIIPrintf(viewer,"  Inverse square root: %s",str));
        CHKERRQ(PetscViewerASCIIUseTabs(viewer,PETSC_FALSE));
        CHKERRQ(SlepcSNPrintfScalar(str,sizeof(str),fn->alpha,PETSC_TRUE));
        CHKERRQ(PetscViewerASCIIPrintf(viewer,"*(%s*x)^(-1/2)\n",str));
        CHKERRQ(PetscViewerASCIIUseTabs(viewer,PETSC_TRUE));
      }
    }
    if (fn->method<nmeth) {
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"  computing matrix functions with: %s\n",methodname[fn->method]));
    }
  }
  PetscFunctionReturn(0);
}

SLEPC_EXTERN PetscErrorCode FNCreate_Invsqrt(FN fn)
{
  PetscFunctionBegin;
  fn->ops->evaluatefunction          = FNEvaluateFunction_Invsqrt;
  fn->ops->evaluatederivative        = FNEvaluateDerivative_Invsqrt;
  fn->ops->evaluatefunctionmat[0]    = FNEvaluateFunctionMat_Invsqrt_Schur;
  fn->ops->evaluatefunctionmat[1]    = FNEvaluateFunctionMat_Invsqrt_DBP;
  fn->ops->evaluatefunctionmat[2]    = FNEvaluateFunctionMat_Invsqrt_NS;
  fn->ops->evaluatefunctionmat[3]    = FNEvaluateFunctionMat_Invsqrt_Sadeghi;
#if defined(PETSC_HAVE_CUDA)
  fn->ops->evaluatefunctionmat[4]    = FNEvaluateFunctionMat_Invsqrt_NS_CUDA;
#if defined(PETSC_HAVE_MAGMA)
  fn->ops->evaluatefunctionmat[5]    = FNEvaluateFunctionMat_Invsqrt_DBP_CUDAm;
  fn->ops->evaluatefunctionmat[6]    = FNEvaluateFunctionMat_Invsqrt_Sadeghi_CUDAm;
#endif /* PETSC_HAVE_MAGMA */
#endif /* PETSC_HAVE_CUDA */
  fn->ops->evaluatefunctionmatvec[0] = FNEvaluateFunctionMatVec_Invsqrt_Schur;
  fn->ops->view                      = FNView_Invsqrt;
  PetscFunctionReturn(0);
}
