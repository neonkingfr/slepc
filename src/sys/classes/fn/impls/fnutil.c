/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Utility subroutines common to several impls
*/

#include <slepc/private/fnimpl.h>      /*I "slepcfn.h" I*/
#include <slepcblaslapack.h>

/*
   Compute the square root of an upper quasi-triangular matrix T,
   using Higham's algorithm (LAA 88, 1987). T is overwritten with sqrtm(T).
 */
static PetscErrorCode SlepcMatDenseSqrt(PetscBLASInt n,PetscScalar *T,PetscBLASInt ld)
{
  PetscScalar  one=1.0,mone=-1.0;
  PetscReal    scal;
  PetscBLASInt i,j,si,sj,r,ione=1,info;
#if !defined(PETSC_USE_COMPLEX)
  PetscReal    alpha,theta,mu,mu2;
#endif

  PetscFunctionBegin;
  for (j=0;j<n;j++) {
#if defined(PETSC_USE_COMPLEX)
    sj = 1;
    T[j+j*ld] = PetscSqrtScalar(T[j+j*ld]);
#else
    sj = (j==n-1 || T[j+1+j*ld] == 0.0)? 1: 2;
    if (sj==1) {
      PetscCheck(T[j+j*ld]>=0.0,PETSC_COMM_SELF,PETSC_ERR_USER_INPUT,"Matrix has a real negative eigenvalue, no real primary square root exists");
      T[j+j*ld] = PetscSqrtReal(T[j+j*ld]);
    } else {
      /* square root of 2x2 block */
      theta = (T[j+j*ld]+T[j+1+(j+1)*ld])/2.0;
      mu    = (T[j+j*ld]-T[j+1+(j+1)*ld])/2.0;
      mu2   = -mu*mu-T[j+1+j*ld]*T[j+(j+1)*ld];
      mu    = PetscSqrtReal(mu2);
      if (theta>0.0) alpha = PetscSqrtReal((theta+PetscSqrtReal(theta*theta+mu2))/2.0);
      else alpha = mu/PetscSqrtReal(2.0*(-theta+PetscSqrtReal(theta*theta+mu2)));
      T[j+j*ld]       /= 2.0*alpha;
      T[j+1+(j+1)*ld] /= 2.0*alpha;
      T[j+(j+1)*ld]   /= 2.0*alpha;
      T[j+1+j*ld]     /= 2.0*alpha;
      T[j+j*ld]       += alpha-theta/(2.0*alpha);
      T[j+1+(j+1)*ld] += alpha-theta/(2.0*alpha);
    }
#endif
    for (i=j-1;i>=0;i--) {
#if defined(PETSC_USE_COMPLEX)
      si = 1;
#else
      si = (i==0 || T[i+(i-1)*ld] == 0.0)? 1: 2;
      if (si==2) i--;
#endif
      /* solve Sylvester equation of order si x sj */
      r = j-i-si;
      if (r) PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&si,&sj,&r,&mone,T+i+(i+si)*ld,&ld,T+i+si+j*ld,&ld,&one,T+i+j*ld,&ld));
      PetscStackCallBLAS("LAPACKtrsyl",LAPACKtrsyl_("N","N",&ione,&si,&sj,T+i+i*ld,&ld,T+j+j*ld,&ld,T+i+j*ld,&ld,&scal,&info));
      SlepcCheckLapackInfo("trsyl",info);
      PetscCheck(scal==1.0,PETSC_COMM_SELF,PETSC_ERR_SUP,"Current implementation cannot handle scale factor %g",(double)scal);
    }
    if (sj==2) j++;
  }
  PetscFunctionReturn(0);
}

#define BLOCKSIZE 64

/*
   Schur method for the square root of an upper quasi-triangular matrix T.
   T is overwritten with sqrtm(T).
   If firstonly then only the first column of T will contain relevant values.
 */
