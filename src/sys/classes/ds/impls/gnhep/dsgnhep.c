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

/*
  1) Patterns of A and B
      DS_STATE_RAW:       DS_STATE_INTERM/CONDENSED
       0       n-1              0       n-1
      -------------            -------------
    0 |* * * * * *|          0 |* * * * * *|
      |* * * * * *|            |  * * * * *|
      |* * * * * *|            |    * * * *|
      |* * * * * *|            |    * * * *|
      |* * * * * *|            |        * *|
  n-1 |* * * * * *|        n-1 |          *|
      -------------            -------------

  2) Moreover, P and Q are assumed to be the identity in DS_STATE_INTERMEDIATE.
*/

static PetscErrorCode CleanDenseSchur(PetscInt n,PetscInt k,PetscScalar *S,PetscInt ldS,PetscScalar *T,PetscInt ldT,PetscScalar *X,PetscInt ldX,PetscScalar *Y,PetscInt ldY);

PetscErrorCode DSAllocate_GNHEP(DS ds,PetscInt ld)
{
  PetscFunctionBegin;
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_A));
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_B));
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_Z));
  CHKERRQ(DSAllocateMat_Private(ds,DS_MAT_Q));
  CHKERRQ(PetscFree(ds->perm));
  CHKERRQ(PetscMalloc1(ld,&ds->perm));
  CHKERRQ(PetscLogObjectMemory((PetscObject)ds,ld*sizeof(PetscInt)));
  PetscFunctionReturn(0);
}

PetscErrorCode DSView_GNHEP(DS ds,PetscViewer viewer)
{
  PetscViewerFormat format;

  PetscFunctionBegin;
  CHKERRQ(PetscViewerGetFormat(viewer,&format));
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) PetscFunctionReturn(0);
  CHKERRQ(DSViewMat(ds,viewer,DS_MAT_A));
  CHKERRQ(DSViewMat(ds,viewer,DS_MAT_B));
  if (ds->state>DS_STATE_INTERMEDIATE) {
    CHKERRQ(DSViewMat(ds,viewer,DS_MAT_Z));
    CHKERRQ(DSViewMat(ds,viewer,DS_MAT_Q));
  }
  if (ds->mat[DS_MAT_X]) CHKERRQ(DSViewMat(ds,viewer,DS_MAT_X));
  if (ds->mat[DS_MAT_Y]) CHKERRQ(DSViewMat(ds,viewer,DS_MAT_Y));
  PetscFunctionReturn(0);
}

