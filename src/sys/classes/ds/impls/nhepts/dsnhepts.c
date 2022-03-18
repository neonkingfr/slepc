/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#include <slepc/private/dsimpl.h>
#include <slepcblaslapack.h>

typedef struct {
  PetscScalar *wr,*wi;     /* eigenvalues of B */
} DS_NHEPTS;

PetscErrorCode DSAllocate_NHEPTS(DS ds,PetscInt ld)
{
  DS_NHEPTS      *ctx = (DS_NHEPTS*)ds->data;

  PetscFunctionBegin;
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_A));
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_B));
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_Q));
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_Z));
  CHKERRQ(PetscFree(ds->perm));
  CHKERRQ(PetscMalloc1(ld,&ds->perm));
  CHKERRQ(PetscLogObjectMemory((PetscObject)ds,ld*sizeof(PetscInt)));
  CHKERRQ(PetscMalloc1(ld,&ctx->wr));
  CHKERRQ(PetscLogObjectMemory((PetscObject)ds,ld*sizeof(PetscScalar)));
#if !defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscMalloc1(ld,&ctx->wi));
  CHKERRQ(PetscLogObjectMemory((PetscObject)ds,ld*sizeof(PetscScalar)));
#endif
  PetscFunctionReturn(0);
}

PetscErrorCode DSView_NHEPTS(DS ds,PetscViewer viewer)
{
  PetscViewerFormat format;

  PetscFunctionBegin;
  CHKERRQ(PetscViewerGetFormat(viewer,&format));
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) PetscFunctionReturn(0);
  CHKERRQ(DSViewMat(ds,viewer,DS_MAT_A));
  CHKERRQ(DSViewMat(ds,viewer,DS_MAT_B));
  if (ds->state>DS_STATE_INTERMEDIATE) {
    CHKERRQ(DSViewMat(ds,viewer,DS_MAT_Q));
    CHKERRQ(DSViewMat(ds,viewer,DS_MAT_Z));
  }
  if (ds->mat[DS_MAT_X]) CHKERRQ(DSViewMat(ds,viewer,DS_MAT_X));
  if (ds->mat[DS_MAT_Y]) CHKERRQ(DSViewMat(ds,viewer,DS_MAT_Y));
  PetscFunctionReturn(0);
}

