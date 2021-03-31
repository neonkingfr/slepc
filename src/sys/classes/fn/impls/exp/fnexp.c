/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Exponential function  exp(x)
*/

#include <slepc/private/fnimpl.h>      /*I "slepcfn.h" I*/
#include <slepcblaslapack.h>

PetscErrorCode FNEvaluateFunction_Exp(FN fn,PetscScalar x,PetscScalar *y)
{
  PetscFunctionBegin;
  *y = PetscExpScalar(x);
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateDerivative_Exp(FN fn,PetscScalar x,PetscScalar *y)
{
  PetscFunctionBegin;
  *y = PetscExpScalar(x);
  PetscFunctionReturn(0);
}

#define MAX_PADE 6
#define SWAP(a,b,t) {t=a;a=b;b=t;}

PetscErrorCode FNEvaluateFunctionMat_Exp_Pade(FN fn,Mat A,Mat B)
{
  PetscErrorCode    ierr;
  PetscBLASInt      n=0,ld,ld2,*ipiv,info,inc=1;
  PetscInt          m,j,k,sexp;
  PetscBool         odd;
  const PetscInt    p=MAX_PADE;
  PetscReal         c[MAX_PADE+1],s,*rwork;
  PetscScalar       scale,mone=-1.0,one=1.0,two=2.0,zero=0.0;
  PetscScalar       *Ba,*As,*A2,*Q,*P,*W,*aux;
  const PetscScalar *Aa;

  PetscFunctionBegin;
  ierr = MatDenseGetArrayRead(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  ierr = MatGetSize(A,&m,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(m,&n);CHKERRQ(ierr);
  ld  = n;
  ld2 = ld*ld;
  P   = Ba;
  ierr = PetscMalloc6(m*m,&Q,m*m,&W,m*m,&As,m*m,&A2,ld,&rwork,ld,&ipiv);CHKERRQ(ierr);
  ierr = PetscArraycpy(As,Aa,ld2);CHKERRQ(ierr);

  /* Pade' coefficients */
  c[0] = 1.0;
  for (k=1;k<=p;k++) c[k] = c[k-1]*(p+1-k)/(k*(2*p+1-k));

  /* Scaling */
  s = LAPACKlange_("I",&n,&n,As,&ld,rwork);
  ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);
  if (s>0.5) {
    sexp = PetscMax(0,(int)(PetscLogReal(s)/PetscLogReal(2.0))+2);
    scale = PetscPowRealInt(2.0,-sexp);
    PetscStackCallBLAS("BLASscal",BLASscal_(&ld2,&scale,As,&inc));
    ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);
  } else sexp = 0;

  /* Horner evaluation */
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&one,As,&ld,As,&ld,&zero,A2,&ld));
  ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  ierr = PetscArrayzero(Q,ld2);CHKERRQ(ierr);
  ierr = PetscArrayzero(P,ld2);CHKERRQ(ierr);
  for (j=0;j<n;j++) {
    Q[j+j*ld] = c[p];
    P[j+j*ld] = c[p-1];
  }

  odd = PETSC_TRUE;
  for (k=p-1;k>0;k--) {
    if (odd) {
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&one,Q,&ld,A2,&ld,&zero,W,&ld));
      SWAP(Q,W,aux);
      for (j=0;j<n;j++) Q[j+j*ld] += c[k-1];
      odd = PETSC_FALSE;
    } else {
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&one,P,&ld,A2,&ld,&zero,W,&ld));
      SWAP(P,W,aux);
      for (j=0;j<n;j++) P[j+j*ld] += c[k-1];
      odd = PETSC_TRUE;
    }
    ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  }
  /*if (odd) {
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&one,Q,&ld,As,&ld,&zero,W,&ld));
    SWAP(Q,W,aux);
    PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&ld2,&mone,P,&inc,Q,&inc));
    PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n,&n,Q,&ld,ipiv,P,&ld,&info));
    SlepcCheckLapackInfo("gesv",info);
    PetscStackCallBLAS("BLASscal",BLASscal_(&ld2,&two,P,&inc));
    for (j=0;j<n;j++) P[j+j*ld] += 1.0;
    PetscStackCallBLAS("BLASscal",BLASscal_(&ld2,&mone,P,&inc));
  } else {*/
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&one,P,&ld,As,&ld,&zero,W,&ld));
    SWAP(P,W,aux);
    PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&ld2,&mone,P,&inc,Q,&inc));
    PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n,&n,Q,&ld,ipiv,P,&ld,&info));
    SlepcCheckLapackInfo("gesv",info);
    PetscStackCallBLAS("BLASscal",BLASscal_(&ld2,&two,P,&inc));
    for (j=0;j<n;j++) P[j+j*ld] += 1.0;
  /*}*/
  ierr = PetscLogFlops(2.0*n*n*n+2.0*n*n*n/3.0+4.0*n*n);CHKERRQ(ierr);

  for (k=1;k<=sexp;k++) {
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&one,P,&ld,P,&ld,&zero,W,&ld));
    ierr = PetscArraycpy(P,W,ld2);CHKERRQ(ierr);
  }
  if (P!=Ba) { ierr = PetscArraycpy(Ba,P,ld2);CHKERRQ(ierr); }
  ierr = PetscLogFlops(2.0*n*n*n*sexp);CHKERRQ(ierr);

  ierr = PetscFree6(Q,W,As,A2,rwork,ipiv);CHKERRQ(ierr);
  ierr = MatDenseRestoreArrayRead(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
 * Set scaling factor (s) and Pade degree (k,m)
 */
static PetscErrorCode sexpm_params(PetscReal nrm,PetscInt *s,PetscInt *k,PetscInt *m)
{
  PetscFunctionBegin;
  if (nrm>1) {
    if      (nrm<200)  {*s = 4; *k = 5; *m = *k-1;}
    else if (nrm<1e4)  {*s = 4; *k = 4; *m = *k+1;}
    else if (nrm<1e6)  {*s = 4; *k = 3; *m = *k+1;}
    else if (nrm<1e9)  {*s = 3; *k = 3; *m = *k+1;}
    else if (nrm<1e11) {*s = 2; *k = 3; *m = *k+1;}
    else if (nrm<1e12) {*s = 2; *k = 2; *m = *k+1;}
    else if (nrm<1e14) {*s = 2; *k = 1; *m = *k+1;}
    else               {*s = 1; *k = 1; *m = *k+1;}
  } else { /* nrm<1 */
    if       (nrm>0.5)  {*s = 4; *k = 4; *m = *k-1;}
    else  if (nrm>0.3)  {*s = 3; *k = 4; *m = *k-1;}
    else  if (nrm>0.15) {*s = 2; *k = 4; *m = *k-1;}
    else  if (nrm>0.07) {*s = 1; *k = 4; *m = *k-1;}
    else  if (nrm>0.01) {*s = 0; *k = 4; *m = *k-1;}
    else  if (nrm>3e-4) {*s = 0; *k = 3; *m = *k-1;}
    else  if (nrm>1e-5) {*s = 0; *k = 3; *m = 0;}
    else  if (nrm>1e-8) {*s = 0; *k = 2; *m = 0;}
    else                {*s = 0; *k = 1; *m = 0;}
  }
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_COMPLEX)
/*
 * Partial fraction form coefficients.
 * If query, the function returns the size necessary to store the coefficients.
 */
static PetscErrorCode getcoeffs(PetscInt k,PetscInt m,PetscComplex *r,PetscComplex *q,PetscComplex *remain,PetscBool query)
{
  PetscInt i;
  const PetscComplex /* m == k+1 */
    p1r4[5] = {-1.582680186458572e+01 - 2.412564578224361e+01*PETSC_i,
               -1.582680186458572e+01 + 2.412564578224361e+01*PETSC_i,
                1.499984465975511e+02 + 6.804227952202417e+01*PETSC_i,
                1.499984465975511e+02 - 6.804227952202417e+01*PETSC_i,
               -2.733432894659307e+02                                },
    p1q4[5] = { 3.655694325463550e+00 + 6.543736899360086e+00*PETSC_i,
                3.655694325463550e+00 - 6.543736899360086e+00*PETSC_i,
                5.700953298671832e+00 + 3.210265600308496e+00*PETSC_i,
                5.700953298671832e+00 - 3.210265600308496e+00*PETSC_i,
                6.286704751729261e+00                               },
    p1r3[4] = {-1.130153999597152e+01 + 1.247167585025031e+01*PETSC_i,
               -1.130153999597152e+01 - 1.247167585025031e+01*PETSC_i,
                1.330153999597152e+01 - 6.007173273704750e+01*PETSC_i,
                1.330153999597152e+01 + 6.007173273704750e+01*PETSC_i},
    p1q3[4] = { 3.212806896871536e+00 + 4.773087433276636e+00*PETSC_i,
                3.212806896871536e+00 - 4.773087433276636e+00*PETSC_i,
                4.787193103128464e+00 + 1.567476416895212e+00*PETSC_i,
                4.787193103128464e+00 - 1.567476416895212e+00*PETSC_i},
    p1r2[3] = { 7.648749087422928e+00 + 4.171640244747463e+00*PETSC_i,
                7.648749087422928e+00 - 4.171640244747463e+00*PETSC_i,
               -1.829749817484586e+01                                },
    p1q2[3] = { 2.681082873627756e+00 + 3.050430199247411e+00*PETSC_i,
                2.681082873627756e+00 - 3.050430199247411e+00*PETSC_i,
                3.637834252744491e+00                                },
    p1r1[2] = { 1.000000000000000e+00 - 3.535533905932738e+00*PETSC_i,
                1.000000000000000e+00 + 3.535533905932738e+00*PETSC_i},
    p1q1[2] = { 2.000000000000000e+00 + 1.414213562373095e+00*PETSC_i,
                2.000000000000000e+00 - 1.414213562373095e+00*PETSC_i};
  const PetscComplex /* m == k-1 */
    m1r5[4] = {-1.423367961376821e+02 - 1.385465094833037e+01*PETSC_i,
               -1.423367961376821e+02 + 1.385465094833037e+01*PETSC_i,
                2.647367961376822e+02 - 4.814394493714596e+02*PETSC_i,
                2.647367961376822e+02 + 4.814394493714596e+02*PETSC_i},
    m1q5[4] = { 5.203941240131764e+00 + 5.805856841805367e+00*PETSC_i,
                5.203941240131764e+00 - 5.805856841805367e+00*PETSC_i,
                6.796058759868242e+00 + 1.886649260140217e+00*PETSC_i,
                6.796058759868242e+00 - 1.886649260140217e+00*PETSC_i},
    m1r4[3] = { 2.484269593165883e+01 + 7.460342395992306e+01*PETSC_i,
                2.484269593165883e+01 - 7.460342395992306e+01*PETSC_i,
               -1.734353918633177e+02                                },
    m1q4[3] = { 4.675757014491557e+00 + 3.913489560603711e+00*PETSC_i,
                4.675757014491557e+00 - 3.913489560603711e+00*PETSC_i,
                5.648485971016893e+00                                },
    m1r3[2] = { 2.533333333333333e+01 - 2.733333333333333e+01*PETSC_i,
                2.533333333333333e+01 + 2.733333333333333e+01*PETSC_i},
    m1q3[2] = { 4.000000000000000e+00 + 2.000000000000000e+00*PETSC_i,
                4.000000000000000e+00 - 2.000000000000000e+00*PETSC_i};
  const PetscScalar /* m == k-1 */
    m1remain5[2] = { 2.000000000000000e-01,  9.800000000000000e+00},
    m1remain4[2] = {-2.500000000000000e-01, -7.750000000000000e+00},
    m1remain3[2] = { 3.333333333333333e-01,  5.666666666666667e+00},
    m1remain2[2] = {-0.5,                   -3.5},
    remain3[4] = {1.0/6.0, 1.0/2.0, 1, 1},
    remain2[3] = {1.0/2.0, 1, 1};

  PetscFunctionBegin;
  if (query) { /* query about buffer's size */
    if (m==k+1) {
      *remain = 0;
      *r = *q = k+1;
      PetscFunctionReturn(0); /* quick return */
    }
    if (m==k-1) {
      *remain = 2;
      if (k==5) *r = *q = 4;
      else if (k==4) *r = *q = 3;
      else if (k==3) *r = *q = 2;
      else if (k==2) *r = *q = 1;
    }
    if (m==0) {
      *r = *q = 0;
      *remain = k+1;
    }
  } else {
    if (m==k+1) {
      if (k==4) {
        for (i=0;i<5;i++) { r[i] = p1r4[i]; q[i] = p1q4[i]; }
      } else if (k==3) {
        for (i=0;i<4;i++) { r[i] = p1r3[i]; q[i] = p1q3[i]; }
      } else if (k==2) {
        for (i=0;i<3;i++) { r[i] = p1r2[i]; q[i] = p1q2[i]; }
      } else if (k==1) {
        for (i=0;i<2;i++) { r[i] = p1r1[i]; q[i] = p1q1[i]; }
      }
      PetscFunctionReturn(0); /* quick return */
    }
    if (m==k-1) {
      if (k==5) {
        for (i=0;i<4;i++) { r[i] = m1r5[i]; q[i] = m1q5[i]; }
        for (i=0;i<2;i++) remain[i] = m1remain5[i];
      } else if (k==4) {
        for (i=0;i<3;i++) { r[i] = m1r4[i]; q[i] = m1q4[i]; }
        for (i=0;i<2;i++) remain[i] = m1remain4[i];
      } else if (k==3) {
        for (i=0;i<2;i++) { r[i] = m1r3[i]; q[i] = m1q3[i]; remain[i] = m1remain3[i]; }
      } else if (k==2) {
        r[0] = -13.5; q[0] = 3;
        for (i=0;i<2;i++) remain[i] = m1remain2[i];
      }
    }
    if (m==0) {
      r = q = 0;
      if (k==3) {
        for (i=0;i<4;i++) remain[i] = remain3[i];
      } else if (k==2) {
        for (i=0;i<3;i++) remain[i] = remain2[i];
      }
    }
  }
  PetscFunctionReturn(0);
}

/*
 * Product form coefficients.
 * If query, the function returns the size necessary to store the coefficients.
 */
static PetscErrorCode getcoeffsproduct(PetscInt k,PetscInt m,PetscComplex *p,PetscComplex *q,PetscComplex *mult,PetscBool query)
{
  PetscInt i;
  const PetscComplex /* m == k+1 */
  p1p4[4] = {-5.203941240131764e+00 + 5.805856841805367e+00*PETSC_i,
             -5.203941240131764e+00 - 5.805856841805367e+00*PETSC_i,
             -6.796058759868242e+00 + 1.886649260140217e+00*PETSC_i,
             -6.796058759868242e+00 - 1.886649260140217e+00*PETSC_i},
  p1q4[5] = { 3.655694325463550e+00 + 6.543736899360086e+00*PETSC_i,
              3.655694325463550e+00 - 6.543736899360086e+00*PETSC_i,
              6.286704751729261e+00                                ,
              5.700953298671832e+00 + 3.210265600308496e+00*PETSC_i,
              5.700953298671832e+00 - 3.210265600308496e+00*PETSC_i},
  p1p3[3] = {-4.675757014491557e+00 + 3.913489560603711e+00*PETSC_i,
             -4.675757014491557e+00 - 3.913489560603711e+00*PETSC_i,
             -5.648485971016893e+00                                },
  p1q3[4] = { 3.212806896871536e+00 + 4.773087433276636e+00*PETSC_i,
              3.212806896871536e+00 - 4.773087433276636e+00*PETSC_i,
              4.787193103128464e+00 + 1.567476416895212e+00*PETSC_i,
              4.787193103128464e+00 - 1.567476416895212e+00*PETSC_i},
  p1p2[2] = {-4.00000000000000e+00  + 2.000000000000000e+00*PETSC_i,
             -4.00000000000000e+00  - 2.000000000000000e+00*PETSC_i},
  p1q2[3] = { 2.681082873627756e+00 + 3.050430199247411e+00*PETSC_i,
              2.681082873627756e+00 - 3.050430199247411e+00*PETSC_i,
              3.637834252744491e+00                               },
  p1q1[2] = { 2.000000000000000e+00 + 1.414213562373095e+00*PETSC_i,
              2.000000000000000e+00 - 1.414213562373095e+00*PETSC_i};
  const PetscComplex /* m == k-1 */
  m1p5[5] = {-3.655694325463550e+00 + 6.543736899360086e+00*PETSC_i,
             -3.655694325463550e+00 - 6.543736899360086e+00*PETSC_i,
             -6.286704751729261e+00                                ,
             -5.700953298671832e+00 + 3.210265600308496e+00*PETSC_i,
             -5.700953298671832e+00 - 3.210265600308496e+00*PETSC_i},
  m1q5[4] = { 5.203941240131764e+00 + 5.805856841805367e+00*PETSC_i,
              5.203941240131764e+00 - 5.805856841805367e+00*PETSC_i,
              6.796058759868242e+00 + 1.886649260140217e+00*PETSC_i,
              6.796058759868242e+00 - 1.886649260140217e+00*PETSC_i},
  m1p4[4] = {-3.212806896871536e+00 + 4.773087433276636e+00*PETSC_i,
             -3.212806896871536e+00 - 4.773087433276636e+00*PETSC_i,
             -4.787193103128464e+00 + 1.567476416895212e+00*PETSC_i,
             -4.787193103128464e+00 - 1.567476416895212e+00*PETSC_i},
  m1q4[3] = { 4.675757014491557e+00 + 3.913489560603711e+00*PETSC_i,
              4.675757014491557e+00 - 3.913489560603711e+00*PETSC_i,
              5.648485971016893e+00                                },
  m1p3[3] = {-2.681082873627756e+00 + 3.050430199247411e+00*PETSC_i,
             -2.681082873627756e+00 - 3.050430199247411e+00*PETSC_i,
             -3.637834252744491e+00                                },
  m1q3[2] = { 4.000000000000000e+00 + 2.000000000000000e+00*PETSC_i,
              4.000000000000000e+00 - 2.000000000000001e+00*PETSC_i},
  m1p2[2] = {-2.000000000000000e+00 + 1.414213562373095e+00*PETSC_i,
             -2.000000000000000e+00 - 1.414213562373095e+00*PETSC_i};

  PetscFunctionBegin;
  if (query) {
    if (m == k+1) {
      *mult = 1;
      *p = k;
      *q = k+1;
      PetscFunctionReturn(0);
    }
    if (m==k-1) {
      *mult = 1;
      *p = k;
      *q = k-1;
    }
  } else {
    if (m == k+1) {
      *mult = PetscPowInt(-1,m);
      *mult *= m;
      if (k==4) {
        for (i=0;i<4;i++) { p[i] = p1p4[i]; q[i] = p1q4[i]; }
        q[4] = p1q4[4];
      } else if (k==3) {
        for (i=0;i<3;i++) { p[i] = p1p3[i]; q[i] = p1q3[i]; }
        q[3] = p1q3[3];
      } else if (k==2) {
        for (i=0;i<2;i++) { p[i] = p1p2[i]; q[i] = p1q2[i]; }
        q[2] = p1q2[2];
      } else if (k==1) {
        p[0] = -3;
        for (i=0;i<2;i++) q[i] = p1q1[i];
      }
      PetscFunctionReturn(0);
    }
    if (m==k-1) {
      *mult = PetscPowInt(-1,m);
      *mult /= k;
      if (k==5) {
        for (i=0;i<4;i++) { p[i] = m1p5[i]; q[i] = m1q5[i]; }
        p[4] = m1p5[4];
      } else if (k==4) {
        for (i=0;i<3;i++) { p[i] = m1p4[i]; q[i] = m1q4[i]; }
        p[3] = m1p4[3];
      } else if (k==3) {
        for (i=0;i<2;i++) { p[i] = m1p3[i]; q[i] = m1q3[i]; }
        p[2] = m1p3[2];
      } else if (k==2) {
        for (i=0;i<2;i++) p[i] = m1p2[i];
        q[0] = 3;
      }
    }
  }
  PetscFunctionReturn(0);
}
#endif /* PETSC_HAVE_COMPLEX */

#if defined(PETSC_USE_COMPLEX)
static PetscErrorCode getisreal(PetscInt n,PetscComplex *a,PetscBool *result)
{
  PetscInt i;

  PetscFunctionBegin;
  *result=PETSC_TRUE;
  for (i=0;i<n&&*result;i++) {
    if (PetscImaginaryPartComplex(a[i])) *result=PETSC_FALSE;
  }
  PetscFunctionReturn(0);
}
#endif

/*
 * Matrix exponential implementation based on algorithm and matlab code by Stefan Guettel
 * and Yuji Nakatsukasa
 *
 *     Stefan Guettel and Yuji Nakatsukasa, "Scaled and Squared Subdiagonal Pade
 *     Approximation for the Matrix Exponential",
 *     SIAM J. Matrix Anal. Appl. 37(1):145-170, 2016.
 *     https://doi.org/10.1137/15M1027553
 */
PetscErrorCode FNEvaluateFunctionMat_Exp_GuettelNakatsukasa(FN fn,Mat A,Mat B)
{
#if !defined(PETSC_HAVE_COMPLEX)
  PetscFunctionBegin;
  SETERRQ(PETSC_COMM_SELF,1,"This function requires C99 or C++ complex support");
#else
  PetscInt          i,j,n_,s,k,m,mod;
  PetscBLASInt      n=0,n2=0,irsize=0,rsizediv2,ipsize=0,iremainsize=0,info,*piv,minlen,lwork=0,one=1;
  PetscReal         nrm,shift=0.0;
#if defined(PETSC_USE_COMPLEX) || defined(PETSC_HAVE_ESSL)
  PetscReal         *rwork=NULL;
#endif
  PetscComplex      *As,*RR,*RR2,*expmA,*expmA2,*Maux,*Maux2,rsize,*r,psize,*p,remainsize,*remainterm,*rootp,*rootq,mult=0.0,scale,cone=1.0,czero=0.0,*aux;
  PetscScalar       *Ba,*Ba2,*sMaux,*wr,*wi,expshift,sone=1.0,szero=0.0,*saux;
  const PetscScalar *Aa;
  PetscErrorCode    ierr;
  PetscBool         isreal,flg;
#if defined(PETSC_HAVE_ESSL)
  PetscScalar       sdummy,*wri;
  PetscBLASInt      idummy,io=0;
#else
  PetscBLASInt      query=-1;
  PetscScalar       work1,*work;
#endif

  PetscFunctionBegin;
  ierr = MatGetSize(A,&n_,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(n_,&n);CHKERRQ(ierr);
  ierr = MatDenseGetArrayRead(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  Ba2 = Ba;
  ierr = PetscBLASIntCast(n*n,&n2);CHKERRQ(ierr);

  ierr = PetscMalloc2(n2,&sMaux,n2,&Maux);CHKERRQ(ierr);
  Maux2 = Maux;
  ierr = PetscOptionsGetReal(NULL,NULL,"-fn_expm_estimated_eig",&shift,&flg);CHKERRQ(ierr);
  if (!flg) {
    ierr = PetscMalloc2(n,&wr,n,&wi);CHKERRQ(ierr);
    ierr = PetscArraycpy(sMaux,Aa,n2);CHKERRQ(ierr);
    /* estimate rightmost eigenvalue and shift A with it */
#if !defined(PETSC_HAVE_ESSL)
#if !defined(PETSC_USE_COMPLEX)
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_("N","N",&n,sMaux,&n,wr,wi,NULL,&n,NULL,&n,&work1,&query,&info));
    SlepcCheckLapackInfo("geev",info);
    ierr = PetscBLASIntCast((PetscInt)work1,&lwork);CHKERRQ(ierr);
    ierr = PetscMalloc1(lwork,&work);CHKERRQ(ierr);
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_("N","N",&n,sMaux,&n,wr,wi,NULL,&n,NULL,&n,work,&lwork,&info));
    ierr = PetscFree(work);CHKERRQ(ierr);
#else
    ierr = PetscArraycpy(Maux,Aa,n2);CHKERRQ(ierr);
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_("N","N",&n,Maux,&n,wr,NULL,&n,NULL,&n,&work1,&query,rwork,&info));
    SlepcCheckLapackInfo("geev",info);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPart(work1),&lwork);CHKERRQ(ierr);
    ierr = PetscMalloc2(2*n,&rwork,lwork,&work);CHKERRQ(ierr);
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_("N","N",&n,Maux,&n,wr,NULL,&n,NULL,&n,work,&lwork,rwork,&info));
    ierr = PetscFree2(rwork,work);CHKERRQ(ierr);