static PetscErrorCode DSVectors_GNHEP_Eigen_Some(DS ds,PetscInt *k,PetscReal *rnorm,PetscBool left)
{
  PetscInt       i;
  PetscBLASInt   n,ld,mout,info,*select,mm,inc=1,cols=1,zero=0;
  PetscScalar    *X,*Y,*Z,*A = ds->mat[DS_MAT_A],*B = ds->mat[DS_MAT_B],fone=1.0,fzero=0.0;
  PetscReal      norm,done=1.0;
  PetscBool      iscomplex = PETSC_FALSE;
  const char     *side;

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  if (left) {
    X = NULL;
    Y = &ds->mat[DS_MAT_Y][ld*(*k)];
    side = "L";
  } else {
    X = &ds->mat[DS_MAT_X][ld*(*k)];
    Y = NULL;
    side = "R";
  }
  Z = left? Y: X;
  CHKERRQ(DSAllocateWork_Private(ds,0,0,ld));
  select = ds->iwork;
  for (i=0;i<n;i++) select[i] = (PetscBLASInt)PETSC_FALSE;
  if (ds->state <= DS_STATE_INTERMEDIATE) {
    CHKERRQ(DSSetIdentity(ds,DS_MAT_Q));
    CHKERRQ(DSSetIdentity(ds,DS_MAT_Z));
  }
  CHKERRQ(CleanDenseSchur(n,0,A,ld,B,ld,ds->mat[DS_MAT_Q],ld,ds->mat[DS_MAT_Z],ld));
  if (ds->state < DS_STATE_CONDENSED) CHKERRQ(DSSetState(ds,DS_STATE_CONDENSED));

  /* compute k-th eigenvector */
  select[*k] = (PetscBLASInt)PETSC_TRUE;
#if defined(PETSC_USE_COMPLEX)
  mm = 1;
  CHKERRQ(DSAllocateWork_Private(ds,2*ld,2*ld,0));
  PetscStackCallBLAS("LAPACKtgevc",LAPACKtgevc_(side,"S",select,&n,A,&ld,B,&ld,Y,&ld,X,&ld,&mm,&mout,ds->work,ds->rwork,&info));
#else
  if ((*k)<n-1 && (A[ld*(*k)+(*k)+1] != 0.0 || B[ld*(*k)+(*k)+1] != 0.0)) iscomplex = PETSC_TRUE;
  mm = iscomplex? 2: 1;
  if (iscomplex) select[(*k)+1] = (PetscBLASInt)PETSC_TRUE;
  CHKERRQ(DSAllocateWork_Private(ds,6*ld,0,0));
  PetscStackCallBLAS("LAPACKtgevc",LAPACKtgevc_(side,"S",select,&n,A,&ld,B,&ld,Y,&ld,X,&ld,&mm,&mout,ds->work,&info));
#endif
  SlepcCheckLapackInfo("tgevc",info);
  PetscCheck(select[*k] && mout==mm,PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Wrong arguments in call to Lapack xTGEVC");

  /* accumulate and normalize eigenvectors */
  CHKERRQ(PetscArraycpy(ds->work,Z,mm*ld));
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&mm,&n,&fone,ds->mat[left?DS_MAT_Z:DS_MAT_Q],&ld,ds->work,&ld,&fzero,Z,&ld));
  norm = BLASnrm2_(&n,Z,&inc);
#if !defined(PETSC_USE_COMPLEX)
  if (iscomplex) {
    norm = SlepcAbsEigenvalue(norm,BLASnrm2_(&n,Z+ld,&inc));
    cols = 2;
  }