static PetscErrorCode DSVectors_NHEPTS_Eigen_Some(DS ds,PetscInt *k,PetscReal *rnorm,PetscBool left)
{
  PetscInt       i;
  PetscBLASInt   mm=1,mout,info,ld,n,*select,inc=1,cols=1,zero=0;
  PetscScalar    sone=1.0,szero=0.0;
  PetscReal      norm,done=1.0;
  PetscBool      iscomplex = PETSC_FALSE;
  PetscScalar    *A,*Q,*X,*Y;

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  if (left) {
    A = ds->mat[DS_MAT_B];
    Q = ds->mat[DS_MAT_Z];
    X = ds->mat[DS_MAT_Y];
  } else {
    A = ds->mat[DS_MAT_A];
    Q = ds->mat[DS_MAT_Q];
    X = ds->mat[DS_MAT_X];
  }
  CHKERRQ(DSAllocateWork_Private(ds,0,0,ld));
  select = ds->iwork;
  for (i=0;i<n;i++) select[i] = (PetscBLASInt)PETSC_FALSE;

  /* compute k-th eigenvector Y of A */
  Y = X+(*k)*ld;
  select[*k] = (PetscBLASInt)PETSC_TRUE;
#if !defined(PETSC_USE_COMPLEX)
  if ((*k)<n-1 && A[(*k)+1+(*k)*ld]!=0.0) iscomplex = PETSC_TRUE;
  mm = iscomplex? 2: 1;
  if (iscomplex) select[(*k)+1] = (PetscBLASInt)PETSC_TRUE;
  CHKERRQ(DSAllocateWork_Private(ds,3*ld,0,0));
  PetscStackCallBLAS("LAPACKtrevc",LAPACKtrevc_("R","S",select,&n,A,&ld,Y,&ld,Y,&ld,&mm,&mout,ds->work,&info));
#else
  CHKERRQ(DSAllocateWork_Private(ds,2*ld,ld,0));
  PetscStackCallBLAS("LAPACKtrevc",LAPACKtrevc_("R","S",select,&n,A,&ld,Y,&ld,Y,&ld,&mm,&mout,ds->work,ds->rwork,&info));
#endif
  SlepcCheckLapackInfo("trevc",info);
  PetscCheck(mout==mm,PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Inconsistent arguments");

  /* accumulate and normalize eigenvectors */
  if (ds->state>=DS_STATE_CONDENSED) {
    CHKERRQ(PetscArraycpy(ds->work,Y,mout*ld));
    PetscStackCallBLAS("BLASgemv",BLASgemv_("N",&n,&n,&sone,Q,&ld,ds->work,&inc,&szero,Y,&inc));
#if !defined(PETSC_USE_COMPLEX)
    if (iscomplex) PetscStackCallBLAS("BLASgemv",BLASgemv_("N",&n,&n,&sone,Q,&ld,ds->work+ld,&inc,&szero,Y+ld,&inc));
#endif
    cols = 1;
    norm = BLASnrm2_(&n,Y,&inc);
#if !defined(PETSC_USE_COMPLEX)
    if (iscomplex) {
      norm = SlepcAbsEigenvalue(norm,BLASnrm2_(&n,Y+ld,&inc));
      cols = 2;
    }
#endif
    PetscStackCallBLAS("LAPACKlascl",LAPACKlascl_("G",&zero,&zero,&norm,&done,&n,&cols,Y,&ld,&info));
    SlepcCheckLapackInfo("lascl",info);
  }

  /* set output arguments */
  if (iscomplex) (*k)++;
  if (rnorm) {
    if (iscomplex) *rnorm = SlepcAbsEigenvalue(Y[n-1],Y[n-1+ld]);
    else *rnorm = PetscAbsScalar(Y[n-1]);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DSVectors_NHEPTS_Eigen_All(DS ds,PetscBool left)
{
  PetscInt       i;
  PetscBLASInt   n,ld,mout,info,inc=1,cols,zero=0;
  PetscBool      iscomplex;
  PetscScalar    *A,*Q,*X;
  PetscReal      norm,done=1.0;
  const char     *back;

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  if (left) {
    A = ds->mat[DS_MAT_B];
    Q = ds->mat[DS_MAT_Z];
    X = ds->mat[DS_MAT_Y];
  } else {
    A = ds->mat[DS_MAT_A];
    Q = ds->mat[DS_MAT_Q];
    X = ds->mat[DS_MAT_X];
  }
  if (ds->state>=DS_STATE_CONDENSED) {
    /* DSSolve() has been called, backtransform with matrix Q */
    back = "B";
    CHKERRQ(PetscArraycpy(X,Q,ld*ld));
  } else back = "A";
#if !defined(PETSC_USE_COMPLEX)
  CHKERRQ(DSAllocateWork_Private(ds,3*ld,0,0));
  PetscStackCallBLAS("LAPACKtrevc",LAPACKtrevc_("R",back,NULL,&n,A,&ld,X,&ld,X,&ld,&n,&mout,ds->work,&info));
#else
  CHKERRQ(DSAllocateWork_Private(ds,2*ld,ld,0));
  PetscStackCallBLAS("LAPACKtrevc",LAPACKtrevc_("R",back,NULL,&n,A,&ld,X,&ld,X,&ld,&n,&mout,ds->work,ds->rwork,&info));
#endif
  SlepcCheckLapackInfo("trevc",info);

  /* normalize eigenvectors */
  for (i=0;i<n;i++) {
    iscomplex = (i<n-1 && A[i+1+i*ld]!=0.0)? PETSC_TRUE: PETSC_FALSE;
    cols = 1;
    norm = BLASnrm2_(&n,X+i*ld,&inc);
#if !defined(PETSC_USE_COMPLEX)
    if (iscomplex) {
      norm = SlepcAbsEigenvalue(norm,BLASnrm2_(&n,X+(i+1)*ld,&inc));
      cols = 2;
    }
#endif
    PetscStackCallBLAS("LAPACKlascl",LAPACKlascl_("G",&zero,&zero,&norm,&done,&n,&cols,X+i*ld,&ld,&info));
    SlepcCheckLapackInfo("lascl",info);
    if (iscomplex) i++;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSVectors_NHEPTS(DS ds,DSMatType mat,PetscInt *j,PetscReal *rnorm)
{
  PetscFunctionBegin;
  switch (mat) {
    case DS_MAT_X:
      PetscCheck(!ds->refined,PetscObjectComm((PetscObject)ds),PETSC_ERR_SUP,"Not implemented yet");
      else {
        if (j) {
          CHKERRQ(DSVectors_NHEPTS_Eigen_Some(ds,j,rnorm,PETSC_FALSE));
        } else {
          CHKERRQ(DSVectors_NHEPTS_Eigen_All(ds,PETSC_FALSE));
        }
      }
      break;
    case DS_MAT_Y:
      PetscCheck(!ds->refined,PetscObjectComm((PetscObject)ds),PETSC_ERR_SUP,"Not implemented yet");
      if (j) {
        CHKERRQ(DSVectors_NHEPTS_Eigen_Some(ds,j,rnorm,PETSC_TRUE));
      } else {
        CHKERRQ(DSVectors_NHEPTS_Eigen_All(ds,PETSC_TRUE));
      }
      break;
    case DS_MAT_U:
    case DS_MAT_V:
      SETERRQ(PetscObjectComm((PetscObject)ds),PETSC_ERR_SUP,"Not implemented yet");
    default:
      SETERRQ(PetscObjectComm((PetscObject)ds),PETSC_ERR_ARG_OUTOFRANGE,"Invalid mat parameter");
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSSort_NHEPTS(DS ds,PetscScalar *wr,PetscScalar *wi,PetscScalar *rr,PetscScalar *ri,PetscInt *k)
{
  DS_NHEPTS      *ctx = (DS_NHEPTS*)ds->data;
  PetscInt       i,j,cont,id=0,*p,*idx,*idx2;
  PetscReal      s,t;
#if defined(PETSC_USE_COMPLEX)
  Mat            A,U;
#endif

  PetscFunctionBegin;
  PetscCheck(!rr || wr==rr,PetscObjectComm((PetscObject)ds),PETSC_ERR_SUP,"Not implemented yet");
  CHKERRQ(PetscMalloc3(ds->ld,&idx,ds->ld,&idx2,ds->ld,&p));
  CHKERRQ(DSSort_NHEP_Total(ds,ds->mat[DS_MAT_A],ds->mat[DS_MAT_Q],wr,wi));
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(DSGetMat(ds,DS_MAT_B,&A));
  CHKERRQ(MatConjugate(A));
  CHKERRQ(DSRestoreMat(ds,DS_MAT_B,&A));
  CHKERRQ(DSGetMat(ds,DS_MAT_Z,&U));
  CHKERRQ(MatConjugate(U));
  CHKERRQ(DSRestoreMat(ds,DS_MAT_Z,&U));
  for (i=0;i<ds->n;i++) ctx->wr[i] = PetscConj(ctx->wr[i]);
#endif
  CHKERRQ(DSSort_NHEP_Total(ds,ds->mat[DS_MAT_B],ds->mat[DS_MAT_Z],ctx->wr,ctx->wi));
  /* check correct eigenvalue correspondence */
  cont = 0;
  for (i=0;i<ds->n;i++) {
    if (SlepcAbsEigenvalue(ctx->wr[i]-wr[i],ctx->wi[i]-wi[i])>PETSC_SQRT_MACHINE_EPSILON) {idx2[cont] = i; idx[cont++] = i;}
    p[i] = -1;
  }
  if (cont) {
    for (i=0;i<cont;i++) {
      t = PETSC_MAX_REAL;
      for (j=0;j<cont;j++) if (idx2[j]!=-1 && (s=SlepcAbsEigenvalue(ctx->wr[idx[j]]-wr[idx[i]],ctx->wi[idx[j]]-wi[idx[i]]))<t) { id = j; t = s; }
      p[idx[i]] = idx[id];
      idx2[id] = -1;
    }
    for (i=0;i<ds->n;i++) if (p[i]==-1) p[i] = i;
    CHKERRQ(DSSortWithPermutation_NHEP_Private(ds,p,ds->mat[DS_MAT_B],ds->mat[DS_MAT_Z],ctx->wr,ctx->wi));
  }
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(DSGetMat(ds,DS_MAT_B,&A));
  CHKERRQ(MatConjugate(A));
  CHKERRQ(DSRestoreMat(ds,DS_MAT_B,&A));
  CHKERRQ(DSGetMat(ds,DS_MAT_Z,&U));
  CHKERRQ(MatConjugate(U));
  CHKERRQ(DSRestoreMat(ds,DS_MAT_Z,&U));
#endif
  CHKERRQ(PetscFree3(idx,idx2,p));
  PetscFunctionReturn(0);
}

PetscErrorCode DSUpdateExtraRow_NHEPTS(DS ds)
{
  PetscInt       i;
  PetscBLASInt   n,ld,incx=1;
  PetscScalar    *A,*Q,*x,*y,one=1.0,zero=0.0;

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  CHKERRQ(DSAllocateWork_Private(ds,2*ld,0,0));
  x = ds->work;
  y = ds->work+ld;
  A = ds->mat[DS_MAT_A];
  Q = ds->mat[DS_MAT_Q];
  for (i=0;i<n;i++) x[i] = PetscConj(A[n+i*ld]);
  PetscStackCallBLAS("BLASgemv",BLASgemv_("C",&n,&n,&one,Q,&ld,x,&incx,&zero,y,&incx));
  for (i=0;i<n;i++) A[n+i*ld] = PetscConj(y[i]);
  A = ds->mat[DS_MAT_B];
  Q = ds->mat[DS_MAT_Z];
  for (i=0;i<n;i++) x[i] = PetscConj(A[n+i*ld]);
  PetscStackCallBLAS("BLASgemv",BLASgemv_("C",&n,&n,&one,Q,&ld,x,&incx,&zero,y,&incx));
  for (i=0;i<n;i++) A[n+i*ld] = PetscConj(y[i]);
  ds->k = n;
  PetscFunctionReturn(0);
}

PetscErrorCode DSSolve_NHEPTS(DS ds,PetscScalar *wr,PetscScalar *wi)
{
  DS_NHEPTS      *ctx = (DS_NHEPTS*)ds->data;

  PetscFunctionBegin;
#if !defined(PETSC_USE_COMPLEX)
  PetscValidScalarPointer(wi,3);
#endif
  CHKERRQ(DSSolve_NHEP_Private(ds,ds->mat[DS_MAT_A],ds->mat[DS_MAT_Q],wr,wi));
  CHKERRQ(DSSolve_NHEP_Private(ds,ds->mat[DS_MAT_B],ds->mat[DS_MAT_Z],ctx->wr,ctx->wi));
  PetscFunctionReturn(0);
}

PetscErrorCode DSSynchronize_NHEPTS(DS ds,PetscScalar eigr[],PetscScalar eigi[])
{
  PetscInt       ld=ds->ld,l=ds->l,k;
  PetscMPIInt    n,rank,off=0,size,ldn;
  DS_NHEPTS      *ctx = (DS_NHEPTS*)ds->data;

  PetscFunctionBegin;
  k = 2*(ds->n-l)*ld;
  if (ds->state>DS_STATE_RAW) k += 2*(ds->n-l)*ld;
  if (eigr) k += ds->n-l;
  if (eigi) k += ds->n-l;
  if (ctx->wr) k += ds->n-l;
  if (ctx->wi) k += ds->n-l;
  CHKERRQ(DSAllocateWork_Private(ds,k,0,0));
  CHKERRQ(PetscMPIIntCast(k*sizeof(PetscScalar),&size));
  CHKERRQ(PetscMPIIntCast(ds->n-l,&n));
  CHKERRQ(PetscMPIIntCast(ld*(ds->n-l),&ldn));
  CHKERRMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)ds),&rank));
  if (!rank) {
    CHKERRMPI(MPI_Pack(ds->mat[DS_MAT_A]+l*ld,ldn,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    CHKERRMPI(MPI_Pack(ds->mat[DS_MAT_B]+l*ld,ldn,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    if (ds->state>DS_STATE_RAW) {
      CHKERRMPI(MPI_Pack(ds->mat[DS_MAT_Q]+l*ld,ldn,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
      CHKERRMPI(MPI_Pack(ds->mat[DS_MAT_Z]+l*ld,ldn,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    }
    if (eigr) {
      CHKERRMPI(MPI_Pack(eigr+l,n,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    }
#if !defined(PETSC_USE_COMPLEX)
    if (eigi) {
      CHKERRMPI(MPI_Pack(eigi+l,n,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    }
#endif
    if (ctx->wr) {
      CHKERRMPI(MPI_Pack(ctx->wr+l,n,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    }
    if (ctx->wi) {
      CHKERRMPI(MPI_Pack(ctx->wi+l,n,MPIU_SCALAR,ds->work,size,&off,PetscObjectComm((PetscObject)ds)));
    }
  }
  CHKERRMPI(MPI_Bcast(ds->work,size,MPI_BYTE,0,PetscObjectComm((PetscObject)ds)));
  if (rank) {
    CHKERRMPI(MPI_Unpack(ds->work,size,&off,ds->mat[DS_MAT_A]+l*ld,ldn,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    CHKERRMPI(MPI_Unpack(ds->work,size,&off,ds->mat[DS_MAT_B]+l*ld,ldn,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    if (ds->state>DS_STATE_RAW) {
      CHKERRMPI(MPI_Unpack(ds->work,size,&off,ds->mat[DS_MAT_Q]+l*ld,ldn,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
      CHKERRMPI(MPI_Unpack(ds->work,size,&off,ds->mat[DS_MAT_Z]+l*ld,ldn,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    }
    if (eigr) {
      CHKERRMPI(MPI_Unpack(ds->work,size,&off,eigr+l,n,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    }
#if !defined(PETSC_USE_COMPLEX)
    if (eigi) {
      CHKERRMPI(MPI_Unpack(ds->work,size,&off,eigi+l,n,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    }
#endif
    if (ctx->wr) {
      CHKERRMPI(MPI_Unpack(ds->work,size,&off,ctx->wr+l,n,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    }
    if (ctx->wi) {
      CHKERRMPI(MPI_Unpack(ds->work,size,&off,ctx->wi+l,n,MPIU_SCALAR,PetscObjectComm((PetscObject)ds)));
    }
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSGetTruncateSize_NHEPTS(DS ds,PetscInt l,PetscInt n,PetscInt *k)
{
#if !defined(PETSC_USE_COMPLEX)
  PetscScalar *A = ds->mat[DS_MAT_A],*B = ds->mat[DS_MAT_B];
#endif

  PetscFunctionBegin;
#if !defined(PETSC_USE_COMPLEX)
  if (A[l+(*k)+(l+(*k)-1)*ds->ld] != 0.0 || B[l+(*k)+(l+(*k)-1)*ds->ld] != 0.0) {
    if (l+(*k)<n-1) (*k)++;
    else (*k)--;
  }
#endif
  PetscFunctionReturn(0);
}

PetscErrorCode DSTruncate_NHEPTS(DS ds,PetscInt n,PetscBool trim)
{
  PetscInt    i,ld=ds->ld,l=ds->l;
  PetscScalar *A = ds->mat[DS_MAT_A],*B = ds->mat[DS_MAT_B];

  PetscFunctionBegin;
#if defined(PETSC_USE_DEBUG)
  /* make sure diagonal 2x2 block is not broken */
  PetscCheck(ds->state<DS_STATE_CONDENSED || n==0 || n==ds->n || A[n+(n-1)*ld]==0.0 || B[n+(n-1)*ld]==0.0,PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"The given size would break a 2x2 block, call DSGetTruncateSize() first");
#endif
  if (trim) {
    if (ds->extrarow) {   /* clean extra row */
      for (i=l;i<ds->n;i++) { A[ds->n+i*ld] = 0.0; B[ds->n+i*ld] = 0.0; }
    }
    ds->l = 0;
    ds->k = 0;
    ds->n = n;
    ds->t = ds->n;   /* truncated length equal to the new dimension */
  } else {
    if (ds->extrarow && ds->k==ds->n) {
      /* copy entries of extra row to the new position, then clean last row */
      for (i=l;i<n;i++) { A[n+i*ld] = A[ds->n+i*ld]; B[n+i*ld] = B[ds->n+i*ld]; }
      for (i=l;i<ds->n;i++) { A[ds->n+i*ld] = 0.0; B[ds->n+i*ld] = 0.0; }
    }
    ds->k = (ds->extrarow)? n: 0;
    ds->t = ds->n;   /* truncated length equal to previous dimension */
    ds->n = n;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSDestroy_NHEPTS(DS ds)
{
  DS_NHEPTS      *ctx = (DS_NHEPTS*)ds->data;

  PetscFunctionBegin;
  if (ctx->wr) CHKERRQ(PetscFree(ctx->wr));
  if (ctx->wi) CHKERRQ(PetscFree(ctx->wi));
  CHKERRQ(PetscFree(ds->data));
  PetscFunctionReturn(0);
}

PetscErrorCode DSMatGetSize_NHEPTS(DS ds,DSMatType t,PetscInt *rows,PetscInt *cols)
{
  PetscFunctionBegin;
  *rows = ((t==DS_MAT_A || t==DS_MAT_B) && ds->extrarow)? ds->n+1: ds->n;
  *cols = ds->n;
  PetscFunctionReturn(0);
}

/*MC
   DSNHEPTS - Dense Non-Hermitian Eigenvalue Problem (special variant intended
   for two-sided Krylov solvers).

   Level: beginner

   Notes:
   Two related problems are solved, A*X = X*Lambda and B*Y = Y*Lambda', where A and
   B are supposed to come from the Arnoldi factorizations of a certain matrix and its
   (conjugate) transpose, respectively. Hence, in exact arithmetic the columns of Y
   are equal to the left eigenvectors of A. Lambda is a diagonal matrix whose diagonal
   elements are the arguments of DSSolve(). After solve, A is overwritten with the
   upper quasi-triangular matrix T of the (real) Schur form, A*Q = Q*T, and similarly
   another (real) Schur relation is computed, B*Z = Z*S, overwriting B.

   In the intermediate state A and B are reduced to upper Hessenberg form.

   When left eigenvectors DS_MAT_Y are requested, right eigenvectors of B are returned,
   while DS_MAT_X contains right eigenvectors of A.

   Used DS matrices:
+  DS_MAT_A - first problem matrix obtained from Arnoldi
.  DS_MAT_B - second problem matrix obtained from Arnoldi on the transpose
.  DS_MAT_Q - orthogonal/unitary transformation that reduces A to Hessenberg form
   (intermediate step) or matrix of orthogonal Schur vectors of A
-  DS_MAT_Z - orthogonal/unitary transformation that reduces B to Hessenberg form
   (intermediate step) or matrix of orthogonal Schur vectors of B

   Implemented methods:
.  0 - Implicit QR (_hseqr)

.seealso: DSCreate(), DSSetType(), DSType
M*/
SLEPC_EXTERN PetscErrorCode DSCreate_NHEPTS(DS ds)
{
  DS_NHEPTS      *ctx;

  PetscFunctionBegin;
  CHKERRQ(PetscNewLog(ds,&ctx));
  ds->data = (void*)ctx;

  ds->ops->allocate        = DSAllocate_NHEPTS;
  ds->ops->view            = DSView_NHEPTS;
  ds->ops->vectors         = DSVectors_NHEPTS;
  ds->ops->solve[0]        = DSSolve_NHEPTS;
  ds->ops->sort            = DSSort_NHEPTS;
  ds->ops->synchronize     = DSSynchronize_NHEPTS;
  ds->ops->gettruncatesize = DSGetTruncateSize_NHEPTS;
  ds->ops->truncate        = DSTruncate_NHEPTS;
  ds->ops->update          = DSUpdateExtraRow_NHEPTS;
  ds->ops->destroy         = DSDestroy_NHEPTS;
  ds->ops->matgetsize      = DSMatGetSize_NHEPTS;
  PetscFunctionReturn(0);
}