#endif
    SlepcCheckLapackInfo("geev",info);
#else /* defined(PETSC_HAVE_ESSL) */
    ierr = PetscBLASIntCast(4*n,&lwork);CHKERRQ(ierr);
    ierr = PetscMalloc2(lwork,&rwork,2*n,&wri);CHKERRQ(ierr);
#if !defined(PETSC_USE_COMPLEX)
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_(&io,sMaux,&n,wri,&sdummy,&idummy,&idummy,&n,rwork,&lwork));
    for (i=0;i<n;i++) {
      wr[i] = wri[2*i];
      wi[i] = wri[2*i+1];
    }
#else
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_(&io,Maux,&n,wri,&sdummy,&idummy,&idummy,&n,rwork,&lwork));
    for (i=0;i<n;i++) wr[i] = wri[i];
#endif
    ierr = PetscFree2(rwork,wri);CHKERRQ(ierr);
#endif
    ierr = PetscLogFlops(25.0*n*n*n+(n*n*n)/3.0+1.0*n*n*n);CHKERRQ(ierr);

    shift = PetscRealPart(wr[0]);
    for (i=1;i<n;i++) {
      if (PetscRealPart(wr[i]) > shift) shift = PetscRealPart(wr[i]);
    }
    ierr = PetscFree2(wr,wi);CHKERRQ(ierr);
  }
  /* shift so that largest real part is (about) 0 */
  ierr = PetscArraycpy(sMaux,Aa,n2);CHKERRQ(ierr);
  if (shift) {
    for (i=0;i<n;i++) sMaux[i+i*n] -= shift;
    ierr = PetscLogFlops(1.0*n);CHKERRQ(ierr);
  }