#endif
  PetscStackCallBLAS("LAPACKlascl",LAPACKlascl_("G",&zero,&zero,&norm,&done,&n,&cols,Z,&ld,&info));
  SlepcCheckLapackInfo("lascl",info);

  /* set output arguments */
  if (iscomplex) (*k)++;
  if (rnorm) {
    if (iscomplex) *rnorm = SlepcAbsEigenvalue(Z[n-1],Z[n-1+ld]);
    else *rnorm = PetscAbsScalar(Z[n-1]);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DSVectors_GNHEP_Eigen_All(DS ds,PetscBool left)
{
  PetscInt       i;
  PetscBLASInt   n,ld,mout,info,inc = 1;
  PetscBool      iscomplex;
  PetscScalar    *X,*Y,*Z,*A = ds->mat[DS_MAT_A],*B = ds->mat[DS_MAT_B],tmp;
  PetscReal      norm;
  const char     *side,*back;

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  if (left) {
    X = NULL;
    Y = ds->mat[DS_MAT_Y];
    side = "L";
  } else {
    X = ds->mat[DS_MAT_X];
    Y = NULL;
    side = "R";
  }
  Z = left? Y: X;
  if (ds->state <= DS_STATE_INTERMEDIATE) {
    CHKERRQ(DSSetIdentity(ds,DS_MAT_Q));
    CHKERRQ(DSSetIdentity(ds,DS_MAT_Z));
  }
  CHKERRQ(CleanDenseSchur(n,0,A,ld,B,ld,ds->mat[DS_MAT_Q],ld,ds->mat[DS_MAT_Z],ld));
  if (ds->state>=DS_STATE_CONDENSED) {
    /* DSSolve() has been called, backtransform with matrix Q */
    back = "B";
    CHKERRQ(PetscArraycpy(left?Y:X,ds->mat[left?DS_MAT_Z:DS_MAT_Q],ld*ld));
  } else {
    back = "A";
    CHKERRQ(DSSetState(ds,DS_STATE_CONDENSED));
  }
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(DSAllocateWork_Private(ds,2*ld,2*ld,0));
  PetscStackCallBLAS("LAPACKtgevc",LAPACKtgevc_(side,back,NULL,&n,A,&ld,B,&ld,Y,&ld,X,&ld,&n,&mout,ds->work,ds->rwork,&info));
#else
  CHKERRQ(DSAllocateWork_Private(ds,6*ld,0,0));
  PetscStackCallBLAS("LAPACKtgevc",LAPACKtgevc_(side,back,NULL,&n,A,&ld,B,&ld,Y,&ld,X,&ld,&n,&mout,ds->work,&info));
#endif
  SlepcCheckLapackInfo("tgevc",info);

  /* normalize eigenvectors */
  for (i=0;i<n;i++) {
    iscomplex = (i<n-1 && (A[i+1+i*ld]!=0.0 || B[i+1+i*ld]!=0.0))? PETSC_TRUE: PETSC_FALSE;
    norm = BLASnrm2_(&n,Z+i*ld,&inc);
#if !defined(PETSC_USE_COMPLEX)
    if (iscomplex) {
      tmp = BLASnrm2_(&n,Z+(i+1)*ld,&inc);
      norm = SlepcAbsEigenvalue(norm,tmp);
    }
#endif
    tmp = 1.0 / norm;
    PetscStackCallBLAS("BLASscal",BLASscal_(&n,&tmp,Z+i*ld,&inc));
#if !defined(PETSC_USE_COMPLEX)
    if (iscomplex) PetscStackCallBLAS("BLASscal",BLASscal_(&n,&tmp,Z+(i+1)*ld,&inc));
#endif
    if (iscomplex) i++;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSVectors_GNHEP(DS ds,DSMatType mat,PetscInt *k,PetscReal *rnorm)
{
  PetscFunctionBegin;
  switch (mat) {
    case DS_MAT_X:
    case DS_MAT_Y:
      if (k) {
        CHKERRQ(DSVectors_GNHEP_Eigen_Some(ds,k,rnorm,mat == DS_MAT_Y?PETSC_TRUE:PETSC_FALSE));
      } else {
        CHKERRQ(DSVectors_GNHEP_Eigen_All(ds,mat == DS_MAT_Y?PETSC_TRUE:PETSC_FALSE));
      }
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)ds),PETSC_ERR_ARG_OUTOFRANGE,"Invalid mat parameter");
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DSSort_GNHEP_Arbitrary(DS ds,PetscScalar *wr,PetscScalar *wi,PetscScalar *rr,PetscScalar *ri,PetscInt *k)
{
  PetscInt       i;
  PetscBLASInt   info,n,ld,mout,lwork,liwork,*iwork,*selection,zero_=0,true_=1;
  PetscScalar    *S = ds->mat[DS_MAT_A],*T = ds->mat[DS_MAT_B],*Q = ds->mat[DS_MAT_Q],*Z = ds->mat[DS_MAT_Z],*work,*beta;

  PetscFunctionBegin;
  if (!ds->sc) PetscFunctionReturn(0);
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
#if !defined(PETSC_USE_COMPLEX)
  lwork = 4*n+16;
#else
  lwork = 1;
#endif
  liwork = 1;
  CHKERRQ(DSAllocateWork_Private(ds,lwork+2*n,0,liwork+n));
  beta      = ds->work;
  work      = ds->work + n;
  lwork     = ds->lwork - n;
  selection = ds->iwork;
  iwork     = ds->iwork + n;
  liwork    = ds->liwork - n;
  /* Compute the selected eigenvalue to be in the leading position */
  CHKERRQ(DSSortEigenvalues_Private(ds,rr,ri,ds->perm,PETSC_FALSE));
  CHKERRQ(PetscArrayzero(selection,n));
  for (i=0; i<*k; i++) selection[ds->perm[i]] = 1;
#if !defined(PETSC_USE_COMPLEX)
  PetscStackCallBLAS("LAPACKtgsen",LAPACKtgsen_(&zero_,&true_,&true_,selection,&n,S,&ld,T,&ld,wr,wi,beta,Z,&ld,Q,&ld,&mout,NULL,NULL,NULL,work,&lwork,iwork,&liwork,&info));
#else
  PetscStackCallBLAS("LAPACKtgsen",LAPACKtgsen_(&zero_,&true_,&true_,selection,&n,S,&ld,T,&ld,wr,beta,Z,&ld,Q,&ld,&mout,NULL,NULL,NULL,work,&lwork,iwork,&liwork,&info));
#endif
  SlepcCheckLapackInfo("tgsen",info);
  *k = mout;
  for (i=0;i<n;i++) {
    if (beta[i]==0.0) wr[i] = (PetscRealPart(wr[i])>0.0)? PETSC_MAX_REAL: PETSC_MIN_REAL;
    else wr[i] /= beta[i];
#if !defined(PETSC_USE_COMPLEX)
    if (beta[i]==0.0) wi[i] = (wi[i]>0.0)? PETSC_MAX_REAL: PETSC_MIN_REAL;
    else wi[i] /= beta[i];
#endif
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DSSort_GNHEP_Total(DS ds,PetscScalar *wr,PetscScalar *wi)
{
  PetscScalar    re;
  PetscInt       i,j,pos,result;
  PetscBLASInt   ifst,ilst,info,n,ld,one=1;
  PetscScalar    *S = ds->mat[DS_MAT_A],*T = ds->mat[DS_MAT_B],*Z = ds->mat[DS_MAT_Z],*Q = ds->mat[DS_MAT_Q];
#if !defined(PETSC_USE_COMPLEX)
  PetscBLASInt   lwork;
  PetscScalar    *work,a,safmin,scale1,scale2,im;
#endif

  PetscFunctionBegin;
  if (!ds->sc) PetscFunctionReturn(0);
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
#if !defined(PETSC_USE_COMPLEX)
  lwork = -1;
  PetscStackCallBLAS("LAPACKtgexc",LAPACKtgexc_(&one,&one,&ld,NULL,&ld,NULL,&ld,NULL,&ld,NULL,&ld,&one,&one,&a,&lwork,&info));
  SlepcCheckLapackInfo("tgexc",info);
  safmin = LAPACKlamch_("S");
  CHKERRQ(PetscBLASIntCast((PetscInt)a,&lwork));
  CHKERRQ(DSAllocateWork_Private(ds,lwork,0,0));
  work = ds->work;
#endif
  /* selection sort */
  for (i=ds->l;i<n-1;i++) {
    re = wr[i];
#if !defined(PETSC_USE_COMPLEX)
    im = wi[i];
#endif
    pos = 0;
    j = i+1; /* j points to the next eigenvalue */
#if !defined(PETSC_USE_COMPLEX)
    if (im != 0) j=i+2;
#endif
    /* find minimum eigenvalue */
    for (;j<n;j++) {
#if !defined(PETSC_USE_COMPLEX)
      CHKERRQ(SlepcSCCompare(ds->sc,re,im,wr[j],wi[j],&result));
#else
      CHKERRQ(SlepcSCCompare(ds->sc,re,0.0,wr[j],0.0,&result));
#endif
      if (result > 0) {
        re = wr[j];
#if !defined(PETSC_USE_COMPLEX)
        im = wi[j];
#endif
        pos = j;
      }
#if !defined(PETSC_USE_COMPLEX)
      if (wi[j] != 0) j++;
#endif
    }
    if (pos) {
      /* interchange blocks */
      CHKERRQ(PetscBLASIntCast(pos+1,&ifst));
      CHKERRQ(PetscBLASIntCast(i+1,&ilst));
#if !defined(PETSC_USE_COMPLEX)
      PetscStackCallBLAS("LAPACKtgexc",LAPACKtgexc_(&one,&one,&n,S,&ld,T,&ld,Z,&ld,Q,&ld,&ifst,&ilst,work,&lwork,&info));
#else
      PetscStackCallBLAS("LAPACKtgexc",LAPACKtgexc_(&one,&one,&n,S,&ld,T,&ld,Z,&ld,Q,&ld,&ifst,&ilst,&info));
#endif
      SlepcCheckLapackInfo("tgexc",info);
      /* recover original eigenvalues from T and S matrices */
      for (j=i;j<n;j++) {
#if !defined(PETSC_USE_COMPLEX)
        if (j<n-1 && S[j*ld+j+1] != 0.0) {
          /* complex conjugate eigenvalue */
          PetscStackCallBLAS("LAPACKlag2",LAPACKlag2_(S+j*ld+j,&ld,T+j*ld+j,&ld,&safmin,&scale1,&scale2,&re,&a,&im));
          wr[j] = re / scale1;
          wi[j] = im / scale1;
          wr[j+1] = a / scale2;
          wi[j+1] = -wi[j];
          j++;
        } else
#endif
        {
          if (T[j*ld+j] == 0.0) wr[j] = (PetscRealPart(S[j*ld+j])>0.0)? PETSC_MAX_REAL: PETSC_MIN_REAL;
          else wr[j] = S[j*ld+j] / T[j*ld+j];
#if !defined(PETSC_USE_COMPLEX)
          wi[j] = 0.0;
#endif
        }
      }
    }
#if !defined(PETSC_USE_COMPLEX)
    if (wi[i] != 0.0) i++;
#endif
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSSort_GNHEP(DS ds,PetscScalar *wr,PetscScalar *wi,PetscScalar *rr,PetscScalar *ri,PetscInt *k)
{
  PetscFunctionBegin;
  if (!rr || wr == rr) {
    CHKERRQ(DSSort_GNHEP_Total(ds,wr,wi));
  } else {
    CHKERRQ(DSSort_GNHEP_Arbitrary(ds,wr,wi,rr,ri,k));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSUpdateExtraRow_GNHEP(DS ds)
{
  PetscInt       i;
  PetscBLASInt   n,ld,incx=1;
  PetscScalar    *A,*B,*Q,*x,*y,one=1.0,zero=0.0;

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  A  = ds->mat[DS_MAT_A];
  B  = ds->mat[DS_MAT_B];
  Q  = ds->mat[DS_MAT_Q];
  CHKERRQ(DSAllocateWork_Private(ds,2*ld,0,0));
  x = ds->work;
  y = ds->work+ld;
  for (i=0;i<n;i++) x[i] = PetscConj(A[n+i*ld]);
  PetscStackCallBLAS("BLASgemv",BLASgemv_("C",&n,&n,&one,Q,&ld,x,&incx,&zero,y,&incx));
  for (i=0;i<n;i++) A[n+i*ld] = PetscConj(y[i]);
  for (i=0;i<n;i++) x[i] = PetscConj(B[n+i*ld]);
  PetscStackCallBLAS("BLASgemv",BLASgemv_("C",&n,&n,&one,Q,&ld,x,&incx,&zero,y,&incx));
  for (i=0;i<n;i++) B[n+i*ld] = PetscConj(y[i]);
  ds->k = n;
  PetscFunctionReturn(0);
}

/*
   Write zeros from the column k to n in the lower triangular part of the
   matrices S and T, and inside 2-by-2 diagonal blocks of T in order to
   make (S,T) a valid Schur decompositon.
*/
static PetscErrorCode CleanDenseSchur(PetscInt n,PetscInt k,PetscScalar *S,PetscInt ldS,PetscScalar *T,PetscInt ldT,PetscScalar *X,PetscInt ldX,PetscScalar *Y,PetscInt ldY)
{
  PetscInt       i;
#if defined(PETSC_USE_COMPLEX)
  PetscInt       j;
  PetscScalar    s;
#else
  PetscBLASInt   ldS_,ldT_,n_i,n_i_2,one=1,n_,i_2,i_;
  PetscScalar    b11,b22,sr,cr,sl,cl;
#endif

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  for (i=k; i<n; i++) {
    /* Some functions need the diagonal elements in T be real */
    if (T && PetscImaginaryPart(T[ldT*i+i]) != 0.0) {
      s = PetscConj(T[ldT*i+i])/PetscAbsScalar(T[ldT*i+i]);
      for (j=0;j<=i;j++) {
        T[ldT*i+j] *= s;
        S[ldS*i+j] *= s;
      }
      T[ldT*i+i] = PetscRealPart(T[ldT*i+i]);
      if (X) for (j=0;j<n;j++) X[ldX*i+j] *= s;
    }
    j = i+1;
    if (j<n) {
      S[ldS*i+j] = 0.0;
      if (T) T[ldT*i+j] = 0.0;
    }
  }
#else
  CHKERRQ(PetscBLASIntCast(ldS,&ldS_));
  CHKERRQ(PetscBLASIntCast(ldT,&ldT_));
  CHKERRQ(PetscBLASIntCast(n,&n_));
  for (i=k;i<n-1;i++) {
    if (S[ldS*i+i+1] != 0.0) {
      /* Check if T(i+1,i) and T(i,i+1) are zero */
      if (T[ldT*(i+1)+i] != 0.0 || T[ldT*i+i+1] != 0.0) {
        /* Check if T(i+1,i) and T(i,i+1) are negligible */
        if (PetscAbs(T[ldT*(i+1)+i])+PetscAbs(T[ldT*i+i+1]) < (PetscAbs(T[ldT*i+i])+PetscAbs(T[ldT*(i+1)+i+1]))*PETSC_MACHINE_EPSILON) {
          T[ldT*i+i+1] = 0.0;
          T[ldT*(i+1)+i] = 0.0;
        } else {
          /* If one of T(i+1,i) or T(i,i+1) is negligible, we make zero the other element */
          if (PetscAbs(T[ldT*i+i+1]) < (PetscAbs(T[ldT*i+i])+PetscAbs(T[ldT*(i+1)+i+1])+PetscAbs(T[ldT*(i+1)+i]))*PETSC_MACHINE_EPSILON) {
            PetscStackCallBLAS("LAPACKlasv2",LAPACKlasv2_(&T[ldT*i+i],&T[ldT*(i+1)+i],&T[ldT*(i+1)+i+1],&b22,&b11,&sl,&cl,&sr,&cr));
          } else if (PetscAbs(T[ldT*(i+1)+i]) < (PetscAbs(T[ldT*i+i])+PetscAbs(T[ldT*(i+1)+i+1])+PetscAbs(T[ldT*i+i+1]))*PETSC_MACHINE_EPSILON) {
            PetscStackCallBLAS("LAPACKlasv2",LAPACKlasv2_(&T[ldT*i+i],&T[ldT*i+i+1],&T[ldT*(i+1)+i+1],&b22,&b11,&sr,&cr,&sl,&cl));
          } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"Unsupported format. Call DSSolve before this function");
          CHKERRQ(PetscBLASIntCast(n-i,&n_i));
          n_i_2 = n_i - 2;
          CHKERRQ(PetscBLASIntCast(i+2,&i_2));
          CHKERRQ(PetscBLASIntCast(i,&i_));
          if (b11 < 0.0) {
            cr = -cr; sr = -sr;
            b11 = -b11; b22 = -b22;
          }
          PetscStackCallBLAS("BLASrot",BLASrot_(&n_i,&S[ldS*i+i],&ldS_,&S[ldS*i+i+1],&ldS_,&cl,&sl));
          PetscStackCallBLAS("BLASrot",BLASrot_(&i_2,&S[ldS*i],&one,&S[ldS*(i+1)],&one,&cr,&sr));
          PetscStackCallBLAS("BLASrot",BLASrot_(&n_i_2,&T[ldT*(i+2)+i],&ldT_,&T[ldT*(i+2)+i+1],&ldT_,&cl,&sl));
          PetscStackCallBLAS("BLASrot",BLASrot_(&i_,&T[ldT*i],&one,&T[ldT*(i+1)],&one,&cr,&sr));
          if (X) PetscStackCallBLAS("BLASrot",BLASrot_(&n_,&X[ldX*i],&one,&X[ldX*(i+1)],&one,&cr,&sr));
          if (Y) PetscStackCallBLAS("BLASrot",BLASrot_(&n_,&Y[ldY*i],&one,&Y[ldY*(i+1)],&one,&cl,&sl));
          T[ldT*i+i] = b11; T[ldT*i+i+1] = 0.0;
          T[ldT*(i+1)+i] = 0.0; T[ldT*(i+1)+i+1] = b22;
        }
      }
      i++;
    }
  }
#endif
  PetscFunctionReturn(0);
}

PetscErrorCode DSSolve_GNHEP(DS ds,PetscScalar *wr,PetscScalar *wi)
{
  PetscScalar    *work,*beta,a;
  PetscInt       i;
  PetscBLASInt   lwork,info,n,ld,iaux;
  PetscScalar    *A = ds->mat[DS_MAT_A],*B = ds->mat[DS_MAT_B],*Z = ds->mat[DS_MAT_Z],*Q = ds->mat[DS_MAT_Q];

  PetscFunctionBegin;
#if !defined(PETSC_USE_COMPLEX)
  PetscValidScalarPointer(wi,3);
#endif
  CHKERRQ(PetscBLASIntCast(ds->n,&n));
  CHKERRQ(PetscBLASIntCast(ds->ld,&ld));
  lwork = -1;
#if !defined(PETSC_USE_COMPLEX)
  PetscStackCallBLAS("LAPACKgges",LAPACKgges_("V","V","N",NULL,&n,A,&ld,B,&ld,&iaux,wr,wi,NULL,Z,&ld,Q,&ld,&a,&lwork,NULL,&info));
  CHKERRQ(PetscBLASIntCast((PetscInt)a,&lwork));
  CHKERRQ(DSAllocateWork_Private(ds,lwork+ld,0,0));
  beta = ds->work;
  work = beta+ds->n;
  CHKERRQ(PetscBLASIntCast(ds->lwork-ds->n,&lwork));
  PetscStackCallBLAS("LAPACKgges",LAPACKgges_("V","V","N",NULL,&n,A,&ld,B,&ld,&iaux,wr,wi,beta,Z,&ld,Q,&ld,work,&lwork,NULL,&info));
#else
  PetscStackCallBLAS("LAPACKgges",LAPACKgges_("V","V","N",NULL,&n,A,&ld,B,&ld,&iaux,wr,NULL,Z,&ld,Q,&ld,&a,&lwork,NULL,NULL,&info));
  CHKERRQ(PetscBLASIntCast((PetscInt)PetscRealPart(a),&lwork));
  CHKERRQ(DSAllocateWork_Private(ds,lwork+ld,8*ld,0));
  beta = ds->work;
  work = beta+ds->n;
  CHKERRQ(PetscBLASIntCast(ds->lwork-ds->n,&lwork));
  PetscStackCallBLAS("LAPACKgges",LAPACKgges_("V","V","N",NULL,&n,A,&ld,B,&ld,&iaux,wr,beta,Z,&ld,Q,&ld,work,&lwork,ds->rwork,NULL,&info));
#endif
  SlepcCheckLapackInfo("gges",info);
  for (i=0;i<n;i++) {
    if (beta[i]==0.0) wr[i] = (PetscRealPart(wr[i])>0.0)? PETSC_MAX_REAL: PETSC_MIN_REAL;
    else wr[i] /= beta[i];
#if !defined(PETSC_USE_COMPLEX)
    if (beta[i]==0.0) wi[i] = (wi[i]>0.0)? PETSC_MAX_REAL: PETSC_MIN_REAL;
    else wi[i] /= beta[i];
#else
    if (wi) wi[i] = 0.0;
#endif
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSSynchronize_GNHEP(DS ds,PetscScalar eigr[],PetscScalar eigi[])
{
  PetscInt       ld=ds->ld,l=ds->l,k;
  PetscMPIInt    n,rank,off=0,size,ldn;

  PetscFunctionBegin;
  k = 2*(ds->n-l)*ld;
  if (ds->state>DS_STATE_RAW) k += 2*(ds->n-l)*ld;
  if (eigr) k += (ds->n-l);
  if (eigi) k += (ds->n-l);
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
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DSTruncate_GNHEP(DS ds,PetscInt n,PetscBool trim)
{
  PetscInt    i,ld=ds->ld,l=ds->l;
  PetscScalar *A = ds->mat[DS_MAT_A],*B = ds->mat[DS_MAT_B];

  PetscFunctionBegin;
#if defined(PETSC_USE_DEBUG)
  /* make sure diagonal 2x2 block is not broken */
  PetscCheck(ds->state<DS_STATE_CONDENSED || n==0 || n==ds->n || (A[n+(n-1)*ld]==0.0 && B[n+(n-1)*ld]==0.0),PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"The given size would break a 2x2 block, call DSGetTruncateSize() first");
#endif
  if (trim) {
    if (ds->extrarow) {   /* clean extra row */
      for (i=l;i<ds->n;i++) A[ds->n+i*ld] = 0.0;
      for (i=l;i<ds->n;i++) B[ds->n+i*ld] = 0.0;
    }
    ds->l = 0;
    ds->k = 0;
    ds->n = n;
    ds->t = ds->n;   /* truncated length equal to the new dimension */
  } else {
    if (ds->extrarow && ds->k==ds->n) {
      /* copy entries of extra row to the new position, then clean last row */
      for (i=l;i<n;i++) A[n+i*ld] = A[ds->n+i*ld];
      for (i=l;i<ds->n;i++) A[ds->n+i*ld] = 0.0;
      for (i=l;i<n;i++) B[n+i*ld] = B[ds->n+i*ld];
      for (i=l;i<ds->n;i++) B[ds->n+i*ld] = 0.0;
    }
    ds->k = (ds->extrarow)? n: 0;
    ds->t = ds->n;   /* truncated length equal to previous dimension */
    ds->n = n;
  }
  PetscFunctionReturn(0);
}

/*MC
   DSGNHEP - Dense Generalized Non-Hermitian Eigenvalue Problem.

   Level: beginner

   Notes:
   The problem is expressed as A*X = B*X*Lambda, where (A,B) is the input
   matrix pencil. Lambda is a diagonal matrix whose diagonal elements are the
   arguments of DSSolve(). After solve, (A,B) is overwritten with the
   generalized (real) Schur form (S,T) = (Z'*A*Q,Z'*B*Q), with the first
   matrix being upper quasi-triangular and the second one triangular.

   Used DS matrices:
+  DS_MAT_A - first problem matrix
.  DS_MAT_B - second problem matrix
.  DS_MAT_Q - first orthogonal/unitary transformation that reduces to
   generalized (real) Schur form
-  DS_MAT_Z - second orthogonal/unitary transformation that reduces to
   generalized (real) Schur form

   Implemented methods:
.  0 - QZ iteration (_gges)

.seealso: DSCreate(), DSSetType(), DSType
M*/
SLEPC_EXTERN PetscErrorCode DSCreate_GNHEP(DS ds)
{
  PetscFunctionBegin;
  ds->ops->allocate        = DSAllocate_GNHEP;
  ds->ops->view            = DSView_GNHEP;
  ds->ops->vectors         = DSVectors_GNHEP;
  ds->ops->solve[0]        = DSSolve_GNHEP;
  ds->ops->sort            = DSSort_GNHEP;
  ds->ops->synchronize     = DSSynchronize_GNHEP;
  ds->ops->gettruncatesize = DSGetTruncateSize_Default;
  ds->ops->truncate        = DSTruncate_GNHEP;
  ds->ops->update          = DSUpdateExtraRow_GNHEP;
  PetscFunctionReturn(0);
}