PetscErrorCode FNSqrtmSchur(FN fn,PetscBLASInt n,PetscScalar *T,PetscBLASInt ld,PetscBool firstonly)
{
  PetscBLASInt   i,j,k,r,ione=1,sdim,lwork,*s,*p,info,bs=BLOCKSIZE;
  PetscScalar    *wr,*W,*Q,*work,one=1.0,zero=0.0,mone=-1.0;
  PetscInt       m,nblk;
  PetscReal      scal;
#if defined(PETSC_USE_COMPLEX)
  PetscReal      *rwork;
#else
  PetscReal      *wi;
#endif

  PetscFunctionBegin;
  m     = n;
  nblk  = (m+bs-1)/bs;
  lwork = 5*n;
  k     = firstonly? 1: n;

  /* compute Schur decomposition A*Q = Q*T */
#if !defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscMalloc7(m,&wr,m,&wi,m*k,&W,m*m,&Q,lwork,&work,nblk,&s,nblk,&p));
  PetscStackCallBLAS("LAPACKgees",LAPACKgees_("V","N",NULL,&n,T,&ld,&sdim,wr,wi,Q,&ld,work,&lwork,NULL,&info));
#else
  CHKERRQ(PetscMalloc7(m,&wr,m,&rwork,m*k,&W,m*m,&Q,lwork,&work,nblk,&s,nblk,&p));
  PetscStackCallBLAS("LAPACKgees",LAPACKgees_("V","N",NULL,&n,T,&ld,&sdim,wr,Q,&ld,work,&lwork,rwork,NULL,&info));
#endif
  SlepcCheckLapackInfo("gees",info);

  /* determine block sizes and positions, to avoid cutting 2x2 blocks */
  j = 0;
  p[j] = 0;
  do {
    s[j] = PetscMin(bs,n-p[j]);
#if !defined(PETSC_USE_COMPLEX)
    if (p[j]+s[j]!=n && T[p[j]+s[j]+(p[j]+s[j]-1)*ld]!=0.0) s[j]++;
#endif
    if (p[j]+s[j]==n) break;
    j++;
    p[j] = p[j-1]+s[j-1];
  } while (1);
  nblk = j+1;

  for (j=0;j<nblk;j++) {
    /* evaluate f(T_jj) */
    CHKERRQ(SlepcMatDenseSqrt(s[j],T+p[j]+p[j]*ld,ld));
    for (i=j-1;i>=0;i--) {
      /* solve Sylvester equation for block (i,j) */
      r = p[j]-p[i]-s[i];
      if (r) PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",s+i,s+j,&r,&mone,T+p[i]+(p[i]+s[i])*ld,&ld,T+p[i]+s[i]+p[j]*ld,&ld,&one,T+p[i]+p[j]*ld,&ld));
      PetscStackCallBLAS("LAPACKtrsyl",LAPACKtrsyl_("N","N",&ione,s+i,s+j,T+p[i]+p[i]*ld,&ld,T+p[j]+p[j]*ld,&ld,T+p[i]+p[j]*ld,&ld,&scal,&info));
      SlepcCheckLapackInfo("trsyl",info);
      PetscCheck(scal==1.0,PETSC_COMM_SELF,PETSC_ERR_SUP,"Current implementation cannot handle scale factor %g",(double)scal);
    }
  }

  /* backtransform B = Q*T*Q' */
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","C",&n,&k,&n,&one,T,&ld,Q,&ld,&zero,W,&ld));
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&k,&n,&one,Q,&ld,W,&ld,&zero,T,&ld));

  /* flop count: Schur decomposition, triangular square root, and backtransform */
  CHKERRQ(PetscLogFlops(25.0*n*n*n+n*n*n/3.0+4.0*n*n*k));

#if !defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscFree7(wr,wi,W,Q,work,s,p));
#else
  CHKERRQ(PetscFree7(wr,rwork,W,Q,work,s,p));
#endif
  PetscFunctionReturn(0);
}

#define DBMAXIT 25

/*
   Computes the principal square root of the matrix T using the product form
   of the Denman-Beavers iteration.
   T is overwritten with sqrtm(T) or inv(sqrtm(T)) depending on flag inv.
 */