#if defined(PETSC_USE_COMPLEX)
  ierr = PetscArraycpy(Maux,Aa,n2);CHKERRQ(ierr);
  if (shift) {
    for (i=0;i<n;i++) Maux[i+i*n] -= shift;
    ierr = PetscLogFlops(1.0*n);CHKERRQ(ierr);
  }
#endif

  /* estimate norm(A) and select the scaling factor */
  nrm = LAPACKlange_("O",&n,&n,sMaux,&n,NULL);
  ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);
  ierr = sexpm_params(nrm,&s,&k,&m);CHKERRQ(ierr);
  if (s==0 && k==1 && m==0) { /* exp(A) = I+A to eps! */
    if (shift) expshift = PetscExpReal(shift);
    for (i=0;i<n;i++) sMaux[i+i*n] += 1.0;
    if (shift) {
      PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&expshift,sMaux,&one));
      ierr = PetscLogFlops(1.0*(n+n2));CHKERRQ(ierr);
    } else {
      ierr = PetscLogFlops(1.0*n);CHKERRQ(ierr);
    }
    ierr = PetscArraycpy(Ba,sMaux,n2);CHKERRQ(ierr);
    ierr = PetscFree2(sMaux,Maux);CHKERRQ(ierr);
    ierr = MatDenseRestoreArrayRead(A,&Aa);CHKERRQ(ierr);
    ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
    PetscFunctionReturn(0); /* quick return */
  }

  ierr = PetscMalloc4(n2,&expmA,n2,&As,n2,&RR,n,&piv);CHKERRQ(ierr);
  expmA2 = expmA; RR2 = RR;
  /* scale matrix */
#if !defined(PETSC_USE_COMPLEX)
  for (i=0;i<n2;i++) {
    As[i] = sMaux[i];
  }
#else
  ierr = PetscArraycpy(As,sMaux,n2);CHKERRQ(ierr);
#endif
  scale = 1.0/PetscPowRealInt(2.0,s);
  PetscStackCallBLAS("BLASCOMPLEXscal",BLASCOMPLEXscal_(&n2,&scale,As,&one));
  ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);

  /* evaluate Pade approximant (partial fraction or product form) */
  if (fn->method==3 || !m) { /* partial fraction */
    ierr = getcoeffs(k,m,&rsize,&psize,&remainsize,PETSC_TRUE);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(rsize),&irsize);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(psize),&ipsize);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(remainsize),&iremainsize);CHKERRQ(ierr);
    ierr = PetscMalloc3(irsize,&r,ipsize,&p,iremainsize,&remainterm);CHKERRQ(ierr);
    ierr = getcoeffs(k,m,r,p,remainterm,PETSC_FALSE);CHKERRQ(ierr);

    ierr = PetscArrayzero(expmA,n2);CHKERRQ(ierr);
#if !defined(PETSC_USE_COMPLEX)
    isreal = PETSC_TRUE;
#else
    ierr = getisreal(n2,Maux,&isreal);CHKERRQ(ierr);
#endif
    if (isreal) {
      rsizediv2 = irsize/2;
      for (i=0;i<rsizediv2;i++) { /* use partial fraction to get R(As) */
        ierr = PetscArraycpy(Maux,As,n2);CHKERRQ(ierr);
        ierr = PetscArrayzero(RR,n2);CHKERRQ(ierr);
        for (j=0;j<n;j++) {
          Maux[j+j*n] -= p[2*i];
          RR[j+j*n] = r[2*i];
        }
        PetscStackCallBLAS("LAPACKCOMPLEXgesv",LAPACKCOMPLEXgesv_(&n,&n,Maux,&n,piv,RR,&n,&info));
        SlepcCheckLapackInfo("gesv",info);
        for (j=0;j<n2;j++) {
          expmA[j] += RR[j] + PetscConj(RR[j]);
        }
        /* loop(n) + gesv + loop(n2) */
        ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n)+2.0*n2);CHKERRQ(ierr);
      }

      mod = ipsize % 2;
      if (mod) {
        ierr = PetscArraycpy(Maux,As,n2);CHKERRQ(ierr);
        ierr = PetscArrayzero(RR,n2);CHKERRQ(ierr);
        for (j=0;j<n;j++) {
          Maux[j+j*n] -= p[ipsize-1];
          RR[j+j*n] = r[irsize-1];
        }
        PetscStackCallBLAS("LAPACKCOMPLEXgesv",LAPACKCOMPLEXgesv_(&n,&n,Maux,&n,piv,RR,&n,&info));
        SlepcCheckLapackInfo("gesv",info);
        for (j=0;j<n2;j++) {
          expmA[j] += RR[j];
        }
        ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n)+1.0*n2);CHKERRQ(ierr);
      }
    } else { /* complex */
      for (i=0;i<irsize;i++) { /* use partial fraction to get R(As) */
        ierr = PetscArraycpy(Maux,As,n2);CHKERRQ(ierr);
        ierr = PetscArrayzero(RR,n2);CHKERRQ(ierr);
        for (j=0;j<n;j++) {
          Maux[j+j*n] -= p[i];
          RR[j+j*n] = r[i];
        }
        PetscStackCallBLAS("LAPACKCOMPLEXgesv",LAPACKCOMPLEXgesv_(&n,&n,Maux,&n,piv,RR,&n,&info));
        SlepcCheckLapackInfo("gesv",info);
        for (j=0;j<n2;j++) {
          expmA[j] += RR[j];
        }
        ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n)+1.0*n2);CHKERRQ(ierr);
      }
    }
    for (i=0;i<iremainsize;i++) {
      if (!i) {
        ierr = PetscArrayzero(RR,n2);CHKERRQ(ierr);
        for (j=0;j<n;j++) {
          RR[j+j*n] = remainterm[iremainsize-1];
        }
      } else {
        ierr = PetscArraycpy(RR,As,n2);CHKERRQ(ierr);
        for (j=1;j<i;j++) {
          PetscStackCallBLAS("BLASCOMPLEXgemm",BLASCOMPLEXgemm_("N","N",&n,&n,&n,&cone,RR,&n,RR,&n,&czero,Maux,&n));
          SWAP(RR,Maux,aux);
          ierr = SlepcLogFlopsComplex(2.0*n*n*n);CHKERRQ(ierr);
        }
        PetscStackCallBLAS("BLASCOMPLEXscal",BLASCOMPLEXscal_(&n2,&remainterm[iremainsize-1-i],RR,&one));
        ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);
      }
      for (j=0;j<n2;j++) {
        expmA[j] += RR[j];
      }
      ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);
    }
    ierr = PetscFree3(r,p,remainterm);CHKERRQ(ierr);
  } else { /* product form, default */
    ierr = getcoeffsproduct(k,m,&rsize,&psize,&mult,PETSC_TRUE);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(rsize),&irsize);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(psize),&ipsize);CHKERRQ(ierr);
    ierr = PetscMalloc2(irsize,&rootp,ipsize,&rootq);CHKERRQ(ierr);
    ierr = getcoeffsproduct(k,m,rootp,rootq,&mult,PETSC_FALSE);CHKERRQ(ierr);

    ierr = PetscArrayzero(expmA,n2);CHKERRQ(ierr);
    for (i=0;i<n;i++) { /* initialize */
      expmA[i+i*n] = 1.0;
    }
    minlen = PetscMin(irsize,ipsize);
    for (i=0;i<minlen;i++) {
      ierr = PetscArraycpy(RR,As,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) {
        RR[j+j*n] -= rootp[i];
      }
      PetscStackCallBLAS("BLASCOMPLEXgemm",BLASCOMPLEXgemm_("N","N",&n,&n,&n,&cone,RR,&n,expmA,&n,&czero,Maux,&n));
      SWAP(expmA,Maux,aux);
      ierr = PetscArraycpy(RR,As,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) {
        RR[j+j*n] -= rootq[i];
      }
      PetscStackCallBLAS("LAPACKCOMPLEXgesv",LAPACKCOMPLEXgesv_(&n,&n,RR,&n,piv,expmA,&n,&info));
      SlepcCheckLapackInfo("gesv",info);
      /* loop(n) + gemm + loop(n) + gesv */
      ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n)+1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n));CHKERRQ(ierr);
    }
    /* extra numerator */
    for (i=minlen;i<irsize;i++) {
      ierr = PetscArraycpy(RR,As,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) {
        RR[j+j*n] -= rootp[i];
      }
      PetscStackCallBLAS("BLASCOMPLEXgemm",BLASCOMPLEXgemm_("N","N",&n,&n,&n,&cone,RR,&n,expmA,&n,&czero,Maux,&n));
      SWAP(expmA,Maux,aux);
      ierr = SlepcLogFlopsComplex(1.0*n+2.0*n*n*n);CHKERRQ(ierr);
    }
    /* extra denominator */
    for (i=minlen;i<ipsize;i++) {
      ierr = PetscArraycpy(RR,As,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) RR[j+j*n] -= rootq[i];
      PetscStackCallBLAS("LAPACKCOMPLEXgesv",LAPACKCOMPLEXgesv_(&n,&n,RR,&n,piv,expmA,&n,&info));
      SlepcCheckLapackInfo("gesv",info);
      ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n));CHKERRQ(ierr);
    }
    PetscStackCallBLAS("BLASCOMPLEXscal",BLASCOMPLEXscal_(&n2,&mult,expmA,&one));
    ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);
    ierr = PetscFree2(rootp,rootq);CHKERRQ(ierr);
  }

#if !defined(PETSC_USE_COMPLEX)
  for (i=0;i<n2;i++) {
    Ba2[i] = PetscRealPartComplex(expmA[i]);
  }
#else
  ierr = PetscArraycpy(Ba2,expmA,n2);CHKERRQ(ierr);
#endif

  /* perform repeated squaring */
  for (i=0;i<s;i++) { /* final squaring */
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&n,&n,&sone,Ba2,&n,Ba2,&n,&szero,sMaux,&n));
    SWAP(Ba2,sMaux,saux);
    ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  }
  if (Ba2!=Ba) {
    ierr = PetscArraycpy(Ba,Ba2,n2);CHKERRQ(ierr);
    sMaux = Ba2;
  }
  if (shift) {
    expshift = PetscExpReal(shift);
    PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&expshift,Ba,&one));
    ierr = PetscLogFlops(1.0*n2);CHKERRQ(ierr);
  }

  /* restore pointers */
  Maux = Maux2; expmA = expmA2; RR = RR2;
  ierr = PetscFree2(sMaux,Maux);CHKERRQ(ierr);
  ierr = PetscFree4(expmA,As,RR,piv);CHKERRQ(ierr);
  ierr = MatDenseRestoreArrayRead(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  PetscFunctionReturn(0);
#endif
}

#define SMALLN 100

/*
 * Function needed to compute optimal parameters (required workspace is 3*n*n)
 */
static PetscInt ell(PetscBLASInt n,PetscScalar *A,PetscReal coeff,PetscInt m,PetscScalar *work,PetscRandom rand)
{
  PetscScalar    *Ascaled=work;
  PetscReal      nrm,alpha,beta,rwork[1];
  PetscInt       t;
  PetscBLASInt   i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  beta = PetscPowReal(coeff,1.0/(2*m+1));
  for (i=0;i<n;i++)
    for (j=0;j<n;j++)
      Ascaled[i+j*n] = beta*PetscAbsScalar(A[i+j*n]);
  nrm = LAPACKlange_("O",&n,&n,A,&n,rwork);
  ierr = PetscLogFlops(2.0*n*n);CHKERRQ(ierr);
  ierr = SlepcNormAm(n,Ascaled,2*m+1,work+n*n,rand,&alpha);CHKERRQ(ierr);
  alpha /= nrm;
  t = PetscMax((PetscInt)PetscCeilReal(PetscLogReal(2.0*alpha/PETSC_MACHINE_EPSILON)/PetscLogReal(2.0)/(2*m)),0);
  PetscFunctionReturn(t);
}

