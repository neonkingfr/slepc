/*
   Private DS routines

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2012, Universitat Politecnica de Valencia, Spain

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

#include <slepc-private/dsimpl.h>      /*I "slepcds.h" I*/
#include <slepcblaslapack.h>

PetscErrorCode SlepcDenseMatProd(PetscScalar *C, PetscInt _ldC, PetscScalar b, PetscScalar a, const PetscScalar *A, PetscInt _ldA, PetscInt rA, PetscInt cA, PetscBool At, const PetscScalar *B, PetscInt _ldB, PetscInt rB, PetscInt cB, PetscBool Bt);

#undef __FUNCT__  
#define __FUNCT__ "DSAllocateMat_Private"
PetscErrorCode DSAllocateMat_Private(DS ds,DSMatType m)
{
  PetscInt       sz;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (m==DS_MAT_T) sz = 3*ds->ld*sizeof(PetscScalar);
  else if (m==DS_MAT_D) sz = ds->ld*sizeof(PetscScalar);
  else sz = ds->ld*ds->ld*sizeof(PetscScalar);
  if (ds->mat[m]) { ierr = PetscFree(ds->mat[m]);CHKERRQ(ierr); }
  else { ierr = PetscLogObjectMemory(ds,sz);CHKERRQ(ierr); }
  ierr = PetscMalloc(sz,&ds->mat[m]);CHKERRQ(ierr); 
  ierr = PetscMemzero(ds->mat[m],sz);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSAllocateMatReal_Private"
PetscErrorCode DSAllocateMatReal_Private(DS ds,DSMatType m)
{
  PetscInt       sz;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (m==DS_MAT_T) sz = 3*ds->ld*sizeof(PetscReal);
  else if (m==DS_MAT_D) sz = ds->ld*sizeof(PetscReal);
  else sz = ds->ld*ds->ld*sizeof(PetscReal);
  if (!ds->rmat[m]) {
    ierr = PetscLogObjectMemory(ds,sz);CHKERRQ(ierr);
    ierr = PetscMalloc(sz,&ds->rmat[m]);CHKERRQ(ierr); 
  }
  ierr = PetscMemzero(ds->rmat[m],sz);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSAllocateWork_Private"
PetscErrorCode DSAllocateWork_Private(DS ds,PetscInt s,PetscInt r,PetscInt i)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (s>ds->lwork) {
    ierr = PetscFree(ds->work);CHKERRQ(ierr);
    ierr = PetscMalloc(s*sizeof(PetscScalar),&ds->work);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(ds,(s-ds->lwork)*sizeof(PetscScalar));CHKERRQ(ierr); 
    ds->lwork = s;
  }
  if (r>ds->lrwork) {
    ierr = PetscFree(ds->rwork);CHKERRQ(ierr);
    ierr = PetscMalloc(r*sizeof(PetscReal),&ds->rwork);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(ds,(r-ds->lrwork)*sizeof(PetscReal));CHKERRQ(ierr); 
    ds->lrwork = r;
  }
  if (i>ds->liwork) {
    ierr = PetscFree(ds->iwork);CHKERRQ(ierr);
    ierr = PetscMalloc(i*sizeof(PetscBLASInt),&ds->iwork);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(ds,(i-ds->liwork)*sizeof(PetscBLASInt));CHKERRQ(ierr); 
    ds->liwork = i;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSViewMat_Private"
PetscErrorCode DSViewMat_Private(DS ds,PetscViewer viewer,DSMatType m)
{
  PetscErrorCode    ierr;
  PetscInt          i,j,rows,cols;
  PetscScalar       *v;
  PetscViewerFormat format;
#if defined(PETSC_USE_COMPLEX)
  PetscBool         allreal = PETSC_TRUE;
#endif

  PetscFunctionBegin;
  ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) PetscFunctionReturn(0);
  ierr = PetscViewerASCIIUseTabs(viewer,PETSC_FALSE);CHKERRQ(ierr);
  if (ds->state==DS_STATE_TRUNCATED && m>=DS_MAT_Q) rows = ds->t;
  else rows = (m==DS_MAT_A && ds->extrarow)? ds->n+1: ds->n;
  cols = (ds->m!=0)? ds->m: ds->n;
#if defined(PETSC_USE_COMPLEX)
  /* determine if matrix has all real values */
  v = ds->mat[m];
  for (i=0;i<rows;i++)
    for (j=0;j<cols;j++)
      if (PetscImaginaryPart(v[i+j*ds->ld])) { allreal = PETSC_FALSE; break; }
#endif
  if (format == PETSC_VIEWER_ASCII_MATLAB) {
    ierr = PetscViewerASCIIPrintf(viewer,"%% Size = %D %D\n",rows,cols);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"%s = [\n",DSMatName[m]);CHKERRQ(ierr);
  } else {
    ierr = PetscViewerASCIIPrintf(viewer,"Matrix %s =\n",DSMatName[m]);CHKERRQ(ierr);
  }

  for (i=0;i<rows;i++) {
    v = ds->mat[m]+i;
    for (j=0;j<cols;j++) {
#if defined(PETSC_USE_COMPLEX)
      if (allreal) {
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e ",PetscRealPart(*v));CHKERRQ(ierr);
      } else {
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e + %18.16ei ",PetscRealPart(*v),PetscImaginaryPart(*v));CHKERRQ(ierr);
      }
#else
      ierr = PetscViewerASCIIPrintf(viewer,"%18.16e ",*v);CHKERRQ(ierr);
#endif
      v += ds->ld;
    }
    ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
  }

  if (format == PETSC_VIEWER_ASCII_MATLAB) {
    ierr = PetscViewerASCIIPrintf(viewer,"];\n");CHKERRQ(ierr);
  }
  ierr = PetscViewerASCIIUseTabs(viewer,PETSC_TRUE);CHKERRQ(ierr);
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DSSortEigenvalues_Private"
PetscErrorCode DSSortEigenvalues_Private(DS ds,PetscScalar *wr,PetscScalar *wi,PetscInt *perm,PetscBool isghiep)
{
  PetscErrorCode ierr;
  PetscScalar    re,im,wi0;
  PetscInt       i,j,result,tmp1,tmp2=0,d=1;

  PetscFunctionBegin;
  for (i=0;i<ds->n;i++) perm[i] = i;
  /* insertion sort */
  i=ds->l+1;
#if !defined(PETSC_USE_COMPLEX)
  if (wi && wi[perm[i-1]]!=0.0) i++; /* initial value is complex */
#else
  if (isghiep && PetscImaginaryPart(wr[perm[i-1]])!=0.0) i++;
#endif
  for (;i<ds->n;i+=d) {
    re = wr[perm[i]];
    if (wi) im = wi[perm[i]];
    else im = 0.0;
    tmp1 = perm[i];
#if !defined(PETSC_USE_COMPLEX)
    if (im!=0.0) { d = 2; tmp2 = perm[i+1]; }
    else d = 1;
#else
    if (isghiep && PetscImaginaryPart(re)!=0.0) { d = 2; tmp2 = perm[i+1]; }
    else d = 1;
#endif
    j = i-1;
    if (wi) wi0 = wi[perm[j]];
    else wi0 = 0.0;
    ierr = (*ds->comp_fun)(re,im,wr[perm[j]],wi0,&result,ds->comp_ctx);CHKERRQ(ierr);
    while (result<0 && j>=ds->l) {
      perm[j+d] = perm[j];
      j--;
#if !defined(PETSC_USE_COMPLEX)
      if (wi && wi[perm[j+1]]!=0)
#else
      if (isghiep && PetscImaginaryPart(wr[perm[j+1]])!=0)
#endif
        { perm[j+d] = perm[j]; j--; }

     if (j>=ds->l) {
       if (wi) wi0 = wi[perm[j]];
       else wi0 = 0.0;
       ierr = (*ds->comp_fun)(re,im,wr[perm[j]],wi0,&result,ds->comp_ctx);CHKERRQ(ierr);
     }
    }
    perm[j+1] = tmp1;
    if(d==2) perm[j+2] = tmp2;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSSortEigenvaluesReal_Private"
PetscErrorCode DSSortEigenvaluesReal_Private(DS ds,PetscReal *eig,PetscInt *perm)
{
  PetscErrorCode ierr;
  PetscScalar    re;
  PetscInt       i,j,result,tmp,l,n;

  PetscFunctionBegin;
  n = ds->n;
  l = ds->l;
  for (i=0;i<n;i++) perm[i] = i;
  /* insertion sort */
  for (i=l+1;i<n;i++) {
    re = eig[perm[i]];
    j = i-1;
    ierr = (*ds->comp_fun)(re,0.0,eig[perm[j]],0.0,&result,ds->comp_ctx);CHKERRQ(ierr);
    while (result<0 && j>=l) {
      tmp = perm[j]; perm[j] = perm[j+1]; perm[j+1] = tmp; j--;
      if (j>=l) {
        ierr = (*ds->comp_fun)(re,0.0,eig[perm[j]],0.0,&result,ds->comp_ctx);CHKERRQ(ierr);
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSCopyMatrix_Private"
/*
  DSCopyMatrix_Private - Copies the trailing block of a matrix (from
  rows/columns l to n).
*/
PetscErrorCode DSCopyMatrix_Private(DS ds,DSMatType dst,DSMatType src)
{
  PetscErrorCode ierr;
  PetscInt    j,m,off,ld;
  PetscScalar *S,*D;

  PetscFunctionBegin;
  ld  = ds->ld;
  m   = ds->n-ds->l;
  off = ds->l+ds->l*ld;
  S   = ds->mat[src];
  D   = ds->mat[dst];
  for (j=0;j<m;j++) {
    ierr = PetscMemcpy(D+off+j*ld,S+off+j*ld,m*sizeof(PetscScalar));CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSPermuteColumns_Private"
PetscErrorCode DSPermuteColumns_Private(DS ds,PetscInt l,PetscInt n,DSMatType mat,PetscInt *perm)
{
  PetscInt    i,j,k,p,ld;
  PetscScalar *Q,rtmp;

  PetscFunctionBegin;
  ld = ds->ld;
  Q  = ds->mat[mat];
  for (i=l;i<n;i++) {
    p = perm[i];
    if (p != i) {
      j = i + 1;
      while (perm[j] != i) j++;
      perm[j] = p; perm[i] = i;
      /* swap columns i and j */
      for (k=0;k<n;k++) {
        rtmp = Q[k+p*ld]; Q[k+p*ld] = Q[k+i*ld]; Q[k+i*ld] = rtmp;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSPermuteRows_Private"
PetscErrorCode DSPermuteRows_Private(DS ds,PetscInt l,PetscInt n,DSMatType mat,PetscInt *perm)
{
  PetscInt    i,j,m=ds->m,k,p,ld;
  PetscScalar *Q,rtmp;

  PetscFunctionBegin;
  if (m==0) SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ARG_WRONG,"m was not set");
  ld = ds->ld;
  Q  = ds->mat[mat];
  for (i=l;i<n;i++) {
    p = perm[i];
    if (p != i) {
      j = i + 1;
      while (perm[j] != i) j++;
      perm[j] = p; perm[i] = i;
      /* swap rows i and j */
      for (k=0;k<m;k++) {
        rtmp = Q[p+k*ld]; Q[p+k*ld] = Q[i+k*ld]; Q[i+k*ld] = rtmp;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSPermuteBoth_Private"
PetscErrorCode DSPermuteBoth_Private(DS ds,PetscInt l,PetscInt n,DSMatType mat1,DSMatType mat2,PetscInt *perm)
{
  PetscInt    i,j,m=ds->m,k,p,ld;
  PetscScalar *U,*VT,rtmp;

  PetscFunctionBegin;
  if (m==0) SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ARG_WRONG,"m was not set");
  ld = ds->ld;
  U  = ds->mat[mat1];
  VT = ds->mat[mat2];
  for (i=l;i<n;i++) {
    p = perm[i];
    if (p != i) {
      j = i + 1;
      while (perm[j] != i) j++;
      perm[j] = p; perm[i] = i;
      /* swap columns i and j of U */
      for (k=0;k<n;k++) {
        rtmp = U[k+p*ld]; U[k+p*ld] = U[k+i*ld]; U[k+i*ld] = rtmp;
      }
      /* swap rows i and j of VT */
      for (k=0;k<m;k++) {
        rtmp = VT[p+k*ld]; VT[p+k*ld] = VT[i+k*ld]; VT[i+k*ld] = rtmp;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSSetIdentity"
/*
   DSSetIdentity - Copy the identity (a diagonal matrix with ones) on the
   active part of a matrix.
*/
PetscErrorCode DSSetIdentity(DS ds,DSMatType mat)
{
  PetscErrorCode ierr;
  PetscScalar    *x;
  PetscInt       i,ld,n,l;

  PetscFunctionBegin;
  ierr = DSGetDimensions(ds,&n,PETSC_NULL,&l,PETSC_NULL);CHKERRQ(ierr);
  ierr = DSGetLeadingDimension(ds,&ld);CHKERRQ(ierr);
  ierr = DSGetArray(ds,mat,&x);CHKERRQ(ierr);
  ierr = PetscLogEventBegin(DS_Other,ds,0,0,0);CHKERRQ(ierr);
  ierr = PetscMemzero(&x[ld*l],ld*(n-l)*sizeof(PetscScalar));CHKERRQ(ierr);
  for (i=l;i<n;i++) {
    x[ld*i+i] = 1.0;
  }
  ierr = DSRestoreArray(ds,mat,&x);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(DS_Other,ds,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSOrthogonalize"
/*
   DSOrthogonalize - Orthogonalize the columns of a matrix.

   Input Parameters:
+  ds   - the direct solver context
.  mat  - a matrix
-  cols - number of columns to orthogonalize (starting from the column zero)

   Output Parameter:
.  lindcols - number of linearly independent columns of the matrix (can be PETSC_NULL) 
*/
PetscErrorCode DSOrthogonalize(DS ds,DSMatType mat,PetscInt cols,PetscInt *lindcols)
{
#if defined(PETSC_MISSING_LAPACK_GEQRF) || defined(SLEPC_MISSING_LAPACK_ORGQR)
  PetscFunctionBegin;
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"GEQRF/ORGQR - Lapack routine is unavailable");
#else
  PetscErrorCode  ierr;
  PetscInt        n,l,ld;
  PetscBLASInt    ld_,rA,cA,info,ltau,lw;
  PetscScalar     *A,*tau,*w,saux;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ds,DS_CLASSID,1);
  PetscValidLogicalCollectiveEnum(ds,mat,2);
  PetscValidLogicalCollectiveInt(ds,cols,3);
  ierr = DSGetDimensions(ds,&n,PETSC_NULL,&l,PETSC_NULL);CHKERRQ(ierr);
  ierr = DSGetLeadingDimension(ds,&ld);CHKERRQ(ierr);
  n = n - l;
  if (cols > n) SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ARG_WRONG,"Invalid number of columns");
  if (n == 0 || cols == 0) { PetscFunctionReturn(0); }
  ierr = DSGetArray(ds,mat,&A);CHKERRQ(ierr);
  ltau = PetscBLASIntCast(PetscMin(cols,n));
  ld_ = PetscBLASIntCast(ld);
  rA = PetscBLASIntCast(n);
  cA = PetscBLASIntCast(cols);
  lw = -1;
  LAPACKgeqrf_(&rA,&cA,A,&ld_,PETSC_NULL,&saux,&lw,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGEQRF %d",info);
  lw = (PetscBLASInt)PetscRealPart(saux);
  ierr = DSAllocateWork_Private(ds,lw+ltau,0,0);CHKERRQ(ierr);
  tau = ds->work;
  w = &tau[ltau];
  ierr = PetscLogEventBegin(DS_Other,ds,0,0,0);CHKERRQ(ierr);
  ierr = PetscFPTrapPush(PETSC_FP_TRAP_OFF);CHKERRQ(ierr);
  LAPACKgeqrf_(&rA,&cA,&A[ld*l+l],&ld_,tau,w,&lw,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGEQRF %d",info);
  LAPACKorgqr_(&rA,&ltau,&ltau,&A[ld*l+l],&ld_,tau,w,&lw,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xORGQR %d",info);
  ierr = PetscFPTrapPop();CHKERRQ(ierr);
  ierr = PetscLogEventEnd(DS_Other,ds,0,0,0);CHKERRQ(ierr);
  ierr = DSRestoreArray(ds,mat,&A);CHKERRQ(ierr);
  ierr = PetscObjectStateIncrease((PetscObject)ds);CHKERRQ(ierr);
  if (lindcols) *lindcols = ltau;
  PetscFunctionReturn(0);
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "DSPseudoOrthogonalize"
/*
   DSPseudoOrthogonalize - Orthogonalize the columns of a matrix with Modified
   Gram-Schmidt in an indefinite inner product space defined by a signature.

   Input Parameters:
+  ds   - the direct solver context
.  mat  - the matrix
.  cols - number of columns to orthogonalize (starting from the column zero)
-  s    - the signature that defines the inner product

   Output Parameter:
+  lindcols - linear independent columns of the matrix (can be PETSC_NULL) 
-  ns - the new norm of the vectors (can be PETSC_NULL)
*/
PetscErrorCode DSPseudoOrthogonalize(DS ds,DSMatType mat,PetscInt cols,PetscReal *s,PetscInt *lindcols,PetscReal *ns)
{
  PetscErrorCode  ierr;
  PetscInt        i,j,k,l,n,ld;
  PetscBLASInt    one=1,rA_;
  PetscScalar     alpha,*A,*A_,*m,*h,nr0;
  PetscReal       nr_o,nr,*ns_;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ds,DS_CLASSID,1);
  PetscValidLogicalCollectiveEnum(ds,mat,2);
  PetscValidLogicalCollectiveInt(ds,cols,3);
  PetscValidScalarPointer(s,4);
  if (ns) PetscValidPointer(ns,6);
  ierr = DSGetDimensions(ds,&n,PETSC_NULL,&l,PETSC_NULL);CHKERRQ(ierr);
  ierr = DSGetLeadingDimension(ds,&ld);CHKERRQ(ierr);
  n = n - l;
  if (cols > n) SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ARG_WRONG,"Invalid number of columns");
  if (n == 0 || cols == 0) { PetscFunctionReturn(0); }
  rA_ = PetscBLASIntCast(n);
  ierr = DSGetArray(ds,mat,&A_);CHKERRQ(ierr);
  A = &A_[ld*l+l];
  ierr = DSAllocateWork_Private(ds,n+cols,ns?0:cols,0);CHKERRQ(ierr);
  m = ds->work;
  h = &m[n];
  ns_ = ns ? ns : ds->rwork;
  ierr = PetscLogEventBegin(DS_Other,ds,0,0,0);CHKERRQ(ierr);
  for (i=0; i<cols; i++) {
    /* m <- diag(s)*A[i] */
    for (k=0; k<n; k++) m[k] = s[k]*A[k+i*ld];
    /* nr_o <- mynorm(A[i]'*m), mynorm(x) = sign(x)*sqrt(|x|) */
    ierr = SlepcDenseMatProd(&nr0,1,0.0,1.0,&A[ld*i],ld,n,1,PETSC_TRUE,m,n,n,1,PETSC_FALSE);CHKERRQ(ierr);
    nr = nr_o = PetscSign(PetscRealPart(nr0))*PetscSqrtReal(PetscAbsScalar(nr0));
    for (j=0; j<3 && i>0; j++) {
      /* h <- A[0:i-1]'*m */
      ierr = SlepcDenseMatProd(h,i,0.0,1.0,A,ld,n,i,PETSC_TRUE,m,n,n,1,PETSC_FALSE);CHKERRQ(ierr);
      /* h <- diag(ns)*h */
      for (k=0; k<i; k++) h[k] *= ns_[k];
      /* A[i] <- A[i] - A[0:i-1]*h */
      ierr = SlepcDenseMatProd(&A[ld*i],ld,1.0,-1.0,A,ld,n,i,PETSC_FALSE,h,i,i,1,PETSC_FALSE);CHKERRQ(ierr);
      /* m <- diag(s)*A[i] */
      for (k=0; k<n; k++) m[k] = s[k]*A[k+i*ld];
      /* nr_o <- mynorm(A[i]'*m) */
      ierr = SlepcDenseMatProd(&nr0,1,0.0,1.0,&A[ld*i],ld,n,1,PETSC_TRUE,m,n,n,1,PETSC_FALSE);CHKERRQ(ierr);
      nr = PetscSign(PetscRealPart(nr0))*PetscSqrtReal(PetscAbsScalar(nr0));
      if (PetscAbs(nr) < PETSC_MACHINE_EPSILON) SETERRQ(PETSC_COMM_SELF,1, "Linear dependency detected");
      if (PetscAbs(nr) > 0.7*PetscAbs(nr_o)) break;
      nr_o = nr;
    }
    ns_[i] = PetscSign(nr);
    /* A[i] <- A[i]/|nr| */
    alpha = 1.0/PetscAbs(nr);
    BLASscal_(&rA_,&alpha,&A[i*ld],&one);
  }
  ierr = PetscLogEventEnd(DS_Other,ds,0,0,0);CHKERRQ(ierr);
  ierr = DSRestoreArray(ds,mat,&A_);CHKERRQ(ierr);
  ierr = PetscObjectStateIncrease((PetscObject)ds);CHKERRQ(ierr);
  if (lindcols) *lindcols = cols;
  PetscFunctionReturn(0);
}