PetscErrorCode FNSqrtmDenmanBeavers(FN fn,PetscBLASInt n,PetscScalar *T,PetscBLASInt ld,PetscBool inv)
{
  PetscScalar        *Told,*M=NULL,*invM,*work,work1,prod,alpha;
  PetscScalar        szero=0.0,sone=1.0,smone=-1.0,spfive=0.5,sp25=0.25;
  PetscReal          tol,Mres=0.0,detM,g,reldiff,fnormdiff,fnormT,rwork[1];
  PetscBLASInt       N,i,it,*piv=NULL,info,query=-1,lwork;
  const PetscBLASInt one=1;
  PetscBool          converged=PETSC_FALSE,scale=PETSC_FALSE;
  unsigned int       ftz;

  PetscFunctionBegin;
  N = n*n;
  tol = PetscSqrtReal((PetscReal)n)*PETSC_MACHINE_EPSILON/2;
  CHKERRQ(SlepcSetFlushToZero(&ftz));

  /* query work size */
  PetscStackCallBLAS("LAPACKgetri",LAPACKgetri_(&n,M,&ld,piv,&work1,&query,&info));
  CHKERRQ(PetscBLASIntCast((PetscInt)PetscRealPart(work1),&lwork));
  CHKERRQ(PetscMalloc5(lwork,&work,n,&piv,n*n,&Told,n*n,&M,n*n,&invM));
  CHKERRQ(PetscArraycpy(M,T,n*n));

  if (inv) {  /* start recurrence with I instead of A */
    CHKERRQ(PetscArrayzero(T,n*n));
    for (i=0;i<n;i++) T[i+i*ld] += 1.0;
  }

  for (it=0;it<DBMAXIT && !converged;it++) {

    if (scale) {  /* g = (abs(det(M)))^(-1/(2*n)) */
      CHKERRQ(PetscArraycpy(invM,M,n*n));
      PetscStackCallBLAS("LAPACKgetrf",LAPACKgetrf_(&n,&n,invM,&ld,piv,&info));
      SlepcCheckLapackInfo("getrf",info);
      prod = invM[0];
      for (i=1;i<n;i++) prod *= invM[i+i*ld];
      detM = PetscAbsScalar(prod);
      g = PetscPowReal(detM,-1.0/(2.0*n));
      alpha = g;
      PetscStackCallBLAS("BLASscal",BLASscal_(&N,&alpha,T,&one));
      alpha = g*g;
      PetscStackCallBLAS("BLASscal",BLASscal_(&N,&alpha,M,&one));
      CHKERRQ(PetscLogFlops(2.0*n*n*n/3.0+2.0*n*n));
    }

    CHKERRQ(PetscArraycpy(Told,T,n*n));
    CHKERRQ(PetscArraycpy(invM,M,n*n));

    PetscStackCallBLAS("LAPACKgetrf",LAPACKgetrf_(&n,&n,invM,&ld,piv,&info));
    SlepcCheckLapackInfo("getrf",info);
    PetscStackCallBLAS("LAPACKgetri",LAPACKgetri_(&n,invM,&ld,piv,work,&lwork,&info));
    SlepcCheckLapackInfo("getri",info);
    CHKERRQ(PetscLogFlops(2.0*n*n*n/3.0+4.0*n*n*n/3.0));

    for (i=0;i<n;i++) invM[i+i*ld] += 1.0;
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&spfive,Told,&ld,invM,&ld,&szero,T,&ld));
    for (i=0;i<n;i++) invM[i+i*ld] -= 1.0;

    PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&N,&sone,invM,&one,M,&one));
    PetscStackCallBLAS("BLASscal",BLASscal_(&N,&sp25,M,&one));
    for (i=0;i<n;i++) M[i+i*ld] -= 0.5;
    CHKERRQ(PetscLogFlops(2.0*n*n*n+2.0*n*n));

    Mres = LAPACKlange_("F",&n,&n,M,&n,rwork);
    for (i=0;i<n;i++) M[i+i*ld] += 1.0;

    if (scale) {
      /* reldiff = norm(T - Told,'fro')/norm(T,'fro') */
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&N,&smone,T,&one,Told,&one));
      fnormdiff = LAPACKlange_("F",&n,&n,Told,&n,rwork);
      fnormT = LAPACKlange_("F",&n,&n,T,&n,rwork);
      CHKERRQ(PetscLogFlops(7.0*n*n));
      reldiff = fnormdiff/fnormT;
      CHKERRQ(PetscInfo(fn,"it: %" PetscBLASInt_FMT " reldiff: %g scale: %g tol*scale: %g\n",it,(double)reldiff,(double)g,(double)(tol*g)));
      if (reldiff<1e-2) scale = PETSC_FALSE;  /* Switch off scaling */
    }

    if (Mres<=tol) converged = PETSC_TRUE;
  }

  PetscCheck(Mres<=tol,PETSC_COMM_SELF,PETSC_ERR_LIB,"SQRTM not converged after %d iterations",DBMAXIT);
  CHKERRQ(PetscFree5(work,piv,Told,M,invM));
  CHKERRQ(SlepcResetFlushToZero(&ftz));
  PetscFunctionReturn(0);
}

#define NSMAXIT 50

/*
   Computes the principal square root of the matrix A using the Newton-Schulz iteration.
   T is overwritten with sqrtm(T) or inv(sqrtm(T)) depending on flag inv.
 */