/*
 * Compute scaling parameter (s) and order of Pade approximant (m)  (required workspace is 4*n*n)
 */
static PetscErrorCode expm_params(PetscInt n,PetscScalar **Apowers,PetscInt *s,PetscInt *m,PetscScalar *work)
{
  PetscErrorCode  ierr;
  PetscScalar     sfactor,sone=1.0,szero=0.0,*A=Apowers[0],*Ascaled;
  PetscReal       d4,d6,d8,d10,eta1,eta3,eta4,eta5,rwork[1];
  PetscBLASInt    n_=0,n2,one=1;
  PetscRandom     rand;
  const PetscReal coeff[5] = { 9.92063492063492e-06, 9.94131285136576e-11,  /* backward error function */
                               2.22819456055356e-16, 1.69079293431187e-22, 8.82996160201868e-36 };
  const PetscReal theta[5] = { 1.495585217958292e-002,    /* m = 3  */
                               2.539398330063230e-001,    /* m = 5  */
                               9.504178996162932e-001,    /* m = 7  */
                               2.097847961257068e+000,    /* m = 9  */
                               5.371920351148152e+000 };  /* m = 13 */

  PetscFunctionBegin;
  *s = 0;
  *m = 13;
  ierr = PetscBLASIntCast(n,&n_);CHKERRQ(ierr);
  ierr = PetscRandomCreate(PETSC_COMM_SELF,&rand);CHKERRQ(ierr);
  d4 = PetscPowReal(LAPACKlange_("O",&n_,&n_,Apowers[2],&n_,rwork),1.0/4.0);
  if (d4==0.0) { /* safeguard for the case A = 0 */
    *m = 3;
    goto done;
  }
  d6 = PetscPowReal(LAPACKlange_("O",&n_,&n_,Apowers[3],&n_,rwork),1.0/6.0);
  ierr = PetscLogFlops(2.0*n*n);CHKERRQ(ierr);
  eta1 = PetscMax(d4,d6);
  if (eta1<=theta[0] && !ell(n_,A,coeff[0],3,work,rand)) {
    *m = 3;
    goto done;
  }
  if (eta1<=theta[1] && !ell(n_,A,coeff[1],5,work,rand)) {
    *m = 5;
    goto done;
  }
  if (n<SMALLN) {
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[2],&n_,Apowers[2],&n_,&szero,work,&n_));
    d8 = PetscPowReal(LAPACKlange_("O",&n_,&n_,work,&n_,rwork),1.0/8.0);
    ierr = PetscLogFlops(2.0*n*n*n+1.0*n*n);CHKERRQ(ierr);
  } else {
    ierr = SlepcNormAm(n_,Apowers[2],2,work,rand,&d8);CHKERRQ(ierr);
    d8 = PetscPowReal(d8,1.0/8.0);
  }
  eta3 = PetscMax(d6,d8);
  if (eta3<=theta[2] && !ell(n_,A,coeff[2],7,work,rand)) {
    *m = 7;
    goto done;
  }
  if (eta3<=theta[3] && !ell(n_,A,coeff[3],9,work,rand)) {
    *m = 9;
    goto done;
  }
  if (n<SMALLN) {
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[2],&n_,Apowers[3],&n_,&szero,work,&n_));
    d10 = PetscPowReal(LAPACKlange_("O",&n_,&n_,work,&n_,rwork),1.0/10.0);
    ierr = PetscLogFlops(2.0*n*n*n+1.0*n*n);CHKERRQ(ierr);
  } else {
    ierr = SlepcNormAm(n_,Apowers[1],5,work,rand,&d10);CHKERRQ(ierr);
    d10 = PetscPowReal(d10,1.0/10.0);
  }
  eta4 = PetscMax(d8,d10);
  eta5 = PetscMin(eta3,eta4);
  *s = PetscMax((PetscInt)PetscCeilReal(PetscLogReal(eta5/theta[4])/PetscLogReal(2.0)),0);
  if (*s) {
    Ascaled = work+3*n*n;
    n2 = n_*n_;
    PetscStackCallBLAS("BLAScopy",BLAScopy_(&n2,A,&one,Ascaled,&one));
    sfactor = PetscPowRealInt(2.0,-(*s));
    PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&sfactor,Ascaled,&one));
    ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);
  } else Ascaled = A;
  *s += ell(n_,Ascaled,coeff[4],13,work,rand);
done:
  ierr = PetscRandomDestroy(&rand);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
 * Matrix exponential implementation based on algorithm and matlab code by N. Higham and co-authors
 *
 *     N. J. Higham, "The scaling and squaring method for the matrix exponential
 *     revisited", SIAM J. Matrix Anal. Appl. 26(4):1179-1193, 2005.
 */
PetscErrorCode FNEvaluateFunctionMat_Exp_Higham(FN fn,Mat A,Mat B)
{
  PetscErrorCode    ierr;
  PetscBLASInt      n_=0,n2,*ipiv,info,one=1;
  PetscInt          n,m,j,s;
  PetscScalar       scale,smone=-1.0,sone=1.0,stwo=2.0,szero=0.0;
  PetscScalar       *Ba,*Apowers[5],*Q,*P,*W,*work,*aux;
  const PetscScalar *Aa,*c;
  const PetscScalar c3[4]   = { 120, 60, 12, 1 };
  const PetscScalar c5[6]   = { 30240, 15120, 3360, 420, 30, 1 };
  const PetscScalar c7[8]   = { 17297280, 8648640, 1995840, 277200, 25200, 1512, 56, 1 };
  const PetscScalar c9[10]  = { 17643225600.0, 8821612800.0, 2075673600, 302702400, 30270240,
                                2162160, 110880, 3960, 90, 1 };
  const PetscScalar c13[14] = { 64764752532480000.0, 32382376266240000.0, 7771770303897600.0,
                                1187353796428800.0,  129060195264000.0,   10559470521600.0,
                                670442572800.0,      33522128640.0,       1323241920.0,
                                40840800,          960960,            16380,  182,  1 };

  PetscFunctionBegin;
  ierr = MatDenseGetArrayRead(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  ierr = MatGetSize(A,&n,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(n,&n_);CHKERRQ(ierr);
  n2 = n_*n_;
  ierr = PetscMalloc2(8*n*n,&work,n,&ipiv);CHKERRQ(ierr);

  /* Matrix powers */
  Apowers[0] = work;                  /* Apowers[0] = A   */
  Apowers[1] = Apowers[0] + n*n;      /* Apowers[1] = A^2 */
  Apowers[2] = Apowers[1] + n*n;      /* Apowers[2] = A^4 */
  Apowers[3] = Apowers[2] + n*n;      /* Apowers[3] = A^6 */
  Apowers[4] = Apowers[3] + n*n;      /* Apowers[4] = A^8 */

  ierr = PetscArraycpy(Apowers[0],Aa,n2);CHKERRQ(ierr);
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[0],&n_,Apowers[0],&n_,&szero,Apowers[1],&n_));
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[1],&n_,Apowers[1],&n_,&szero,Apowers[2],&n_));
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[1],&n_,Apowers[2],&n_,&szero,Apowers[3],&n_));
  ierr = PetscLogFlops(6.0*n*n*n);CHKERRQ(ierr);

  /* Compute scaling parameter and order of Pade approximant */
  ierr = expm_params(n,Apowers,&s,&m,Apowers[4]);CHKERRQ(ierr);

  if (s) { /* rescale */
    for (j=0;j<4;j++) {
      scale = PetscPowRealInt(2.0,-PetscMax(2*j,1)*s);
      PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&scale,Apowers[j],&one));
    }
    ierr = PetscLogFlops(4.0*n*n);CHKERRQ(ierr);
  }

  /* Evaluate the Pade approximant */
  switch (m) {
    case 3:  c = c3;  break;
    case 5:  c = c5;  break;
    case 7:  c = c7;  break;
    case 9:  c = c9;  break;
    case 13: c = c13; break;
    default: SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Wrong value of m %d",m);
  }
  P = Ba;
  Q = Apowers[4] + n*n;
  W = Q + n*n;
  switch (m) {
    case 3:
    case 5:
    case 7:
    case 9:
      if (m==9) PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[1],&n_,Apowers[3],&n_,&szero,Apowers[4],&n_));
      ierr = PetscArrayzero(P,n2);CHKERRQ(ierr);
      ierr = PetscArrayzero(Q,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) {
        P[j+j*n] = c[1];
        Q[j+j*n] = c[0];
      }
      for (j=m;j>=3;j-=2) {
        PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[j],Apowers[(j+1)/2-1],&one,P,&one));
        PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[j-1],Apowers[(j+1)/2-1],&one,Q,&one));
        ierr = PetscLogFlops(4.0*n*n);CHKERRQ(ierr);
      }
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[0],&n_,P,&n_,&szero,W,&n_));
      ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
      SWAP(P,W,aux);
      break;
    case 13:
      /*  P = A*(Apowers[3]*(c[13]*Apowers[3] + c[11]*Apowers[2] + c[9]*Apowers[1])
              + c[7]*Apowers[3] + c[5]*Apowers[2] + c[3]*Apowers[1] + c[1]*I)       */
      PetscStackCallBLAS("BLAScopy",BLAScopy_(&n2,Apowers[3],&one,P,&one));
      PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&c[13],P,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[11],Apowers[2],&one,P,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[9],Apowers[1],&one,P,&one));
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[3],&n_,P,&n_,&szero,W,&n_));
      ierr = PetscLogFlops(5.0*n*n+2.0*n*n*n);CHKERRQ(ierr);
      ierr = PetscArrayzero(P,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) P[j+j*n] = c[1];
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[7],Apowers[3],&one,P,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[5],Apowers[2],&one,P,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[3],Apowers[1],&one,P,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&sone,P,&one,W,&one));
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[0],&n_,W,&n_,&szero,P,&n_));
      ierr = PetscLogFlops(7.0*n*n+2.0*n*n*n);CHKERRQ(ierr);
      /*  Q = Apowers[3]*(c[12]*Apowers[3] + c[10]*Apowers[2] + c[8]*Apowers[1])
              + c[6]*Apowers[3] + c[4]*Apowers[2] + c[2]*Apowers[1] + c[0]*I        */
      PetscStackCallBLAS("BLAScopy",BLAScopy_(&n2,Apowers[3],&one,Q,&one));
      PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&c[12],Q,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[10],Apowers[2],&one,Q,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[8],Apowers[1],&one,Q,&one));
      PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,Apowers[3],&n_,Q,&n_,&szero,W,&n_));
      ierr = PetscLogFlops(5.0*n*n+2.0*n*n*n);CHKERRQ(ierr);
      ierr = PetscArrayzero(Q,n2);CHKERRQ(ierr);
      for (j=0;j<n;j++) Q[j+j*n] = c[0];
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[6],Apowers[3],&one,Q,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[4],Apowers[2],&one,Q,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&c[2],Apowers[1],&one,Q,&one));
      PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&sone,W,&one,Q,&one));
      ierr = PetscLogFlops(7.0*n*n);CHKERRQ(ierr);
      break;
    default: SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Wrong value of m %d",m);
  }
  PetscStackCallBLAS("BLASaxpy",BLASaxpy_(&n2,&smone,P,&one,Q,&one));
  PetscStackCallBLAS("LAPACKgesv",LAPACKgesv_(&n_,&n_,Q,&n_,ipiv,P,&n_,&info));
  SlepcCheckLapackInfo("gesv",info);
  PetscStackCallBLAS("BLASscal",BLASscal_(&n2,&stwo,P,&one));
  for (j=0;j<n;j++) P[j+j*n] += 1.0;
  ierr = PetscLogFlops(2.0*n*n*n/3.0+4.0*n*n);CHKERRQ(ierr);

  /* Squaring */
  for (j=1;j<=s;j++) {
    PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n_,&n_,&n_,&sone,P,&n_,P,&n_,&szero,W,&n_));
    SWAP(P,W,aux);
  }
  if (P!=Ba) { ierr = PetscArraycpy(Ba,P,n2);CHKERRQ(ierr); }
  ierr = PetscLogFlops(2.0*n*n*n*s);CHKERRQ(ierr);

  ierr = PetscFree2(work,ipiv);CHKERRQ(ierr);
  ierr = MatDenseRestoreArrayRead(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_CUDA)
#include "../src/sys/classes/fn/impls/cuda/fnutilcuda.h"
#include <slepccublas.h>

PetscErrorCode FNEvaluateFunctionMat_Exp_Pade_CUDA(FN fn,Mat A,Mat B)
{
  PetscErrorCode ierr;
  PetscBLASInt   n=0,ld,ld2,*d_ipiv,*d_info,info,one=1,zero=0;
  PetscInt       m,k,sexp;
  PetscBool      odd;
  const PetscInt p=MAX_PADE;
  PetscReal      c[MAX_PADE+1],s;
  PetscScalar    scale,smone=-1.0,sone=1.0,stwo=2.0,szero=0.0;
  PetscScalar    *Aa,*Ba;
  PetscScalar    *d_Ba,*d_As,*d_A2,*d_Q,*d_P,*d_W,*aux,**ppP,**d_ppP,**ppQ,**d_ppQ;
  cublasHandle_t cublasv2handle;
  cublasStatus_t cberr;
  cudaError_t    cerr;

  PetscFunctionBegin;
  ierr = PetscCUBLASGetHandle(&cublasv2handle);CHKERRQ(ierr);
  ierr = MatDenseGetArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  ierr = MatGetSize(A,&m,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(m,&n);CHKERRQ(ierr);
  ld  = n;
  ld2 = ld*ld;

  cerr = cudaMalloc((void **)&d_Ba,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_Q,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_W,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_As,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_A2,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_ipiv,sizeof(PetscBLASInt)*ld);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_info,sizeof(PetscBLASInt));CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_ppP,sizeof(PetscScalar *));CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_ppQ,sizeof(PetscScalar *));CHKERRCUDA(cerr);

  ierr = PetscMalloc(sizeof(PetscScalar *),&ppP);CHKERRQ(ierr);
  ierr = PetscMalloc(sizeof(PetscScalar *),&ppQ);CHKERRQ(ierr);

  cerr = cudaMemcpy(d_As,Aa,sizeof(PetscScalar)*ld2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  cerr = cudaMemcpy(d_Ba,Ba,sizeof(PetscScalar)*ld2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  d_P = d_Ba;

  /* Pade' coefficients */
  c[0] = 1.0;
  for (k=1;k<=p;k++) c[k] = c[k-1]*(p+1-k)/(k*(2*p+1-k));

  /* Scaling */
  cberr = cublasXnrm2(cublasv2handle,ld2,d_As,one,&s);CHKERRCUBLAS(cberr);
  if (s>0.5) {
    sexp = PetscMax(0,(int)(PetscLogReal(s)/PetscLogReal(2.0))+2);
    scale = PetscPowRealInt(2.0,-sexp);
    cberr = cublasXscal(cublasv2handle,ld2,&scale,d_As,one);CHKERRCUBLAS(cberr);
    ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);
  } else sexp = 0;

  /* Horner evaluation */
  cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_As,ld,d_As,ld,&szero,d_A2,ld);CHKERRCUBLAS(cberr);
  ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  cerr = cudaMemset(d_Q,zero,sizeof(PetscScalar)*ld2);CHKERRCUDA(cerr);
  cerr = cudaMemset(d_P,zero,sizeof(PetscScalar)*ld2);CHKERRCUDA(cerr);
  ierr = set_diagonal(n,d_Q,ld,c[p]);CHKERRQ(cerr);
  ierr = set_diagonal(n,d_P,ld,c[p-1]);CHKERRQ(cerr);

  odd = PETSC_TRUE;
  for (k=p-1;k>0;k--) {
    if (odd) {
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_Q,ld,d_A2,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
      SWAP(d_Q,d_W,aux);
      ierr = shift_diagonal(n,d_Q,ld,c[k-1]);CHKERRQ(cerr);
      odd = PETSC_FALSE;
    } else {
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_P,ld,d_A2,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
      SWAP(d_P,d_W,aux);
      ierr = shift_diagonal(n,d_P,ld,c[k-1]);CHKERRQ(cerr);
      odd = PETSC_TRUE;
    }
    ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  }
  if (odd) {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_Q,ld,d_As,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
    SWAP(d_Q,d_W,aux);
    cberr = cublasXaxpy(cublasv2handle,ld2,&smone,d_P,one,d_Q,one);CHKERRCUBLAS(cberr);

    ppQ[0] = d_Q;
    ppP[0] = d_P;
    cerr = cudaMemcpy(d_ppQ,ppQ,sizeof(PetscScalar *),cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
    cerr = cudaMemcpy(d_ppP,ppP,sizeof(PetscScalar *),cudaMemcpyHostToDevice);CHKERRCUDA(cerr);

    cberr = cublasXgetrfBatched(cublasv2handle,n,d_ppQ,ld,d_ipiv,d_info,one);CHKERRCUBLAS(cberr);
    cerr = cudaMemcpy(&info,d_info,sizeof(PetscBLASInt),cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
    if (info < 0) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"LAPACKgetrf: Illegal value on argument %d",PetscAbsInt(info));
    if (info > 0) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_MAT_LU_ZRPVT,"LAPACKgetrf: Matrix is singular. U(%d,%d) is zero",info,info);
#if defined (CUDA_VERSION) && CUDA_VERSION >= 5050
    cberr = cublasXgetrsBatched(cublasv2handle,CUBLAS_OP_N,n,n,(const PetscScalar **)d_ppQ,ld,d_ipiv,d_ppP,ld,&info,one);CHKERRCUBLAS(cberr);
    if (info < 0) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"LAPACKgetri: Illegal value on argument %d",PetscAbsInt(info));
    if (info > 0) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_MAT_LU_ZRPVT,"LAPACKgetri: Matrix is singular. U(%d,%d) is zero",info,info);
#else
    SETERRQ(communicator,PETSC_ERR_LIB,"cublasXgetrsBatched needs CUDA >= 7");