PetscErrorCode FNSqrtmNewtonSchulz(FN fn,PetscBLASInt n,PetscScalar *A,PetscBLASInt ld,PetscBool inv)
{
  PetscScalar    *Y=A,*Yold,*Z,*Zold,*M;
  PetscScalar    szero=0.0,sone=1.0,smone=-1.0,spfive=0.5,sthree=3.0;
  PetscReal      sqrtnrm,tol,Yres=0.0,nrm,rwork[1],done=1.0;
  PetscBLASInt   info,i,it,N,one=1,zero=0;
  PetscBool      converged=PETSC_FALSE;
  unsigned int   ftz;

  PetscFunctionBegin;
  N = n*n;
  tol = PetscSqrtReal((PetscReal)n)*PETSC_MACHINE_EPSILON/2;
  CHKERRQ(SlepcSetFlushToZero(&ftz));

  CHKERRQ(PetscMalloc4(N,&Yold,N,&Z,N,&Zold,N,&M));

  /* scale A so that ||I-A|| < 1 */
  CHKERRQ(PetscArraycpy(Z,A,N));
  for (i=0;i<n;i++) Z[i+i*ld] -= 1.0;
  nrm = LAPACKlange_("fro",&n,&n,Z,&n,rwork);
  sqrtnrm = PetscSqrtReal(nrm);
  PetscStackCallBLAS("LAPACKlascl",LAPACKlascl_("G",&zero,&zero,&nrm,&done,&N,&one,A,&N,&info));
  SlepcCheckLapackInfo("lascl",info);
  tol *= nrm;
  CHKERRQ(PetscInfo(fn,"||I-A||_F = %g, new tol: %g\n",(double)nrm,(double)tol));
  CHKERRQ(PetscLogFlops(2.0*n*n));

  /* Z = I */
  CHKERRQ(PetscArrayzero(Z,N));
  for (i=0;i<n;i++) Z[i+i*ld] = 1.0;

  for (it=0;it<NSMAXIT && !converged;it++) {
    /* Yold = Y, Zold = Z */
    CHKERRQ(PetscArraycpy(Yold,Y,N));
    CHKERRQ(PetscArraycpy(Zold,Z,N));

    /* M = (3*I-Zold*Yold) */
    CHKERRQ(PetscArrayzero(M,N));
    for (i=0;i<n;i++) M[i+i*ld] = sthree;
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&smone,Zold,&ld,Yold,&ld,&sone,M,&ld));

    /* Y = (1/2)*Yold*M, Z = (1/2)*M*Zold */
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&spfive,Yold,&ld,M,&ld,&szero,Y,&ld));
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&spfive,M,&ld,Zold,&ld,&szero,Z,&ld));

    /* reldiff = norm(Y-Yold,'fro')/norm(Y,'fro') */
    PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&N,&smone,Y,&one,Yold,&one));
    Yres = LAPACKlange_("fro",&n,&n,Yold,&n,rwork);
    PetscCheck(!PetscIsNanReal(Yres),PETSC_COMM_SELF,PETSC_ERR_FP,"The computed norm is not-a-number");
    if (Yres<=tol) converged = PETSC_TRUE;
    CHKERRQ(PetscInfo(fn,"it: %" PetscBLASInt_FMT " res: %g\n",it,(double)Yres));

    CHKERRQ(PetscLogFlops(6.0*n*n*n+2.0*n*n));
  }

  PetscCheck(Yres<=tol,PETSC_COMM_SELF,PETSC_ERR_LIB,"SQRTM not converged after %d iterations",NSMAXIT);

  /* undo scaling */
  if (inv) {
    CHKERRQ(PetscArraycpy(A,Z,N));
    PetscStackCallBLAS("LAPACKlascl",LAPACKlascl_("G",&zero,&zero,&sqrtnrm,&done,&N,&one,A,&N,&info));
  } else PetscStackCallBLAS("LAPACKlascl",LAPACKlascl_("G",&zero,&zero,&done,&sqrtnrm,&N,&one,A,&N,&info));
  SlepcCheckLapackInfo("lascl",info);

  CHKERRQ(PetscFree4(Yold,Z,Zold,M));
  CHKERRQ(SlepcResetFlushToZero(&ftz));
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_CUDA)
#include "../src/sys/classes/fn/impls/cuda/fnutilcuda.h"
#include <slepccublas.h>

/*
 * Matrix square root by Newton-Schulz iteration. CUDA version.
 * Computes the principal square root of the matrix T using the
 * Newton-Schulz iteration. T is overwritten with sqrtm(T).
 */
PetscErrorCode FNSqrtmNewtonSchulz_CUDA(FN fn,PetscBLASInt n,PetscScalar *A,PetscBLASInt ld,PetscBool inv)
{
  PetscScalar        *d_A,*d_Yold,*d_Z,*d_Zold,*d_M;
  PetscReal          nrm,sqrtnrm;
  const PetscScalar  szero=0.0,sone=1.0,smone=-1.0,spfive=0.5,sthree=3.0;
  PetscReal          tol,Yres=0.0,alpha;
  PetscInt           it;
  PetscBLASInt       N;
  const PetscBLASInt one=1,zero=0;
  PetscBool          converged=PETSC_FALSE;
  cublasHandle_t     cublasv2handle;

  PetscFunctionBegin;
  CHKERRQ(PetscDeviceInitialize(PETSC_DEVICE_CUDA)); /* For CUDA event timers */
  CHKERRQ(PetscCUBLASGetHandle(&cublasv2handle));
  N = n*n;
  tol = PetscSqrtReal((PetscReal)n)*PETSC_MACHINE_EPSILON/2;

  CHKERRCUDA(cudaMalloc((void **)&d_A,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_Yold,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_Z,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_Zold,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_M,sizeof(PetscScalar)*N));

  CHKERRQ(PetscLogGpuTimeBegin());

  /* Y = A; */
  CHKERRCUDA(cudaMemcpy(d_A,A,sizeof(PetscScalar)*N,cudaMemcpyHostToDevice));
  /* Z = I; */
  CHKERRCUDA(cudaMemset(d_Z,zero,sizeof(PetscScalar)*N));
  CHKERRQ(set_diagonal(n,d_Z,ld,sone));

  /* scale A so that ||I-A|| < 1 */
  CHKERRCUBLAS(cublasXaxpy(cublasv2handle,N,&smone,d_A,one,d_Z,one));
  CHKERRCUBLAS(cublasXnrm2(cublasv2handle,N,d_Z,one,&nrm));
  sqrtnrm = PetscSqrtReal(nrm);
  alpha = 1.0/nrm;
  CHKERRCUBLAS(cublasXscal(cublasv2handle,N,&alpha,d_A,one));
  tol *= nrm;
  CHKERRQ(PetscInfo(fn,"||I-A||_F = %g, new tol: %g\n",(double)nrm,(double)tol));
  CHKERRQ(PetscLogGpuFlops(2.0*n*n));

  /* Z = I; */
  CHKERRCUDA(cudaMemset(d_Z,zero,sizeof(PetscScalar)*N));
  CHKERRQ(set_diagonal(n,d_Z,ld,sone));

  for (it=0;it<NSMAXIT && !converged;it++) {
    /* Yold = Y, Zold = Z */
    CHKERRCUDA(cudaMemcpy(d_Yold,d_A,sizeof(PetscScalar)*N,cudaMemcpyDeviceToDevice));
    CHKERRCUDA(cudaMemcpy(d_Zold,d_Z,sizeof(PetscScalar)*N,cudaMemcpyDeviceToDevice));

    /* M = (3*I - Zold*Yold) */
    CHKERRCUDA(cudaMemset(d_M,zero,sizeof(PetscScalar)*N));
    CHKERRQ(set_diagonal(n,d_M,ld,sthree));
    CHKERRCUBLAS(cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&smone,d_Zold,ld,d_Yold,ld,&sone,d_M,ld));

    /* Y = (1/2) * Yold * M, Z = (1/2) * M * Zold */
    CHKERRCUBLAS(cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&spfive,d_Yold,ld,d_M,ld,&szero,d_A,ld));
    CHKERRCUBLAS(cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&spfive,d_M,ld,d_Zold,ld,&szero,d_Z,ld));

    /* reldiff = norm(Y-Yold,'fro')/norm(Y,'fro') */
    CHKERRCUBLAS(cublasXaxpy(cublasv2handle,N,&smone,d_A,one,d_Yold,one));
    CHKERRCUBLAS(cublasXnrm2(cublasv2handle,N,d_Yold,one,&Yres));
    PetscCheck(!PetscIsNanReal(Yres),PETSC_COMM_SELF,PETSC_ERR_FP,"The computed norm is not-a-number");
    if (Yres<=tol) converged = PETSC_TRUE;
    CHKERRQ(PetscInfo(fn,"it: %" PetscInt_FMT " res: %g\n",it,(double)Yres));

    CHKERRQ(PetscLogGpuFlops(6.0*n*n*n+2.0*n*n));
  }

  PetscCheck(Yres<=tol,PETSC_COMM_SELF,PETSC_ERR_LIB,"SQRTM not converged after %d iterations", NSMAXIT);

  /* undo scaling */
  if (inv) {
    sqrtnrm = 1.0/sqrtnrm;
    CHKERRCUBLAS(cublasXscal(cublasv2handle,N,&sqrtnrm,d_Z,one));
    CHKERRCUDA(cudaMemcpy(A,d_Z,sizeof(PetscScalar)*N,cudaMemcpyDeviceToHost));
  } else {
    CHKERRCUBLAS(cublasXscal(cublasv2handle,N,&sqrtnrm,d_A,one));
    CHKERRCUDA(cudaMemcpy(A,d_A,sizeof(PetscScalar)*N,cudaMemcpyDeviceToHost));
  }

  CHKERRQ(PetscLogGpuTimeEnd());
  CHKERRCUDA(cudaFree(d_A));
  CHKERRCUDA(cudaFree(d_Yold));
  CHKERRCUDA(cudaFree(d_Z));
  CHKERRCUDA(cudaFree(d_Zold));
  CHKERRCUDA(cudaFree(d_M));
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_MAGMA)
#include <slepcmagma.h>

/*
 * Matrix square root by product form of Denman-Beavers iteration. CUDA version.
 * Computes the principal square root of the matrix T using the product form
 * of the Denman-Beavers iteration. T is overwritten with sqrtm(T).
 */