#endif
    cberr = cublasXscal(cublasv2handle,ld2,&stwo,d_P,one);CHKERRCUBLAS(cberr);
    ierr = shift_diagonal(n,d_P,ld,sone);CHKERRQ(cerr);
    cberr = cublasXscal(cublasv2handle,ld2,&smone,d_P,one);CHKERRCUBLAS(cberr);
  } else {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_P,ld,d_As,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
    SWAP(d_P,d_W,aux);
    cberr = cublasXaxpy(cublasv2handle,ld2,&smone,d_P,one,d_Q,one);CHKERRCUBLAS(cberr);

    ppQ[0] = d_Q;
    ppP[0] = d_P;
    cerr = cudaMemcpy(d_ppQ,ppQ,sizeof(PetscScalar *),cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
    cerr = cudaMemcpy(d_ppP,ppP,sizeof(PetscScalar *),cudaMemcpyHostToDevice);CHKERRCUDA(cerr);

    cberr = cublasXgetrfBatched(cublasv2handle,n,d_ppQ,ld,d_ipiv,d_info,one);CHKERRCUBLAS(cberr);
    cerr = cudaMemcpy(&info,d_info,sizeof(PetscBLASInt),cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
    if (info < 0) SETERRQ1(PETSC_COMM_SELF, PETSC_ERR_LIB, "LAPACKgetrf: Illegal value on argument %d",PetscAbsInt(info));
    if (info > 0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_MAT_LU_ZRPVT, "LAPACKgetrf: Matrix is singular. U(%d,%d) is zero",info,info);
#if defined (CUDA_VERSION) && CUDA_VERSION >= 5050
    cberr = cublasXgetrsBatched(cublasv2handle,CUBLAS_OP_N,n,n,(const PetscScalar **)d_ppQ,ld,d_ipiv,d_ppP,ld,&info,one);CHKERRCUBLAS(cberr);
    if (info < 0) SETERRQ1(PETSC_COMM_SELF, PETSC_ERR_LIB, "LAPACKgetri: Illegal value on argument %d",PetscAbsInt(info));
    if (info > 0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_MAT_LU_ZRPVT, "LAPACKgetri: Matrix is singular. U(%d,%d) is zero",info,info);
#else
    SETERRQ(communicator,PETSC_ERR_LIB,"cublasXgetrsBatched needs CUDA >= 7");
#endif
    cberr = cublasXscal(cublasv2handle,ld2,&stwo,d_P,one);CHKERRCUBLAS(cberr);
    ierr = shift_diagonal(n,d_P,ld,sone);CHKERRQ(cerr);
  }
  ierr = PetscLogFlops(2.0*n*n*n+2.0*n*n*n/3.0+4.0*n*n);CHKERRQ(ierr);

  for (k=1;k<=sexp;k++) {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_P,ld,d_P,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
    cerr = cudaMemcpy(d_P,d_W,sizeof(PetscScalar)*ld2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
  }
  if (d_P!=d_Ba) {
    cerr = cudaMemcpy(Ba,d_P,sizeof(PetscScalar)*ld2,cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
  } else {
    cerr = cudaMemcpy(Ba,d_Ba,sizeof(PetscScalar)*ld2,cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
  }
  ierr = PetscLogFlops(2.0*n*n*n*sexp);CHKERRQ(ierr);

  cerr = cudaFree(d_Ba);CHKERRCUDA(cerr);
  cerr = cudaFree(d_Q);CHKERRCUDA(cerr);
  cerr = cudaFree(d_W);CHKERRCUDA(cerr);
  cerr = cudaFree(d_As);CHKERRCUDA(cerr);
  cerr = cudaFree(d_A2);CHKERRCUDA(cerr);
  cerr = cudaFree(d_ipiv);CHKERRCUDA(cerr);
  cerr = cudaFree(d_info);CHKERRCUDA(cerr);
  cerr = cudaFree(d_ppP);CHKERRCUDA(cerr);
  cerr = cudaFree(d_ppQ);CHKERRCUDA(cerr);

  ierr = PetscFree(ppP);CHKERRQ(ierr);
  ierr = PetscFree(ppQ);CHKERRQ(ierr);

  ierr = MatDenseRestoreArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_MAGMA)
#include <slepcmagma.h>

PetscErrorCode FNEvaluateFunctionMat_Exp_Pade_CUDAm(FN fn,Mat A,Mat B)
{
  PetscErrorCode ierr;
  PetscBLASInt   n=0,ld,ld2,*piv,info,one=1,zero=0;
  PetscInt       m,k,sexp;
  PetscBool      odd;
  const PetscInt p=MAX_PADE;
  PetscReal      c[MAX_PADE+1],s;
  PetscScalar    scale,smone=-1.0,sone=1.0,stwo=2.0,szero=0.0;
  PetscScalar    *Aa,*Ba;
  PetscScalar    *d_Ba,*d_As,*d_A2,*d_Q,*d_P,*d_W,*aux;
  cublasHandle_t cublasv2handle;
  cublasStatus_t cberr;
  cudaError_t    cerr;
  magma_int_t    mierr;

  PetscFunctionBegin;
  ierr = PetscCUBLASGetHandle(&cublasv2handle);CHKERRQ(ierr);
  magma_init();
  ierr = MatDenseGetArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  ierr = MatGetSize(A,&m,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(m,&n);CHKERRQ(ierr);
  ld  = n;
  ld2 = ld*ld;

  cerr = cudaMalloc((void **)&d_Ba,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_Q,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_W,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_As,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_A2,sizeof(PetscScalar)*m*m);CHKERRCUDA(cerr);

  ierr = PetscMalloc(sizeof(PetscInt)*n,&piv);CHKERRQ(ierr);

  cerr = cudaMemcpy(d_As,Aa,sizeof(PetscScalar)*ld2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  cerr = cudaMemcpy(d_Ba,Ba,sizeof(PetscScalar)*ld2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  d_P = d_Ba;

  /* Pade' coefficients */
  c[0] = 1.0;
  for (k=1;k<=p;k++) c[k] = c[k-1]*(p+1-k)/(k*(2*p+1-k));

  /* Scaling */
  cberr = cublasXnrm2(cublasv2handle,ld2,d_As,one,&s);CHKERRCUBLAS(cberr);
  ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);

  if (s>0.5) {
    sexp = PetscMax(0,(int)(PetscLogReal(s)/PetscLogReal(2.0))+2);
    scale = PetscPowRealInt(2.0,-sexp);
    cberr = cublasXscal(cublasv2handle,ld2,&scale,d_As,one);CHKERRCUBLAS(cberr);
    ierr = PetscLogFlops(1.0*n*n);CHKERRQ(ierr);
  } else sexp = 0;

  /* Horner evaluation */
  cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_As,ld,d_As,ld,&szero,d_A2,ld);CHKERRCUBLAS(cberr);
  ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  cerr = cudaMemset(d_Q,zero,sizeof(PetscScalar)*ld2);CHKERRCUDA(cerr);
  cerr = cudaMemset(d_P,zero,sizeof(PetscScalar)*ld2);CHKERRCUDA(cerr);
  ierr = set_diagonal(n,d_Q,ld,c[p]);CHKERRQ(cerr);
  ierr = set_diagonal(n,d_P,ld,c[p-1]);CHKERRQ(cerr);

  odd = PETSC_TRUE;
  for (k=p-1;k>0;k--) {
    if (odd) {
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_Q,ld,d_A2,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
      SWAP(d_Q,d_W,aux);
      ierr = shift_diagonal(n,d_Q,ld,c[k-1]);CHKERRQ(cerr);
      odd = PETSC_FALSE;
    } else {
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_P,ld,d_A2,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
      SWAP(d_P,d_W,aux);
      ierr = shift_diagonal(n,d_P,ld,c[k-1]);CHKERRQ(cerr);
      odd = PETSC_TRUE;
    }
    ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  }
  if (odd) {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_Q,ld,d_As,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
    SWAP(d_Q,d_W,aux);
    cberr = cublasXaxpy(cublasv2handle,ld2,&smone,d_P,one,d_Q,one);CHKERRCUBLAS(cberr);
    mierr = magma_xgesv_gpu(n,n,d_Q,ld,piv,d_P,ld,&info);CHKERRMAGMA(mierr);
    if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGESV %d",info);
    cberr = cublasXscal(cublasv2handle,ld2,&stwo,d_P,one);CHKERRCUBLAS(cberr);
    ierr = shift_diagonal(n,d_P,ld,sone);CHKERRQ(cerr);
    cberr = cublasXscal(cublasv2handle,ld2,&smone,d_P,one);CHKERRCUBLAS(cberr);
  } else {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_P,ld,d_As,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
    SWAP(d_P,d_W,aux);
    cberr = cublasXaxpy(cublasv2handle,ld2,&smone,d_P,one,d_Q,one);CHKERRCUBLAS(cberr);
    mierr = magma_xgesv_gpu(n,n,d_Q,ld,piv,d_P,ld,&info);CHKERRMAGMA(mierr);
    if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGESV %d",info);
    cberr = cublasXscal(cublasv2handle,ld2,&stwo,d_P,one);CHKERRCUBLAS(cberr);
    ierr = shift_diagonal(n,d_P,ld,sone);CHKERRQ(cerr);
  }
  ierr = PetscLogFlops(2.0*n*n*n+2.0*n*n*n/3.0+4.0*n*n);CHKERRQ(ierr);

  for (k=1;k<=sexp;k++) {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_P,ld,d_P,ld,&szero,d_W,ld);CHKERRCUBLAS(cberr);
    cerr = cudaMemcpy(d_P,d_W,sizeof(PetscScalar)*ld2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
  }
  if (d_P!=d_Ba) {
    cerr = cudaMemcpy(Ba,d_P,sizeof(PetscScalar)*ld2,cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
  } else {
    cerr = cudaMemcpy(Ba,d_Ba,sizeof(PetscScalar)*ld2,cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
  }
  ierr = PetscLogFlops(2.0*n*n*n*sexp);CHKERRQ(ierr);

  cerr = cudaFree(d_Ba);CHKERRCUDA(cerr);
  cerr = cudaFree(d_Q);CHKERRCUDA(cerr);
  cerr = cudaFree(d_W);CHKERRCUDA(cerr);
  cerr = cudaFree(d_As);CHKERRCUDA(cerr);
  cerr = cudaFree(d_A2);CHKERRCUDA(cerr);
  ierr = PetscFree(piv);CHKERRQ(ierr);

  ierr = MatDenseRestoreArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  magma_finalize();
  PetscFunctionReturn(0);
}

/*
 * Matrix exponential implementation based on algorithm and matlab code by N. Higham and co-authors
 *
 *     N. J. Higham, "The scaling and squaring method for the matrix exponential
 *     revisited", SIAM J. Matrix Anal. Appl. 26(4):1179-1193, 2005.
 */
PetscErrorCode FNEvaluateFunctionMat_Exp_Higham_CUDAm(FN fn,Mat A,Mat B)
{
  PetscErrorCode    ierr;
  PetscBLASInt      n_=0,n2,*ipiv,info,one=1;
  PetscInt          n,m,j,s,zero=0;
  PetscScalar       scale,smone=-1.0,sone=1.0,stwo=2.0,szero=0.0;
  PetscScalar       *Aa,*Ba,*d_Ba,*Apowers[5],*d_Apowers[5],*d_Q,*d_P,*d_W,*work,*d_work,*aux;
  const PetscScalar *c;
  const PetscScalar c3[4]   = { 120, 60, 12, 1 };
  const PetscScalar c5[6]   = { 30240, 15120, 3360, 420, 30, 1 };
  const PetscScalar c7[8]   = { 17297280, 8648640, 1995840, 277200, 25200, 1512, 56, 1 };
  const PetscScalar c9[10]  = { 17643225600, 8821612800, 2075673600, 302702400, 30270240,
    2162160, 110880, 3960, 90, 1 };
  const PetscScalar c13[14] = { 64764752532480000, 32382376266240000, 7771770303897600,
    1187353796428800,  129060195264000,   10559470521600,
    670442572800,      33522128640,       1323241920,
    40840800,          960960,            16380,  182,  1 };
  cublasHandle_t    cublasv2handle;
  cublasStatus_t    cberr;
  cudaError_t       cerr;
  magma_int_t       mierr;

  PetscFunctionBegin;
  ierr = PetscCUBLASGetHandle(&cublasv2handle);CHKERRQ(ierr);
  magma_init();
  ierr = MatDenseGetArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  ierr = MatGetSize(A,&n,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(n,&n_);CHKERRQ(ierr);
  n2 = n_*n_;
  ierr = PetscMalloc2(8*n*n,&work,n,&ipiv);CHKERRQ(ierr);
  ierr = cudaMalloc((void**)&d_work,8*n*n*sizeof(PetscScalar));CHKERRQ(ierr);
  cerr = cudaMalloc((void **)&d_Ba,sizeof(PetscScalar)*n*n);CHKERRCUDA(cerr);
  ierr = cudaMemcpy(d_Ba,Ba,n2*sizeof(PetscScalar),cudaMemcpyHostToDevice);CHKERRQ(ierr);

  /* Matrix powers */
  Apowers[0] = work;                  /* Apowers[0] = A   */
  Apowers[1] = Apowers[0] + n*n;      /* Apowers[1] = A^2 */
  Apowers[2] = Apowers[1] + n*n;      /* Apowers[2] = A^4 */
  Apowers[3] = Apowers[2] + n*n;      /* Apowers[3] = A^6 */
  Apowers[4] = Apowers[3] + n*n;      /* Apowers[4] = A^8 */
  /* Matrix powers on device */
  d_Apowers[0] = d_work;                /* d_Apowers[0] = A   */
  d_Apowers[1] = d_Apowers[0] + n*n;    /* d_Apowers[1] = A^2 */
  d_Apowers[2] = d_Apowers[1] + n*n;    /* d_Apowers[2] = A^4 */
  d_Apowers[3] = d_Apowers[2] + n*n;    /* d_Apowers[3] = A^6 */
  d_Apowers[4] = d_Apowers[3] + n*n;    /* d_Apowers[4] = A^8 */

  ierr = cudaMemcpy(d_Apowers[0],Aa,n2*sizeof(PetscScalar),cudaMemcpyHostToDevice);CHKERRQ(ierr);
  cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[0],n_,d_Apowers[0],n_,&szero,d_Apowers[1],n_);CHKERRCUBLAS(cberr);
  cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[1],n_,d_Apowers[1],n_,&szero,d_Apowers[2],n_);CHKERRCUBLAS(cberr);
  cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[1],n_,d_Apowers[2],n_,&szero,d_Apowers[3],n_);CHKERRCUBLAS(cberr);
  ierr = PetscLogFlops(6.0*n*n*n);CHKERRQ(ierr);

  ierr = cudaMemcpy(Apowers[0],d_Apowers[0],4*n2*sizeof(PetscScalar),cudaMemcpyDeviceToHost);CHKERRQ(ierr);
  /* Compute scaling parameter and order of Pade approximant */
  ierr = expm_params(n,Apowers,&s,&m,Apowers[4]);CHKERRQ(ierr);

  if (s) { /* rescale */
    for (j=0;j<4;j++) {
      scale = PetscPowRealInt(2.0,-PetscMax(2*j,1)*s);
      cberr = cublasXscal(cublasv2handle,n2,&scale,d_Apowers[j],one);CHKERRCUBLAS(cberr);
    }
    ierr = PetscLogFlops(4.0*n*n);CHKERRQ(ierr);
  }

  /* Evaluate the Pade approximant */
  switch (m) {
    case 3:  c = c3;  break;
    case 5:  c = c5;  break;
    case 7:  c = c7;  break;
    case 9:  c = c9;  break;
    case 13: c = c13; break;
    default: SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Wrong value of m %d",m);
  }
  d_P = d_Ba;
  d_Q = d_Apowers[4] + n*n;
  d_W = d_Q + n*n;
  switch (m) {
    case 3:
    case 5:
    case 7:
    case 9:
      if (m==9) {cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[1],n_,d_Apowers[3],n_,&szero,d_Apowers[4],n_);CHKERRCUBLAS(cberr);}
      cerr = cudaMemset(d_P,zero,sizeof(PetscScalar)*n2);CHKERRCUDA(cerr);
      cerr = cudaMemset(d_Q,zero,sizeof(PetscScalar)*n2);CHKERRCUDA(cerr);
      ierr = set_diagonal(n,d_P,n,c[1]);CHKERRQ(cerr);
      ierr = set_diagonal(n,d_Q,n,c[0]);CHKERRQ(cerr);
      for (j=m;j>=3;j-=2) {
        cberr = cublasXaxpy(cublasv2handle,n2,&c[j],d_Apowers[(j+1)/2-1],one,d_P,one);CHKERRCUBLAS(cberr);
        cberr = cublasXaxpy(cublasv2handle,n2,&c[j-1],d_Apowers[(j+1)/2-1],one,d_Q,one);CHKERRCUBLAS(cberr);
        ierr = PetscLogFlops(4.0*n*n);CHKERRQ(ierr);
      }
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[0],n_,d_P,n_,&szero,d_W,n_);CHKERRCUBLAS(cberr);
      ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
      SWAP(d_P,d_W,aux);
      break;
    case 13:
      /*  P = A*(Apowers[3]*(c[13]*Apowers[3] + c[11]*Apowers[2] + c[9]*Apowers[1])
          + c[7]*Apowers[3] + c[5]*Apowers[2] + c[3]*Apowers[1] + c[1]*I)       */
      cerr = cudaMemcpy(d_P,d_Apowers[3],n2*sizeof(PetscScalar),cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
      cberr = cublasXscal(cublasv2handle,n2,&c[13],d_P,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[11],d_Apowers[2],one,d_P,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[9],d_Apowers[1],one,d_P,one);CHKERRCUBLAS(cberr);
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[3],n_,d_P,n_,&szero,d_W,n_);CHKERRCUBLAS(cberr);
      ierr = PetscLogFlops(5.0*n*n+2.0*n*n*n);CHKERRQ(ierr);

      cerr = cudaMemset(d_P,zero,sizeof(PetscScalar)*n2);CHKERRCUDA(cerr);
      ierr = set_diagonal(n,d_P,n,c[1]);CHKERRQ(cerr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[7],d_Apowers[3],one,d_P,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[5],d_Apowers[2],one,d_P,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[3],d_Apowers[1],one,d_P,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&sone,d_P,one,d_W,one);CHKERRCUBLAS(cberr);
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[0],n_,d_W,n_,&szero,d_P,n_);CHKERRCUBLAS(cberr);
      ierr = PetscLogFlops(7.0*n*n+2.0*n*n*n);CHKERRQ(ierr);
      /*  Q = Apowers[3]*(c[12]*Apowers[3] + c[10]*Apowers[2] + c[8]*Apowers[1])
          + c[6]*Apowers[3] + c[4]*Apowers[2] + c[2]*Apowers[1] + c[0]*I        */
      cerr = cudaMemcpy(d_Q,d_Apowers[3],n2*sizeof(PetscScalar),cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
      cberr = cublasXscal(cublasv2handle,n2,&c[12],d_Q,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[10],d_Apowers[2],one,d_Q,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[8],d_Apowers[1],one,d_Q,one);CHKERRCUBLAS(cberr);
      cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_Apowers[3],n_,d_Q,n_,&szero,d_W,n_);CHKERRCUBLAS(cberr);
      ierr = PetscLogFlops(5.0*n*n+2.0*n*n*n);CHKERRQ(ierr);
      cerr = cudaMemset(d_Q,zero,sizeof(PetscScalar)*n2);CHKERRCUDA(cerr);
      ierr = set_diagonal(n,d_Q,n,c[0]);CHKERRQ(cerr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[6],d_Apowers[3],one,d_Q,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[4],d_Apowers[2],one,d_Q,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&c[2],d_Apowers[1],one,d_Q,one);CHKERRCUBLAS(cberr);
      cberr = cublasXaxpy(cublasv2handle,n2,&sone,d_W,one,d_Q,one);CHKERRCUBLAS(cberr);
      ierr = PetscLogFlops(7.0*n*n);CHKERRQ(ierr);
      break;
    default: SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Wrong value of m %d",m);
  }
  cberr = cublasXaxpy(cublasv2handle,n2,&smone,d_P,one,d_Q,one);CHKERRCUBLAS(cberr);

  mierr = magma_xgesv_gpu(n_,n_,d_Q,n_,ipiv,d_P,n_,&info);CHKERRMAGMA(mierr);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGESV %d",info);

  cberr = cublasXscal(cublasv2handle,n2,&stwo,d_P,one);CHKERRCUBLAS(cberr);
  ierr = shift_diagonal(n,d_P,n,sone);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*n*n*n/3.0+4.0*n*n);CHKERRQ(ierr);

  /* Squaring */
  for (j=1;j<=s;j++) {
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n_,n_,n_,&sone,d_P,n_,d_P,n_,&szero,d_W,n_);CHKERRCUBLAS(cberr);
    SWAP(d_P,d_W,aux);
  }
  if (d_P!=d_Ba) {
    cerr = cudaMemcpy(Ba,d_P,n2*sizeof(PetscScalar),cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
  } else {
    cerr = cudaMemcpy(Ba,d_Ba,n2*sizeof(PetscScalar),cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
  }
  ierr = PetscLogFlops(2.0*n*n*n*s);CHKERRQ(ierr);

  ierr = PetscFree2(work,ipiv);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  magma_finalize();
  PetscFunctionReturn(0);
}

/*
 * Matrix exponential implementation based on algorithm and matlab code by Stefan Guettel
 * and Yuji Nakatsukasa
 *
 *     Stefan Guettel and Yuji Nakatsukasa, "Scaled and Squared Subdiagonal Pade'
 *     Approximation for the Matrix Exponential",
 *     SIAM J. Matrix Anal. Appl. 37(1):145-170, 2016.
 *     https://doi.org/10.1137/15M1027553
 */
PetscErrorCode FNEvaluateFunctionMat_Exp_GuettelNakatsukasa_CUDAm(FN fn,Mat A,Mat B)
{
  PetscInt       i,j,n_,s,k,m,zero=0,mod;
  PetscBLASInt   n=0,n2=0,irsize=0,rsizediv2,ipsize=0,iremainsize=0,query=-1,info,*piv,minlen,lwork=0,one=1;
  PetscReal      nrm,shift=0.0,rone=1.0,rzero=0.0;
#if defined(PETSC_USE_COMPLEX)
  PetscReal      *rwork=NULL;
#endif
  PetscComplex   *d_As,*d_RR,*d_RR2,*d_expmA,*d_expmA2,*d_Maux,*d_Maux2,rsize,*r,psize,*p,remainsize,*remainterm,*rootp,*rootq,mult=0.0,scale,cone=1.0,czero=0.0,*aux;
  PetscScalar    *Aa,*Ba,*d_Ba,*d_Ba2,*Maux,*sMaux,*d_sMaux,*wr,*wi,expshift,sone=1.0,szero=0.0,*work,work1,*saux;
  PetscErrorCode ierr;
  PetscBool      isreal,*d_isreal,flg;
  cublasHandle_t cublasv2handle;
  cudaError_t    cerr;
  cublasStatus_t cberr;
  magma_int_t    mierr;

  PetscFunctionBegin;
  ierr = PetscCUBLASGetHandle(&cublasv2handle);CHKERRQ(ierr);
  magma_init();
  ierr = MatGetSize(A,&n_,NULL);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(n_,&n);CHKERRQ(ierr);
  ierr = MatDenseGetArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseGetArray(B,&Ba);CHKERRQ(ierr);
  ierr = PetscBLASIntCast(n*n,&n2);CHKERRQ(ierr);

  cerr = cudaMalloc((void **)&d_Ba,sizeof(PetscScalar)*n2);CHKERRCUDA(cerr);
  cerr = cudaMemcpy(d_Ba,Ba,sizeof(PetscScalar)*n2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  d_Ba2 = d_Ba;

  ierr = PetscMalloc2(n2,&sMaux,n2,&Maux);CHKERRQ(ierr);
  cerr = cudaMalloc((void **)&d_isreal,sizeof(PetscBool));CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_sMaux,sizeof(PetscScalar)*n2);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_Maux,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);

  cerr = cudaMemcpy(d_sMaux,Aa,sizeof(PetscScalar)*n2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  d_Maux2 = d_Maux;
  ierr = PetscOptionsGetReal(NULL,NULL,"-fn_expm_estimated_eig",&shift,&flg);CHKERRQ(ierr);
  if (!flg) {
    ierr = PetscMalloc2(n,&wr,n,&wi);CHKERRQ(ierr);
    ierr = PetscArraycpy(sMaux,Aa,n2);CHKERRQ(ierr);
    /* estimate rightmost eigenvalue and shift A with it */
#if !defined(PETSC_USE_COMPLEX)
    mierr = magma_xgeev(MagmaNoVec,MagmaNoVec,n,sMaux,n,wr,wi,NULL,n,NULL,n,&work1,query,&info);CHKERRMAGMA(mierr);
    SlepcCheckLapackInfo("geev",info);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPart(work1),&lwork);CHKERRQ(ierr);
    ierr = PetscMalloc1(lwork,&work);CHKERRQ(ierr);
    mierr = magma_xgeev(MagmaNoVec,MagmaNoVec,n,sMaux,n,wr,wi,NULL,n,NULL,n,work,lwork,&info);CHKERRMAGMA(mierr);
    ierr = PetscFree(work);CHKERRQ(ierr);
#else
    ierr = PetscArraycpy(Maux,Aa,n2);CHKERRQ(ierr);
    mierr = magma_xgeev(MagmaNoVec,MagmaNoVec,n,Maux,n,wr,NULL,n,NULL,n,&work1,query,rwork,&info);CHKERRMAGMA(mierr);
    SlepcCheckLapackInfo("geev",info);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPart(work1),&lwork);CHKERRQ(ierr);
    ierr = PetscMalloc2(2*n,&rwork,lwork,&work);CHKERRQ(ierr);
    mierr = magma_xgeev(MagmaNoVec,MagmaNoVec,n,Maux,n,wr,NULL,n,NULL,n,work,lwork,rwork,&info);CHKERRMAGMA(mierr);
    ierr = PetscFree2(rwork,work);CHKERRQ(ierr);
#endif
    SlepcCheckLapackInfo("geev",info);
    ierr = PetscLogFlops(25.0*n*n*n+(n*n*n)/3.0+1.0*n*n*n);CHKERRQ(ierr);

    shift = PetscRealPart(wr[0]);
    for (i=1;i<n;i++) {
      if (PetscRealPart(wr[i]) > shift) shift = PetscRealPart(wr[i]);
    }
    ierr = PetscFree2(wr,wi);CHKERRQ(ierr);
  }
  /* shift so that largest real part is (about) 0 */
  cerr = cudaMemcpy(d_sMaux,Aa,sizeof(PetscScalar)*n2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  if (shift) {
    ierr = shift_diagonal(n,d_sMaux,n,-shift);CHKERRQ(ierr);
    ierr = PetscLogFlops(1.0*n);CHKERRQ(ierr);
  }
#if defined(PETSC_USE_COMPLEX)
  cerr = cudaMemcpy(d_Maux,Aa,sizeof(PetscScalar)*n2,cudaMemcpyHostToDevice);CHKERRCUDA(cerr);
  if (shift) {
    ierr = shift_diagonal(n,d_Maux,n,-shift);CHKERRQ(ierr);
    ierr = PetscLogFlops(1.0*n);CHKERRQ(ierr);
  }
#endif

  /* estimate norm(A) and select the scaling factor */
  cberr = cublasXnrm2(cublasv2handle,n2,d_sMaux,one,&nrm);CHKERRCUBLAS(cberr);
  ierr = PetscLogFlops(2.0*n*n);CHKERRQ(ierr);
  ierr = sexpm_params(nrm,&s,&k,&m);CHKERRQ(ierr);
  if (s==0 && k==1 && m==0) { /* exp(A) = I+A to eps! */
    if (shift) expshift = PetscExpReal(shift);
    ierr = shift_Cdiagonal(n,d_Maux,n,rone,rzero);CHKERRQ(ierr);
    if (shift) {
      cberr = cublasXscal(cublasv2handle,n2,&expshift,d_sMaux,one);CHKERRCUBLAS(cberr);
      ierr = PetscLogFlops(1.0*(n+n2));CHKERRQ(ierr);
    } else {
      ierr = PetscLogFlops(1.0*n);CHKERRQ(ierr);
    }
    cerr = cudaMemcpy(Ba,d_sMaux,sizeof(PetscScalar)*n2,cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
    cerr = cudaFree(d_sMaux);CHKERRCUDA(cerr);
    cerr = cudaFree(d_Ba);CHKERRCUDA(cerr);
    ierr = MatDenseRestoreArray(A,&Aa);CHKERRQ(ierr);
    ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
    PetscFunctionReturn(0); /* quick return */
  }

  cerr = cudaMalloc((void **)&d_expmA,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_As,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
  cerr = cudaMalloc((void **)&d_RR,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
  d_expmA2 = d_expmA; d_RR2 = d_RR;
  ierr = PetscMalloc1(n,&piv);CHKERRQ(ierr);
  /* scale matrix */
#if !defined(PETSC_USE_COMPLEX)
  ierr = copy_array2D_S2C(n,n,d_As,n,d_sMaux,n);CHKERRQ(ierr);
#else
  cerr = cudaMemcpy(d_As,d_sMaux,sizeof(PetscScalar)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
#endif
  scale = 1.0/PetscPowRealInt(2.0,s);
  cberr = cublasXCscal(cublasv2handle,n2,(const cuComplex *)&scale,(cuComplex *)d_As,one);CHKERRCUBLAS(cberr);
  ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);

  /* evaluate Pade approximant (partial fraction or product form) */
  if (fn->method==8 || !m) { /* partial fraction */
    ierr = getcoeffs(k,m,&rsize,&psize,&remainsize,PETSC_TRUE);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(rsize),&irsize);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(psize),&ipsize);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(remainsize),&iremainsize);CHKERRQ(ierr);
    ierr = PetscMalloc3(irsize,&r,ipsize,&p,iremainsize,&remainterm);CHKERRQ(ierr);
    ierr = getcoeffs(k,m,r,p,remainterm,PETSC_FALSE);CHKERRQ(ierr);

    cerr = cudaMemset(d_expmA,zero,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
#if !defined(PETSC_USE_COMPLEX)
    isreal = PETSC_TRUE;
#else
    ierr = getisreal_array2D(n,n,d_Maux,n,d_isreal);CHKERRQ(ierr);
    cerr = cudaMemcpy(&isreal,d_isreal,sizeof(PetscBool),cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);
#endif
    if (isreal) {
      rsizediv2 = irsize/2;
      for (i=0;i<rsizediv2;i++) { /* use partial fraction to get R(As) */
        cerr = cudaMemcpy(d_Maux,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
        cerr = cudaMemset(d_RR,zero,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
        ierr = shift_Cdiagonal(n,d_Maux,n,-PetscRealPartComplex(p[2*i]),-PetscImaginaryPartComplex(p[2*i]));CHKERRQ(ierr);
        ierr = set_Cdiagonal(n,d_RR,n,PetscRealPartComplex(r[2*i]),PetscImaginaryPartComplex(r[2*i]));CHKERRQ(ierr);
        mierr = magma_Cgesv_gpu(n,n,d_Maux,n,piv,d_RR,n,&info);CHKERRMAGMA(mierr);
        SlepcCheckLapackInfo("gesv",info);
        ierr = add_array2D_Conj(n,n,d_RR,n);CHKERRQ(ierr);
        cberr = cublasXCaxpy(cublasv2handle,n2,&cone,d_RR,one,d_expmA,one);CHKERRCUBLAS(cberr);
        /* shift(n) + gesv + axpy(n2) */
        ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n)+2.0*n2);CHKERRQ(ierr);
      }

      mod = ipsize % 2;
      if (mod) {
        cerr = cudaMemcpy(d_Maux,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
        cerr = cudaMemset(d_RR,zero,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
        ierr = shift_Cdiagonal(n,d_Maux,n,-PetscRealPartComplex(p[ipsize-1]),-PetscImaginaryPartComplex(p[ipsize-1]));CHKERRQ(ierr);
        ierr = set_Cdiagonal(n,d_RR,n,PetscRealPartComplex(r[irsize-1]),PetscImaginaryPartComplex(r[irsize-1]));CHKERRQ(ierr);
        mierr = magma_Cgesv_gpu(n,n,d_Maux,n,piv,d_RR,n,&info);CHKERRMAGMA(mierr);
        SlepcCheckLapackInfo("gesv",info);
        cberr = cublasXCaxpy(cublasv2handle,n2,&cone,d_RR,one,d_expmA,one);CHKERRCUBLAS(cberr);
        ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n)+1.0*n2);CHKERRQ(ierr);
      }
    } else { /* complex */
      for (i=0;i<irsize;i++) { /* use partial fraction to get R(As) */
        cerr = cudaMemcpy(d_Maux,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
        cerr = cudaMemset(d_RR,zero,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
        ierr = shift_Cdiagonal(n,d_Maux,n,-PetscRealPartComplex(p[i]),-PetscImaginaryPartComplex(p[i]));CHKERRQ(ierr);
        ierr = set_Cdiagonal(n,d_RR,n,PetscRealPartComplex(r[i]),PetscImaginaryPartComplex(r[i]));CHKERRQ(ierr);
        mierr = magma_Cgesv_gpu(n,n,d_Maux,n,piv,d_RR,n,&info);CHKERRMAGMA(mierr);
        SlepcCheckLapackInfo("gesv",info);
        cberr = cublasXCaxpy(cublasv2handle,n2,&cone,d_RR,one,d_expmA,one);CHKERRCUBLAS(cberr);
        ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n)+1.0*n2);CHKERRQ(ierr);
      }
    }
    for (i=0;i<iremainsize;i++) {
      if (!i) {
        cerr = cudaMemset(d_RR,zero,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
        ierr = set_Cdiagonal(n,d_RR,n,PetscRealPartComplex(remainterm[iremainsize-1]),PetscImaginaryPartComplex(remainterm[iremainsize-1]));CHKERRQ(ierr);
      } else {
        cerr = cudaMemcpy(d_RR,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
        for (j=1;j<i;j++) {
          cberr = cublasXCgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&cone,d_RR,n,d_RR,n,&czero,d_Maux,n);CHKERRCUBLAS(cberr);
          SWAP(d_RR,d_Maux,aux);
          ierr = SlepcLogFlopsComplex(2.0*n*n*n);CHKERRQ(ierr);
        }
        cberr = cublasXCscal(cublasv2handle,n2,&remainterm[iremainsize-1-i],d_RR,one);CHKERRCUBLAS(cberr);
        ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);
      }
      cberr = cublasXCaxpy(cublasv2handle,n2,&cone,d_RR,one,d_expmA,one);CHKERRCUBLAS(cberr);
      ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);
    }
    ierr = PetscFree3(r,p,remainterm);CHKERRQ(ierr);
  } else { /* product form, default */
    ierr = getcoeffsproduct(k,m,&rsize,&psize,&mult,PETSC_TRUE);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(rsize),&irsize);CHKERRQ(ierr);
    ierr = PetscBLASIntCast((PetscInt)PetscRealPartComplex(psize),&ipsize);CHKERRQ(ierr);
    ierr = PetscMalloc2(irsize,&rootp,ipsize,&rootq);CHKERRQ(ierr);
    ierr = getcoeffsproduct(k,m,rootp,rootq,&mult,PETSC_FALSE);CHKERRQ(ierr);

    cerr = cudaMemset(d_expmA,zero,sizeof(PetscComplex)*n2);CHKERRCUDA(cerr);
    ierr = set_Cdiagonal(n,d_expmA,n,rone,rzero);CHKERRQ(ierr); /* initialize */
    minlen = PetscMin(irsize,ipsize);
    for (i=0;i<minlen;i++) {
      cerr = cudaMemcpy(d_RR,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
      ierr = shift_Cdiagonal(n,d_RR,n,-PetscRealPartComplex(rootp[i]),-PetscImaginaryPartComplex(rootp[i]));CHKERRQ(ierr);
      cberr = cublasXCgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&cone,d_RR,n,d_expmA,n,&czero,d_Maux,n);CHKERRCUBLAS(cberr);
      SWAP(d_expmA,d_Maux,aux);
      cerr = cudaMemcpy(d_RR,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
      ierr = shift_Cdiagonal(n,d_RR,n,-PetscRealPartComplex(rootq[i]),-PetscImaginaryPartComplex(rootq[i]));CHKERRQ(ierr);
      mierr = magma_Cgesv_gpu(n,n,d_RR,n,piv,d_expmA,n,&info);CHKERRMAGMA(mierr);
      SlepcCheckLapackInfo("gesv",info);
      /* shift(n) + gemm + shift(n) + gesv */
      ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n)+1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n));CHKERRQ(ierr);
    }
    /* extra enumerator */
    for (i=minlen;i<irsize;i++) {
      cerr = cudaMemcpy(d_RR,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
      ierr = shift_Cdiagonal(n,d_RR,n,-PetscRealPartComplex(rootp[i]),-PetscImaginaryPartComplex(rootp[i]));CHKERRQ(ierr);
      cberr = cublasXCgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&cone,d_RR,n,d_expmA,n,&czero,d_Maux,n);CHKERRCUBLAS(cberr);
      SWAP(d_expmA,d_Maux,aux);
      ierr = SlepcLogFlopsComplex(1.0*n+2.0*n*n*n);CHKERRQ(ierr);
    }
    /* extra denominator */
    for (i=minlen;i<ipsize;i++) {
      cerr = cudaMemcpy(d_RR,d_As,sizeof(PetscComplex)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
      ierr = shift_Cdiagonal(n,d_RR,n,-PetscRealPartComplex(rootq[i]),-PetscImaginaryPartComplex(rootq[i]));CHKERRQ(ierr);
      mierr = magma_Cgesv_gpu(n,n,d_RR,n,piv,d_expmA,n,&info);CHKERRMAGMA(mierr);
      SlepcCheckLapackInfo("gesv",info);
      ierr = SlepcLogFlopsComplex(1.0*n+(2.0*n*n*n/3.0+2.0*n*n*n));CHKERRQ(ierr);
    }
    cberr = cublasXCscal(cublasv2handle,n2,&mult,d_expmA,one);CHKERRCUBLAS(cberr);
    ierr = SlepcLogFlopsComplex(1.0*n2);CHKERRQ(ierr);
    ierr = PetscFree2(rootp,rootq);CHKERRQ(ierr);
  }