PetscErrorCode FNSqrtmDenmanBeavers_CUDAm(FN fn,PetscBLASInt n,PetscScalar *T,PetscBLASInt ld,PetscBool inv)
{
  PetscScalar    *d_T,*d_Told,*d_M,*d_invM,*d_work,zero=0.0,sone=1.0,smone=-1.0,spfive=0.5,sneg_pfive=-0.5,sp25=0.25,alpha;
  PetscReal      tol,Mres=0.0,detM,g,reldiff,fnormdiff,fnormT,prod;
  PetscInt       i,it,lwork,nb;
  PetscBLASInt   N,one=1,info,*piv=NULL;
  PetscBool      converged=PETSC_FALSE,scale=PETSC_FALSE;
  cublasHandle_t cublasv2handle;

  PetscFunctionBegin;
  CHKERRQ(PetscDeviceInitialize(PETSC_DEVICE_CUDA)); /* For CUDA event timers */
  CHKERRQ(PetscCUBLASGetHandle(&cublasv2handle));
  magma_init();
  N = n*n;
  tol = PetscSqrtReal((PetscReal)n)*PETSC_MACHINE_EPSILON/2;

  /* query work size */
  nb = magma_get_xgetri_nb(n);
  lwork = nb*n;
  CHKERRQ(PetscMalloc1(n,&piv));
  CHKERRCUDA(cudaMalloc((void **)&d_work,sizeof(PetscScalar)*lwork));
  CHKERRCUDA(cudaMalloc((void **)&d_T,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_Told,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_M,sizeof(PetscScalar)*N));
  CHKERRCUDA(cudaMalloc((void **)&d_invM,sizeof(PetscScalar)*N));

  CHKERRQ(PetscLogGpuTimeBegin());
  CHKERRCUDA(cudaMemcpy(d_M,T,sizeof(PetscScalar)*N,cudaMemcpyHostToDevice));
  if (inv) {  /* start recurrence with I instead of A */
    CHKERRCUDA(cudaMemset(d_T,zero,sizeof(PetscScalar)*N));
    CHKERRQ(set_diagonal(n,d_T,ld,1.0));
  } else CHKERRCUDA(cudaMemcpy(d_T,T,sizeof(PetscScalar)*N,cudaMemcpyHostToDevice));

  for (it=0;it<DBMAXIT && !converged;it++) {

    if (scale) { /* g = (abs(det(M)))^(-1/(2*n)); */
      CHKERRCUDA(cudaMemcpy(d_invM,d_M,sizeof(PetscScalar)*N,cudaMemcpyDeviceToDevice));
      CHKERRMAGMA(magma_xgetrf_gpu(n,n,d_invM,ld,piv,&info));
      PetscCheck(info>=0,PETSC_COMM_SELF,PETSC_ERR_LIB,"LAPACKgetrf: Illegal value on argument %" PetscBLASInt_FMT,PetscAbsInt(info));
      PetscCheck(info<=0,PETSC_COMM_SELF,PETSC_ERR_MAT_LU_ZRPVT,"LAPACKgetrf: Matrix is singular. U(%" PetscBLASInt_FMT ",%" PetscBLASInt_FMT ") is zero",info,info);

      /* XXX pending */
//      CHKERRQ(mult_diagonal(d_invM,n,ld,&detM));
      CHKERRCUDA(cudaMemcpy(T,d_invM,sizeof(PetscScalar)*N,cudaMemcpyDeviceToHost));
      prod = T[0];
      for (i=1;i<n;i++) { prod *= T[i+i*ld]; }
      detM = PetscAbsReal(prod);
      g = PetscPowReal(detM,-1.0/(2.0*n));
      alpha = g;
      CHKERRCUBLAS(cublasXscal(cublasv2handle,N,&alpha,d_T,one));
      alpha = g*g;
      CHKERRCUBLAS(cublasXscal(cublasv2handle,N,&alpha,d_M,one));
      CHKERRQ(PetscLogGpuFlops(2.0*n*n*n/3.0+2.0*n*n));
    }

    CHKERRCUDA(cudaMemcpy(d_Told,d_T,sizeof(PetscScalar)*N,cudaMemcpyDeviceToDevice));
    CHKERRCUDA(cudaMemcpy(d_invM,d_M,sizeof(PetscScalar)*N,cudaMemcpyDeviceToDevice));

    CHKERRMAGMA(magma_xgetrf_gpu(n,n,d_invM,ld,piv,&info));
    PetscCheck(info>=0,PETSC_COMM_SELF,PETSC_ERR_LIB,"LAPACKgetrf: Illegal value on argument %" PetscBLASInt_FMT,PetscAbsInt(info));
    PetscCheck(info<=0,PETSC_COMM_SELF,PETSC_ERR_MAT_LU_ZRPVT,"LAPACKgetrf: Matrix is singular. U(%" PetscBLASInt_FMT ",%" PetscBLASInt_FMT ") is zero",info,info);
    CHKERRMAGMA(magma_xgetri_gpu(n,d_invM,ld,piv,d_work,lwork,&info));
    PetscCheck(info>=0,PETSC_COMM_SELF,PETSC_ERR_LIB,"LAPACKgetri: Illegal value on argument %" PetscBLASInt_FMT,PetscAbsInt(info));
    PetscCheck(info<=0,PETSC_COMM_SELF,PETSC_ERR_MAT_LU_ZRPVT,"LAPACKgetri: Matrix is singular. U(%" PetscBLASInt_FMT ",%" PetscBLASInt_FMT ") is zero",info,info);
    CHKERRQ(PetscLogGpuFlops(2.0*n*n*n/3.0+4.0*n*n*n/3.0));

    CHKERRQ(shift_diagonal(n,d_invM,ld,sone));
    CHKERRCUBLAS(cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&spfive,d_Told,ld,d_invM,ld,&zero,d_T,ld));
    CHKERRQ(shift_diagonal(n,d_invM,ld,smone));

    CHKERRCUBLAS(cublasXaxpy(cublasv2handle,N,&sone,d_invM,one,d_M,one));
    CHKERRCUBLAS(cublasXscal(cublasv2handle,N,&sp25,d_M,one));
    CHKERRQ(shift_diagonal(n,d_M,ld,sneg_pfive));
    CHKERRQ(PetscLogGpuFlops(2.0*n*n*n+2.0*n*n));

    CHKERRCUBLAS(cublasXnrm2(cublasv2handle,N,d_M,one,&Mres));
    CHKERRQ(shift_diagonal(n,d_M,ld,sone));

    if (scale) {
      // reldiff = norm(T - Told,'fro')/norm(T,'fro');
      CHKERRCUBLAS(cublasXaxpy(cublasv2handle,N,&smone,d_T,one,d_Told,one));
      CHKERRCUBLAS(cublasXnrm2(cublasv2handle,N,d_Told,one,&fnormdiff));
      CHKERRCUBLAS(cublasXnrm2(cublasv2handle,N,d_T,one,&fnormT));
      CHKERRQ(PetscLogGpuFlops(7.0*n*n));
      reldiff = fnormdiff/fnormT;
      CHKERRQ(PetscInfo(fn,"it: %" PetscInt_FMT " reldiff: %g scale: %g tol*scale: %g\n",it,(double)reldiff,(double)g,(double)tol*g));
      if (reldiff<1e-2) scale = PETSC_FALSE; /* Switch to no scaling. */
    }

    CHKERRQ(PetscInfo(fn,"it: %" PetscInt_FMT " Mres: %g\n",it,(double)Mres));
    if (Mres<=tol) converged = PETSC_TRUE;
  }

  PetscCheck(Mres<=tol,PETSC_COMM_SELF,PETSC_ERR_LIB,"SQRTM not converged after %d iterations", DBMAXIT);
  CHKERRCUDA(cudaMemcpy(T,d_T,sizeof(PetscScalar)*N,cudaMemcpyDeviceToHost));
  CHKERRQ(PetscLogGpuTimeEnd());
  CHKERRQ(PetscFree(piv));
  CHKERRCUDA(cudaFree(d_work));
  CHKERRCUDA(cudaFree(d_T));
  CHKERRCUDA(cudaFree(d_Told));
  CHKERRCUDA(cudaFree(d_M));
  CHKERRCUDA(cudaFree(d_invM));
  magma_finalize();
  PetscFunctionReturn(0);
}
#endif /* PETSC_HAVE_MAGMA */

#endif /* PETSC_HAVE_CUDA */

#define ITMAX 5
#define SWAP(a,b,t) {t=a;a=b;b=t;}

/*
   Estimate norm(A^m,1) by block 1-norm power method (required workspace is 11*n)
*/
static PetscErrorCode SlepcNormEst1(PetscBLASInt n,PetscScalar *A,PetscInt m,PetscScalar *work,PetscRandom rand,PetscReal *nrm)
{
  PetscScalar    *X,*Y,*Z,*S,*S_old,*aux,val,sone=1.0,szero=0.0;
  PetscReal      est=0.0,est_old,vals[2]={0.0,0.0},*zvals,maxzval[2],raux;
  PetscBLASInt   i,j,t=2,it=0,ind[2],est_j=0,m1;

  PetscFunctionBegin;
  X = work;
  Y = work + 2*n;
  Z = work + 4*n;
  S = work + 6*n;
  S_old = work + 8*n;
  zvals = (PetscReal*)(work + 10*n);

  for (i=0;i<n;i++) {  /* X has columns of unit 1-norm */
    X[i] = 1.0/n;
    CHKERRQ(PetscRandomGetValue(rand,&val));
    if (PetscRealPart(val) < 0.5) X[i+n] = -1.0/n;
    else X[i+n] = 1.0/n;
  }
  for (i=0;i<t*n;i++) S[i] = 0.0;
  ind[0] = 0; ind[1] = 0;
  est_old = 0;
  while (1) {
    it++;
    for (j=0;j<m;j++) {  /* Y = A^m*X */
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&t,&n,&sone,A,&n,X,&n,&szero,Y,&n));
      if (j<m-1) SWAP(X,Y,aux);
    }
    for (j=0;j<t;j++) {  /* vals[j] = norm(Y(:,j),1) */
      vals[j] = 0.0;
      for (i=0;i<n;i++) vals[j] += PetscAbsScalar(Y[i+j*n]);
    }
    if (vals[0]<vals[1]) {
      SWAP(vals[0],vals[1],raux);
      m1 = 1;
    } else m1 = 0;
    est = vals[0];
    if (est>est_old || it==2) est_j = ind[m1];
    if (it>=2 && est<=est_old) {
      est = est_old;
      break;
    }
    est_old = est;
    if (it>ITMAX) break;
    SWAP(S,S_old,aux);
    for (i=0;i<t*n;i++) {  /* S = sign(Y) */
      S[i] = (PetscRealPart(Y[i]) < 0.0)? -1.0: 1.0;
    }
    for (j=0;j<m;j++) {  /* Z = (A^T)^m*S */
      PetscStackCallBLAS("BLASgemm",BLASgemm_("C","N",&n,&t,&n,&sone,A,&n,S,&n,&szero,Z,&n));
      if (j<m-1) SWAP(S,Z,aux);
    }
    maxzval[0] = -1; maxzval[1] = -1;
    ind[0] = 0; ind[1] = 0;
    for (i=0;i<n;i++) {  /* zvals[i] = norm(Z(i,:),inf) */
      zvals[i] = PetscMax(PetscAbsScalar(Z[i+0*n]),PetscAbsScalar(Z[i+1*n]));
      if (zvals[i]>maxzval[0]) {
        maxzval[0] = zvals[i];
        ind[0] = i;
      } else if (zvals[i]>maxzval[1]) {
        maxzval[1] = zvals[i];
        ind[1] = i;
      }
    }
    if (it>=2 && maxzval[0]==zvals[est_j]) break;
    for (i=0;i<t*n;i++) X[i] = 0.0;
    for (j=0;j<t;j++) X[ind[j]+j*n] = 1.0;
  }
  *nrm = est;
  /* Flop count is roughly (it * 2*m * t*gemv) = 4*its*m*t*n*n */
  CHKERRQ(PetscLogFlops(4.0*it*m*t*n*n));
  PetscFunctionReturn(0);
}