#if !defined(PETSC_USE_COMPLEX)
  ierr = copy_array2D_C2S(n,n,d_Ba2,n,d_expmA,n);CHKERRQ(ierr);
#else
  cerr = cudaMemcpy(d_Ba2,d_expmA,sizeof(PetscScalar)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
#endif

  /* perform repeated squaring */
  for (i=0;i<s;i++) { /* final squaring */
    cberr = cublasXgemm(cublasv2handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&sone,d_Ba2,n,d_Ba2,n,&szero,d_sMaux,n);CHKERRCUBLAS(cberr);
    SWAP(d_Ba2,d_sMaux,saux);
    ierr = PetscLogFlops(2.0*n*n*n);CHKERRQ(ierr);
  }
  if (d_Ba2!=d_Ba) {
    cerr = cudaMemcpy(d_Ba,d_Ba2,sizeof(PetscScalar)*n2,cudaMemcpyDeviceToDevice);CHKERRCUDA(cerr);
    d_sMaux = d_Ba2;
  }
  if (shift) {
    expshift = PetscExpReal(shift);
    cberr = cublasXscal(cublasv2handle,n2,&expshift,d_Ba,one);CHKERRCUBLAS(cberr);
    ierr = PetscLogFlops(1.0*n2);CHKERRQ(ierr);
  }

  cerr = cudaMemcpy(Ba,d_Ba,sizeof(PetscScalar)*n2,cudaMemcpyDeviceToHost);CHKERRCUDA(cerr);

  /* restore pointers */
  d_Maux = d_Maux2; d_expmA = d_expmA2; d_RR = d_RR2;
  cerr = cudaFree(d_isreal);CHKERRCUDA(cerr);
  cerr = cudaFree(d_Maux);CHKERRCUDA(cerr);
  cerr = cudaFree(d_As);CHKERRCUDA(cerr);
  cerr = cudaFree(d_RR);CHKERRCUDA(cerr);
  cerr = cudaFree(d_expmA);CHKERRCUDA(cerr);
  cerr = cudaFree(d_Ba);CHKERRCUDA(cerr);
  ierr = PetscFree(piv);CHKERRQ(ierr);
  ierr = PetscFree2(sMaux,Maux);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(A,&Aa);CHKERRQ(ierr);
  ierr = MatDenseRestoreArray(B,&Ba);CHKERRQ(ierr);
  magma_finalize();
  PetscFunctionReturn(0);
}
#endif /* PETSC_HAVE_MAGMA */
#endif /* PETSC_HAVE_CUDA */