#define SMALLN 100

/*
   Estimate norm(A^m,1) (required workspace is 2*n*n)
*/
PetscErrorCode SlepcNormAm(PetscBLASInt n,PetscScalar *A,PetscInt m,PetscScalar *work,PetscRandom rand,PetscReal *nrm)
{
  PetscScalar    *v=work,*w=work+n*n,*aux,sone=1.0,szero=0.0;
  PetscReal      rwork[1],tmp;
  PetscBLASInt   i,j,one=1;
  PetscBool      isrealpos=PETSC_TRUE;

  PetscFunctionBegin;
  if (n<SMALLN) {   /* compute matrix power explicitly */
    if (m==1) {
      *nrm = LAPACKlange_("O",&n,&n,A,&n,rwork);
      CHKERRQ(PetscLogFlops(1.0*n*n));
    } else {  /* m>=2 */
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&sone,A,&n,A,&n,&szero,v,&n));
      for (j=0;j<m-2;j++) {
        PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&sone,A,&n,v,&n,&szero,w,&n));
        SWAP(v,w,aux);
      }
      *nrm = LAPACKlange_("O",&n,&n,v,&n,rwork);
      CHKERRQ(PetscLogFlops(2.0*n*n*n*(m-1)+1.0*n*n));
    }
  } else {
    for (i=0;i<n;i++)
      for (j=0;j<n;j++)
#if defined(PETSC_USE_COMPLEX)
        if (PetscRealPart(A[i+j*n])<0.0 || PetscImaginaryPart(A[i+j*n])!=0.0) { isrealpos = PETSC_FALSE; break; }
#else
        if (A[i+j*n]<0.0) { isrealpos = PETSC_FALSE; break; }
#endif
    if (isrealpos) {   /* for positive matrices only */
      for (i=0;i<n;i++) v[i] = 1.0;
      for (j=0;j<m;j++) {  /* w = A'*v */
        PetscStackCallBLAS("BLASgemv",BLASgemv_("C",&n,&n,&sone,A,&n,v,&one,&szero,w,&one));
        SWAP(v,w,aux);
      }
      CHKERRQ(PetscLogFlops(2.0*n*n*m));
      *nrm = 0.0;
      for (i=0;i<n;i++) if ((tmp = PetscAbsScalar(v[i])) > *nrm) *nrm = tmp;   /* norm(v,inf) */
    } else CHKERRQ(SlepcNormEst1(n,A,m,work,rand,nrm));
  }
  PetscFunctionReturn(0);
}