PetscErrorCode FNView_Exp(FN fn,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscBool      isascii;
  char           str[50];
  const char     *methodname[] = {
                  "scaling & squaring, [m/m] Pade approximant (Higham)",
                  "scaling & squaring, [6/6] Pade approximant",
                  "scaling & squaring, subdiagonal Pade approximant (product form)",
                  "scaling & squaring, subdiagonal Pade approximant (partial fraction)"
#if defined(PETSC_HAVE_CUDA)
                 ,"scaling & squaring, [6/6] Pade approximant CUDA"
#if defined(PETSC_HAVE_MAGMA)
                 ,"scaling & squaring, [m/m] Pade approximant (Higham) CUDA/MAGMA",
                  "scaling & squaring, [6/6] Pade approximant CUDA/MAGMA",
                  "scaling & squaring, subdiagonal Pade approximant (product form) CUDA/MAGMA",
                  "scaling & squaring, subdiagonal Pade approximant (partial fraction) CUDA/MAGMA",
#endif
#endif
  };
  const int      nmeth=sizeof(methodname)/sizeof(methodname[0]);

  PetscFunctionBegin;
  ierr = PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    if (fn->beta==(PetscScalar)1.0) {
      if (fn->alpha==(PetscScalar)1.0) {
        ierr = PetscViewerASCIIPrintf(viewer,"  Exponential: exp(x)\n");CHKERRQ(ierr);
      } else {
        ierr = SlepcSNPrintfScalar(str,sizeof(str),fn->alpha,PETSC_TRUE);CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer,"  Exponential: exp(%s*x)\n",str);CHKERRQ(ierr);
      }
    } else {
      ierr = SlepcSNPrintfScalar(str,sizeof(str),fn->beta,PETSC_TRUE);CHKERRQ(ierr);
      if (fn->alpha==(PetscScalar)1.0) {
        ierr = PetscViewerASCIIPrintf(viewer,"  Exponential: %s*exp(x)\n",str);CHKERRQ(ierr);
      } else {
        ierr = PetscViewerASCIIPrintf(viewer,"  Exponential: %s",str);CHKERRQ(ierr);
        ierr = PetscViewerASCIIUseTabs(viewer,PETSC_FALSE);CHKERRQ(ierr);
        ierr = SlepcSNPrintfScalar(str,sizeof(str),fn->alpha,PETSC_TRUE);CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer,"*exp(%s*x)\n",str);CHKERRQ(ierr);
        ierr = PetscViewerASCIIUseTabs(viewer,PETSC_TRUE);CHKERRQ(ierr);
      }
    }
    if (fn->method<nmeth) {
      ierr = PetscViewerASCIIPrintf(viewer,"  computing matrix functions with: %s\n",methodname[fn->method]);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

SLEPC_EXTERN PetscErrorCode FNCreate_Exp(FN fn)
{
  PetscFunctionBegin;
  fn->ops->evaluatefunction       = FNEvaluateFunction_Exp;
  fn->ops->evaluatederivative     = FNEvaluateDerivative_Exp;
  fn->ops->evaluatefunctionmat[0] = FNEvaluateFunctionMat_Exp_Higham;
  fn->ops->evaluatefunctionmat[1] = FNEvaluateFunctionMat_Exp_Pade;
  fn->ops->evaluatefunctionmat[2] = FNEvaluateFunctionMat_Exp_GuettelNakatsukasa; /* product form */
  fn->ops->evaluatefunctionmat[3] = FNEvaluateFunctionMat_Exp_GuettelNakatsukasa; /* partial fraction */
#if defined(PETSC_HAVE_CUDA)
  fn->ops->evaluatefunctionmat[4] = FNEvaluateFunctionMat_Exp_Pade_CUDA;
#if defined(PETSC_HAVE_MAGMA)
  fn->ops->evaluatefunctionmat[5] = FNEvaluateFunctionMat_Exp_Higham_CUDAm;
  fn->ops->evaluatefunctionmat[6] = FNEvaluateFunctionMat_Exp_Pade_CUDAm;
  fn->ops->evaluatefunctionmat[7] = FNEvaluateFunctionMat_Exp_GuettelNakatsukasa_CUDAm; /* product form */
  fn->ops->evaluatefunctionmat[8] = FNEvaluateFunctionMat_Exp_GuettelNakatsukasa_CUDAm; /* partial fraction */
#endif
#endif
  fn->ops->view                   = FNView_Exp;
  PetscFunctionReturn(0);
}

