/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   SLEPc nonlinear eigensolver: "nleigs"

   Method: NLEIGS

   Algorithm:

       Fully rational Krylov method for nonlinear eigenvalue problems.

   References:

       [1] S. Guttel et al., "NLEIGS: A class of robust fully rational Krylov
           method for nonlinear eigenvalue problems", SIAM J. Sci. Comput.
           36(6):A2842-A2864, 2014.
*/

#include <slepc/private/nepimpl.h>         /*I "slepcnep.h" I*/
#include <slepcblaslapack.h>
#include "nleigs.h"

PetscErrorCode NEPNLEIGSBackTransform(PetscObject ob,PetscInt n,PetscScalar *valr,PetscScalar *vali)
{
  NEP         nep;
  PetscInt    j;
#if !defined(PETSC_USE_COMPLEX)
  PetscScalar t;
#endif

  PetscFunctionBegin;
  nep = (NEP)ob;
#if !defined(PETSC_USE_COMPLEX)
  for (j=0;j<n;j++) {
    if (vali[j] == 0) valr[j] = 1.0 / valr[j] + nep->target;
    else {
      t = valr[j] * valr[j] + vali[j] * vali[j];
      valr[j] = valr[j] / t + nep->target;
      vali[j] = - vali[j] / t;
    }
  }
#else
  for (j=0;j<n;j++) {
    valr[j] = 1.0 / valr[j] + nep->target;
  }
#endif
  PetscFunctionReturn(0);
}

/* Computes the roots of a polynomial */
static PetscErrorCode NEPNLEIGSAuxiliarPRootFinder(PetscInt deg,PetscScalar *polcoeffs,PetscScalar *wr,PetscScalar *wi,PetscBool *avail)
{
  PetscScalar    *C;
  PetscBLASInt   n_,lwork;
  PetscInt       i;
#if defined(PETSC_USE_COMPLEX)
  PetscReal      *rwork=NULL;
#endif
  PetscScalar    *work;
  PetscBLASInt   info;

  PetscFunctionBegin;
  *avail = PETSC_TRUE;
  if (deg>0) {
    CHKERRQ(PetscCalloc1(deg*deg,&C));
    CHKERRQ(PetscBLASIntCast(deg,&n_));
    for (i=0;i<deg-1;i++) {
      C[(deg+1)*i+1]   = 1.0;
      C[(deg-1)*deg+i] = -polcoeffs[deg-i]/polcoeffs[0];
    }
    C[deg*deg+-1] = -polcoeffs[1]/polcoeffs[0];
    CHKERRQ(PetscBLASIntCast(3*deg,&lwork));

    CHKERRQ(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
#if !defined(PETSC_USE_COMPLEX)
    CHKERRQ(PetscMalloc1(lwork,&work));
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_("N","N",&n_,C,&n_,wr,wi,NULL,&n_,NULL,&n_,work,&lwork,&info));
    if (info) *avail = PETSC_FALSE;
    CHKERRQ(PetscFree(work));
#else
    CHKERRQ(PetscMalloc2(2*deg,&rwork,lwork,&work));
    PetscStackCallBLAS("LAPACKgeev",LAPACKgeev_("N","N",&n_,C,&n_,wr,NULL,&n_,NULL,&n_,work,&lwork,rwork,&info));
    if (info) *avail = PETSC_FALSE;
    CHKERRQ(PetscFree2(rwork,work));
#endif
    CHKERRQ(PetscFPTrapPop());
    CHKERRQ(PetscFree(C));
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSAuxiliarRmDuplicates(PetscInt nin,PetscScalar *pin,PetscInt *nout,PetscScalar *pout,PetscInt max)
{
  PetscInt i,j;

  PetscFunctionBegin;
  for (i=0;i<nin;i++) {
    if (max && *nout>=max) break;
    pout[(*nout)++] = pin[i];
    for (j=0;j<*nout-1;j++)
      if (PetscAbsScalar(pin[i]-pout[j])<PETSC_MACHINE_EPSILON*100) {
        (*nout)--;
        break;
      }
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSFNSingularities(FN f,PetscInt *nisol,PetscScalar **isol,PetscBool *rational)
{
  FNCombineType  ctype;
  FN             f1,f2;
  PetscInt       i,nq,nisol1,nisol2;
  PetscScalar    *qcoeff,*wr,*wi,*isol1,*isol2;
  PetscBool      flg,avail,rat1,rat2;

  PetscFunctionBegin;
  *rational = PETSC_FALSE;
  CHKERRQ(PetscObjectTypeCompare((PetscObject)f,FNRATIONAL,&flg));
  if (flg) {
    *rational = PETSC_TRUE;
    CHKERRQ(FNRationalGetDenominator(f,&nq,&qcoeff));
    if (nq>1) {
      CHKERRQ(PetscMalloc2(nq-1,&wr,nq-1,&wi));
      CHKERRQ(NEPNLEIGSAuxiliarPRootFinder(nq-1,qcoeff,wr,wi,&avail));
      if (avail) {
        CHKERRQ(PetscCalloc1(nq-1,isol));
        *nisol = 0;
        for (i=0;i<nq-1;i++)
#if !defined(PETSC_USE_COMPLEX)
          if (wi[i]==0)
#endif
            (*isol)[(*nisol)++] = wr[i];
        nq = *nisol; *nisol = 0;
        for (i=0;i<nq;i++) wr[i] = (*isol)[i];
        CHKERRQ(NEPNLEIGSAuxiliarRmDuplicates(nq,wr,nisol,*isol,0));
        CHKERRQ(PetscFree2(wr,wi));
      } else { *nisol=0; *isol = NULL; }
    } else { *nisol = 0; *isol = NULL; }
    CHKERRQ(PetscFree(qcoeff));
  }
  CHKERRQ(PetscObjectTypeCompare((PetscObject)f,FNCOMBINE,&flg));
  if (flg) {
    CHKERRQ(FNCombineGetChildren(f,&ctype,&f1,&f2));
    if (ctype != FN_COMBINE_COMPOSE && ctype != FN_COMBINE_DIVIDE) {
      CHKERRQ(NEPNLEIGSFNSingularities(f1,&nisol1,&isol1,&rat1));
      CHKERRQ(NEPNLEIGSFNSingularities(f2,&nisol2,&isol2,&rat2));
      if (nisol1+nisol2>0) {
        CHKERRQ(PetscCalloc1(nisol1+nisol2,isol));
        *nisol = 0;
        CHKERRQ(NEPNLEIGSAuxiliarRmDuplicates(nisol1,isol1,nisol,*isol,0));
        CHKERRQ(NEPNLEIGSAuxiliarRmDuplicates(nisol2,isol2,nisol,*isol,0));
      }
      *rational = (rat1&&rat2)?PETSC_TRUE:PETSC_FALSE;
      CHKERRQ(PetscFree(isol1));
      CHKERRQ(PetscFree(isol2));
    }
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSRationalSingularities(NEP nep,PetscInt *ndptx,PetscScalar *dxi,PetscBool *rational)
{
  PetscInt       nt,i,nisol;
  FN             f;
  PetscScalar    *isol;
  PetscBool      rat;

  PetscFunctionBegin;
  *rational = PETSC_TRUE;
  *ndptx = 0;
  CHKERRQ(NEPGetSplitOperatorInfo(nep,&nt,NULL));
  for (i=0;i<nt;i++) {
    CHKERRQ(NEPGetSplitOperatorTerm(nep,i,NULL,&f));
    CHKERRQ(NEPNLEIGSFNSingularities(f,&nisol,&isol,&rat));
    if (nisol) {
      CHKERRQ(NEPNLEIGSAuxiliarRmDuplicates(nisol,isol,ndptx,dxi,0));
      CHKERRQ(PetscFree(isol));
    }
    *rational = ((*rational)&&rat)?PETSC_TRUE:PETSC_FALSE;
  }
  PetscFunctionReturn(0);
}

/*  Adaptive Anderson-Antoulas algorithm */
static PetscErrorCode NEPNLEIGSAAAComputation(NEP nep,PetscInt ndpt,PetscScalar *ds,PetscScalar *F,PetscInt *ndptx,PetscScalar *dxi)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscScalar    mean=0.0,*z,*f,*C,*A,*VT,*work,*ww,szero=0.0,sone=1.0;
  PetscScalar    *N,*D;
  PetscReal      *S,norm,err,*R;
  PetscInt       i,k,j,idx=0,cont;
  PetscBLASInt   n_,m_,lda_,lwork,info,one=1;
#if defined(PETSC_USE_COMPLEX)
  PetscReal      *rwork;
#endif

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(8*ndpt,&lwork));
  CHKERRQ(PetscMalloc5(ndpt,&R,ndpt,&z,ndpt,&f,ndpt*ndpt,&C,ndpt,&ww));
  CHKERRQ(PetscMalloc6(ndpt*ndpt,&A,ndpt,&S,ndpt*ndpt,&VT,lwork,&work,ndpt,&D,ndpt,&N));
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscMalloc1(8*ndpt,&rwork));
#endif
  CHKERRQ(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
  norm = 0.0;
  for (i=0;i<ndpt;i++) {
    mean += F[i];
    norm = PetscMax(PetscAbsScalar(F[i]),norm);
  }
  mean /= ndpt;
  CHKERRQ(PetscBLASIntCast(ndpt,&lda_));
  for (i=0;i<ndpt;i++) R[i] = PetscAbsScalar(F[i]-mean);
  /* next support point */
  err = 0.0;
  for (i=0;i<ndpt;i++) if (R[i]>=err) {idx = i; err = R[i];}
  for (k=0;k<ndpt-1;k++) {
    z[k] = ds[idx]; f[k] = F[idx]; R[idx] = -1.0;
    /* next column of Cauchy matrix */
    for (i=0;i<ndpt;i++) {
      C[i+k*ndpt] = 1.0/(ds[i]-ds[idx]);
    }

    CHKERRQ(PetscArrayzero(A,ndpt*ndpt));
    cont = 0;
    for (i=0;i<ndpt;i++) {
      if (R[i]!=-1.0) {
        for (j=0;j<=k;j++)A[cont+j*ndpt] = C[i+j*ndpt]*F[i]-C[i+j*ndpt]*f[j];
        cont++;
      }
    }
    CHKERRQ(PetscBLASIntCast(cont,&m_));
    CHKERRQ(PetscBLASIntCast(k+1,&n_));
#if defined(PETSC_USE_COMPLEX)
    PetscStackCallBLAS("LAPACKgesvd",LAPACKgesvd_("N","A",&m_,&n_,A,&lda_,S,NULL,&lda_,VT,&lda_,work,&lwork,rwork,&info));
#else
    PetscStackCallBLAS("LAPACKgesvd",LAPACKgesvd_("N","A",&m_,&n_,A,&lda_,S,NULL,&lda_,VT,&lda_,work,&lwork,&info));
#endif
    SlepcCheckLapackInfo("gesvd",info);
    for (i=0;i<=k;i++) {
      ww[i] = PetscConj(VT[i*ndpt+k]);
      D[i] = ww[i]*f[i];
    }
    PetscStackCallBLAS("BLASgemv",BLASgemv_("N",&lda_,&n_,&sone,C,&lda_,D,&one,&szero,N,&one));
    PetscStackCallBLAS("BLASgemv",BLASgemv_("N",&lda_,&n_,&sone,C,&lda_,ww,&one,&szero,D,&one));
    for (i=0;i<ndpt;i++) if (R[i]>=0) R[i] = PetscAbsScalar(F[i]-N[i]/D[i]);
    /* next support point */
    err = 0.0;
    for (i=0;i<ndpt;i++) if (R[i]>=err) {idx = i; err = R[i];}
    if (err <= ctx->ddtol*norm) break;
  }

  PetscCheck(k<ndpt-1,PetscObjectComm((PetscObject)nep),PETSC_ERR_CONV_FAILED,"Failed to determine singularities automatically in general problem");
  /* poles */
  CHKERRQ(PetscArrayzero(C,ndpt*ndpt));
  CHKERRQ(PetscArrayzero(A,ndpt*ndpt));
  for (i=0;i<=k;i++) {
    C[i+ndpt*i] = 1.0;
    A[(i+1)*ndpt] = ww[i];
    A[i+1] = 1.0;
    A[i+1+(i+1)*ndpt] = z[i];
  }
  C[0] = 0.0; C[k+1+(k+1)*ndpt] = 1.0;
  n_++;
#if defined(PETSC_USE_COMPLEX)
  PetscStackCallBLAS("LAPACKggev",LAPACKggev_("N","N",&n_,A,&lda_,C,&lda_,D,N,NULL,&lda_,NULL,&lda_,work,&lwork,rwork,&info));
#else
  PetscStackCallBLAS("LAPACKggev",LAPACKggev_("N","N",&n_,A,&lda_,C,&lda_,D,VT,N,NULL,&lda_,NULL,&lda_,work,&lwork,&info));
#endif
  SlepcCheckLapackInfo("ggev",info);
  cont = 0.0;
  for (i=0;i<n_;i++) if (N[i]!=0.0) {
    dxi[cont++] = D[i]/N[i];
  }
  *ndptx = cont;
  CHKERRQ(PetscFPTrapPop());
  CHKERRQ(PetscFree5(R,z,f,C,ww));
  CHKERRQ(PetscFree6(A,S,VT,work,D,N));
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscFree(rwork));
#endif
  PetscFunctionReturn(0);
}

/*  Singularities using Adaptive Anderson-Antoulas algorithm */
static PetscErrorCode NEPNLEIGSAAASingularities(NEP nep,PetscInt ndpt,PetscScalar *ds,PetscInt *ndptx,PetscScalar *dxi)
{
  Vec            u,v,w;
  PetscRandom    rand=NULL;
  PetscScalar    *F,*isol;
  PetscInt       i,k,nisol,nt;
  Mat            T;
  FN             f;

  PetscFunctionBegin;
  CHKERRQ(PetscMalloc1(ndpt,&F));
  if (nep->fui==NEP_USER_INTERFACE_SPLIT) {
    CHKERRQ(PetscMalloc1(ndpt,&isol));
    *ndptx = 0;
    CHKERRQ(NEPGetSplitOperatorInfo(nep,&nt,NULL));
    nisol = *ndptx;
    for (k=0;k<nt;k++) {
      CHKERRQ(NEPGetSplitOperatorTerm(nep,k,NULL,&f));
      for (i=0;i<ndpt;i++) CHKERRQ(FNEvaluateFunction(f,ds[i],&F[i]));
      CHKERRQ(NEPNLEIGSAAAComputation(nep,ndpt,ds,F,&nisol,isol));
      if (nisol) CHKERRQ(NEPNLEIGSAuxiliarRmDuplicates(nisol,isol,ndptx,dxi,ndpt));
    }
    CHKERRQ(PetscFree(isol));
  } else {
    CHKERRQ(MatCreateVecs(nep->function,&u,NULL));
    CHKERRQ(VecDuplicate(u,&v));
    CHKERRQ(VecDuplicate(u,&w));
    if (nep->V) CHKERRQ(BVGetRandomContext(nep->V,&rand));
    CHKERRQ(VecSetRandom(u,rand));
    CHKERRQ(VecNormalize(u,NULL));
    CHKERRQ(VecSetRandom(v,rand));
    CHKERRQ(VecNormalize(v,NULL));
    T = nep->function;
    for (i=0;i<ndpt;i++) {
      CHKERRQ(NEPComputeFunction(nep,ds[i],T,T));
      CHKERRQ(MatMult(T,v,w));
      CHKERRQ(VecDot(w,u,&F[i]));
    }
    CHKERRQ(NEPNLEIGSAAAComputation(nep,ndpt,ds,F,ndptx,dxi));
    CHKERRQ(VecDestroy(&u));
    CHKERRQ(VecDestroy(&v));
    CHKERRQ(VecDestroy(&w));
  }
  CHKERRQ(PetscFree(F));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSLejaBagbyPoints(NEP nep)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       i,k,ndpt=NDPOINTS,ndptx=NDPOINTS;
  PetscScalar    *ds,*dsi,*dxi,*nrs,*nrxi,*s=ctx->s,*xi=ctx->xi,*beta=ctx->beta;
  PetscReal      maxnrs,minnrxi;
  PetscBool      rational;
#if !defined(PETSC_USE_COMPLEX)
  PetscReal      a,b,h;
#endif

  PetscFunctionBegin;
  if (!ctx->computesingularities && nep->problem_type!=NEP_RATIONAL) ndpt = ndptx = LBPOINTS;
  CHKERRQ(PetscMalloc5(ndpt+1,&ds,ndpt+1,&dsi,ndpt,&dxi,ndpt+1,&nrs,ndpt,&nrxi));

  /* Discretize the target region boundary */
  CHKERRQ(RGComputeContour(nep->rg,ndpt,ds,dsi));
#if !defined(PETSC_USE_COMPLEX)
  for (i=0;i<ndpt;i++) if (dsi[i]!=0.0) break;
  if (i<ndpt) {
    PetscCheck(nep->problem_type==NEP_RATIONAL,PetscObjectComm((PetscObject)nep),PETSC_ERR_SUP,"NLEIGS with real arithmetic requires the target set to be included in the real axis");
    /* Select a segment in the real axis */
    CHKERRQ(RGComputeBoundingBox(nep->rg,&a,&b,NULL,NULL));
    PetscCheck(a>-PETSC_MAX_REAL && b<PETSC_MAX_REAL,PetscObjectComm((PetscObject)nep),PETSC_ERR_USER_INPUT,"NLEIGS requires a bounded target set");
    h = (b-a)/ndpt;
    for (i=0;i<ndpt;i++) {ds[i] = a+h*i; dsi[i] = 0.0;}
  }
#endif
  /* Discretize the singularity region */
  if (ctx->computesingularities) CHKERRQ((ctx->computesingularities)(nep,&ndptx,dxi,ctx->singularitiesctx));
  else {
    if (nep->problem_type==NEP_RATIONAL) {
      CHKERRQ(NEPNLEIGSRationalSingularities(nep,&ndptx,dxi,&rational));
      PetscCheck(rational,PetscObjectComm((PetscObject)nep),PETSC_ERR_CONV_FAILED,"Failed to determine singularities automatically in rational problem; consider solving the problem as general");
    } else {
      /* AAA algorithm */
      CHKERRQ(NEPNLEIGSAAASingularities(nep,ndpt,ds,&ndptx,dxi));
    }
  }
  /* Look for Leja-Bagby points in the discretization sets */
  s[0]  = ds[0];
  xi[0] = (ndptx>0)?dxi[0]:PETSC_INFINITY;
  PetscCheck(PetscAbsScalar(xi[0])>=10*PETSC_MACHINE_EPSILON,PetscObjectComm((PetscObject)nep),PETSC_ERR_USER_INPUT,"Singularity point 0 is nearly zero: %g; consider removing the singularity or shifting the problem",(double)PetscAbsScalar(xi[0]));
  beta[0] = 1.0; /* scaling factors are also computed here */
  for (i=0;i<ndpt;i++) {
    nrs[i] = 1.0;
    nrxi[i] = 1.0;
  }
  for (k=1;k<ctx->ddmaxit;k++) {
    maxnrs = 0.0;
    minnrxi = PETSC_MAX_REAL;
    for (i=0;i<ndpt;i++) {
      nrs[i] *= ((ds[i]-s[k-1])/(1.0-ds[i]/xi[k-1]))/beta[k-1];
      if (PetscAbsScalar(nrs[i])>maxnrs) {maxnrs = PetscAbsScalar(nrs[i]); s[k] = ds[i];}
    }
    if (ndptx>k) {
      for (i=1;i<ndptx;i++) {
        nrxi[i] *= ((dxi[i]-s[k-1])/(1.0-dxi[i]/xi[k-1]))/beta[k-1];
        if (PetscAbsScalar(nrxi[i])<minnrxi) {minnrxi = PetscAbsScalar(nrxi[i]); xi[k] = dxi[i];}
      }
      PetscCheck(PetscAbsScalar(xi[k])>=10*PETSC_MACHINE_EPSILON,PetscObjectComm((PetscObject)nep),PETSC_ERR_USER_INPUT,"Singularity point %" PetscInt_FMT " is nearly zero: %g; consider removing the singularity or shifting the problem",k,(double)PetscAbsScalar(xi[k]));
    } else xi[k] = PETSC_INFINITY;
    beta[k] = maxnrs;
  }
  CHKERRQ(PetscFree5(ds,dsi,dxi,nrs,nrxi));
  PetscFunctionReturn(0);
}

PetscErrorCode NEPNLEIGSEvalNRTFunct(NEP nep,PetscInt k,PetscScalar sigma,PetscScalar *b)
{
  NEP_NLEIGS  *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt    i;
  PetscScalar *beta=ctx->beta,*s=ctx->s,*xi=ctx->xi;

  PetscFunctionBegin;
  b[0] = 1.0/beta[0];
  for (i=0;i<k;i++) {
    b[i+1] = ((sigma-s[i])*b[i])/(beta[i+1]*(1.0-sigma/xi[i]));
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatMult_Fun(Mat A,Vec x,Vec y)
{
  NEP_NLEIGS_MATSHELL *ctx;
  PetscInt            i;

  PetscFunctionBeginUser;
  CHKERRQ(MatShellGetContext(A,&ctx));
  CHKERRQ(MatMult(ctx->A[0],x,y));
  if (ctx->coeff[0]!=1.0) CHKERRQ(VecScale(y,ctx->coeff[0]));
  for (i=1;i<ctx->nmat;i++) {
    CHKERRQ(MatMult(ctx->A[i],x,ctx->t));
    CHKERRQ(VecAXPY(y,ctx->coeff[i],ctx->t));
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatMultTranspose_Fun(Mat A,Vec x,Vec y)
{
  NEP_NLEIGS_MATSHELL *ctx;
  PetscInt            i;

  PetscFunctionBeginUser;
  CHKERRQ(MatShellGetContext(A,&ctx));
  CHKERRQ(MatMultTranspose(ctx->A[0],x,y));
  if (ctx->coeff[0]!=1.0) CHKERRQ(VecScale(y,ctx->coeff[0]));
  for (i=1;i<ctx->nmat;i++) {
    CHKERRQ(MatMultTranspose(ctx->A[i],x,ctx->t));
    CHKERRQ(VecAXPY(y,ctx->coeff[i],ctx->t));
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatGetDiagonal_Fun(Mat A,Vec diag)
{
  NEP_NLEIGS_MATSHELL *ctx;
  PetscInt            i;

  PetscFunctionBeginUser;
  CHKERRQ(MatShellGetContext(A,&ctx));
  CHKERRQ(MatGetDiagonal(ctx->A[0],diag));
  if (ctx->coeff[0]!=1.0) CHKERRQ(VecScale(diag,ctx->coeff[0]));
  for (i=1;i<ctx->nmat;i++) {
    CHKERRQ(MatGetDiagonal(ctx->A[i],ctx->t));
    CHKERRQ(VecAXPY(diag,ctx->coeff[i],ctx->t));
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatDuplicate_Fun(Mat A,MatDuplicateOption op,Mat *B)
{
  PetscInt            m,n,M,N,i;
  NEP_NLEIGS_MATSHELL *ctxnew,*ctx;
  void                (*fun)(void);

  PetscFunctionBeginUser;
  CHKERRQ(MatShellGetContext(A,&ctx));
  CHKERRQ(PetscNew(&ctxnew));
  ctxnew->nmat = ctx->nmat;
  ctxnew->maxnmat = ctx->maxnmat;
  CHKERRQ(PetscMalloc2(ctxnew->maxnmat,&ctxnew->A,ctxnew->maxnmat,&ctxnew->coeff));
  for (i=0;i<ctx->nmat;i++) {
    CHKERRQ(PetscObjectReference((PetscObject)ctx->A[i]));
    ctxnew->A[i] = ctx->A[i];
    ctxnew->coeff[i] = ctx->coeff[i];
  }
  CHKERRQ(MatGetSize(ctx->A[0],&M,&N));
  CHKERRQ(MatGetLocalSize(ctx->A[0],&m,&n));
  CHKERRQ(VecDuplicate(ctx->t,&ctxnew->t));
  CHKERRQ(MatCreateShell(PetscObjectComm((PetscObject)A),m,n,M,N,(void*)ctxnew,B));
  CHKERRQ(MatShellSetManageScalingShifts(*B));
  CHKERRQ(MatShellGetOperation(A,MATOP_MULT,&fun));
  CHKERRQ(MatShellSetOperation(*B,MATOP_MULT,fun));
  CHKERRQ(MatShellGetOperation(A,MATOP_MULT_TRANSPOSE,&fun));
  CHKERRQ(MatShellSetOperation(*B,MATOP_MULT_TRANSPOSE,fun));
  CHKERRQ(MatShellGetOperation(A,MATOP_GET_DIAGONAL,&fun));
  CHKERRQ(MatShellSetOperation(*B,MATOP_GET_DIAGONAL,fun));
  CHKERRQ(MatShellGetOperation(A,MATOP_DUPLICATE,&fun));
  CHKERRQ(MatShellSetOperation(*B,MATOP_DUPLICATE,fun));
  CHKERRQ(MatShellGetOperation(A,MATOP_DESTROY,&fun));
  CHKERRQ(MatShellSetOperation(*B,MATOP_DESTROY,fun));
  CHKERRQ(MatShellGetOperation(A,MATOP_AXPY,&fun));
  CHKERRQ(MatShellSetOperation(*B,MATOP_AXPY,fun));
  PetscFunctionReturn(0);
}

static PetscErrorCode MatDestroy_Fun(Mat A)
{
  NEP_NLEIGS_MATSHELL *ctx;
  PetscInt            i;

  PetscFunctionBeginUser;
  if (A) {
    CHKERRQ(MatShellGetContext(A,&ctx));
    for (i=0;i<ctx->nmat;i++) CHKERRQ(MatDestroy(&ctx->A[i]));
    CHKERRQ(VecDestroy(&ctx->t));
    CHKERRQ(PetscFree2(ctx->A,ctx->coeff));
    CHKERRQ(PetscFree(ctx));
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatAXPY_Fun(Mat Y,PetscScalar a,Mat X,MatStructure str)
{
  NEP_NLEIGS_MATSHELL *ctxY,*ctxX;
  PetscInt            i,j;
  PetscBool           found;

  PetscFunctionBeginUser;
  CHKERRQ(MatShellGetContext(Y,&ctxY));
  CHKERRQ(MatShellGetContext(X,&ctxX));
  for (i=0;i<ctxX->nmat;i++) {
    found = PETSC_FALSE;
    for (j=0;!found&&j<ctxY->nmat;j++) {
      if (ctxX->A[i]==ctxY->A[j]) {
        found = PETSC_TRUE;
        ctxY->coeff[j] += a*ctxX->coeff[i];
      }
    }
    if (!found) {
      ctxY->coeff[ctxY->nmat] = a*ctxX->coeff[i];
      ctxY->A[ctxY->nmat++] = ctxX->A[i];
      CHKERRQ(PetscObjectReference((PetscObject)ctxX->A[i]));
    }
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatScale_Fun(Mat M,PetscScalar a)
{
  NEP_NLEIGS_MATSHELL *ctx;
  PetscInt            i;

  PetscFunctionBeginUser;
  CHKERRQ(MatShellGetContext(M,&ctx));
  for (i=0;i<ctx->nmat;i++) ctx->coeff[i] *= a;
  PetscFunctionReturn(0);
}

static PetscErrorCode NLEIGSMatToMatShellArray(Mat A,Mat *Ms,PetscInt maxnmat)
{
  NEP_NLEIGS_MATSHELL *ctx;
  PetscInt            m,n,M,N;
  PetscBool           has;

  PetscFunctionBegin;
  CHKERRQ(MatHasOperation(A,MATOP_DUPLICATE,&has));
  PetscCheck(has,PetscObjectComm((PetscObject)A),PETSC_ERR_USER,"MatDuplicate operation required");
  CHKERRQ(PetscNew(&ctx));
  ctx->maxnmat = maxnmat;
  CHKERRQ(PetscMalloc2(ctx->maxnmat,&ctx->A,ctx->maxnmat,&ctx->coeff));
  CHKERRQ(MatDuplicate(A,MAT_COPY_VALUES,&ctx->A[0]));
  ctx->nmat = 1;
  ctx->coeff[0] = 1.0;
  CHKERRQ(MatCreateVecs(A,&ctx->t,NULL));
  CHKERRQ(MatGetSize(A,&M,&N));
  CHKERRQ(MatGetLocalSize(A,&m,&n));
  CHKERRQ(MatCreateShell(PetscObjectComm((PetscObject)A),m,n,M,N,(void*)ctx,Ms));
  CHKERRQ(MatShellSetManageScalingShifts(*Ms));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_MULT,(void(*)(void))MatMult_Fun));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_MULT_TRANSPOSE,(void(*)(void))MatMultTranspose_Fun));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_GET_DIAGONAL,(void(*)(void))MatGetDiagonal_Fun));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_DUPLICATE,(void(*)(void))MatDuplicate_Fun));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_DESTROY,(void(*)(void))MatDestroy_Fun));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_AXPY,(void(*)(void))MatAXPY_Fun));
  CHKERRQ(MatShellSetOperation(*Ms,MATOP_SCALE,(void(*)(void))MatScale_Fun));
  PetscFunctionReturn(0);
}

/*
   MatIsShellAny - returns true if any of the n matrices is a shell matrix
 */
static PetscErrorCode MatIsShellAny(Mat *A,PetscInt n,PetscBool *shell)
{
  PetscInt       i;
  PetscBool      flg;

  PetscFunctionBegin;
  *shell = PETSC_FALSE;
  for (i=0;i<n;i++) {
    CHKERRQ(MatIsShell(A[i],&flg));
    if (flg) { *shell = PETSC_TRUE; break; }
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSDividedDifferences_split(NEP nep)
{
  PetscErrorCode ierr;
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       k,j,i,maxnmat,nmax;
  PetscReal      norm0,norm,*matnorm;
  PetscScalar    *s=ctx->s,*beta=ctx->beta,*xi=ctx->xi,*b,alpha,*coeffs,*pK,*pH,sone=1.0;
  Mat            T,P,Ts,K,H;
  PetscBool      shell,hasmnorm=PETSC_FALSE,matrix=PETSC_TRUE;
  PetscBLASInt   n_;

  PetscFunctionBegin;
  nmax = ctx->ddmaxit;
  CHKERRQ(PetscMalloc1(nep->nt*nmax,&ctx->coeffD));
  CHKERRQ(PetscMalloc3(nmax+1,&b,nmax+1,&coeffs,nep->nt,&matnorm));
  for (j=0;j<nep->nt;j++) {
    CHKERRQ(MatHasOperation(nep->A[j],MATOP_NORM,&hasmnorm));
    if (!hasmnorm) break;
    CHKERRQ(MatNorm(nep->A[j],NORM_INFINITY,matnorm+j));
  }
  /* Try matrix functions scheme */
  CHKERRQ(PetscCalloc2(nmax*nmax,&pK,nmax*nmax,&pH));
  for (i=0;i<nmax-1;i++) {
    pK[(nmax+1)*i]   = 1.0;
    pK[(nmax+1)*i+1] = beta[i+1]/xi[i];
    pH[(nmax+1)*i]   = s[i];
    pH[(nmax+1)*i+1] = beta[i+1];
  }
  pH[nmax*nmax-1] = s[nmax-1];
  pK[nmax*nmax-1] = 1.0;
  CHKERRQ(PetscBLASIntCast(nmax,&n_));
  PetscStackCallBLAS("BLAStrsm",BLAStrsm_("R","L","N","U",&n_,&n_,&sone,pK,&n_,pH,&n_));
  /* The matrix to be used is in H. K will be a work-space matrix */
  CHKERRQ(MatCreateSeqDense(PETSC_COMM_SELF,nmax,nmax,pH,&H));
  CHKERRQ(MatCreateSeqDense(PETSC_COMM_SELF,nmax,nmax,pK,&K));
  for (j=0;matrix&&j<nep->nt;j++) {
    PetscPushErrorHandler(PetscReturnErrorHandler,NULL);
    ierr = FNEvaluateFunctionMat(nep->f[j],H,K);
    PetscPopErrorHandler();
    if (!ierr) {
      for (i=0;i<nmax;i++) ctx->coeffD[j+i*nep->nt] = pK[i]*beta[0];
    } else {
      matrix = PETSC_FALSE;
      CHKERRQ(PetscFPTrapPop());
    }
  }
  CHKERRQ(MatDestroy(&H));
  CHKERRQ(MatDestroy(&K));
  if (!matrix) {
    for (j=0;j<nep->nt;j++) {
      CHKERRQ(FNEvaluateFunction(nep->f[j],s[0],ctx->coeffD+j));
      ctx->coeffD[j] *= beta[0];
    }
  }
  if (hasmnorm) {
    norm0 = 0.0;
    for (j=0;j<nep->nt;j++) norm0 += matnorm[j]*PetscAbsScalar(ctx->coeffD[j]);
  } else {
    norm0 = 0.0;
    for (j=0;j<nep->nt;j++) norm0 = PetscMax(PetscAbsScalar(ctx->coeffD[j]),norm0);
  }
  ctx->nmat = ctx->ddmaxit;
  for (k=1;k<ctx->ddmaxit;k++) {
    if (!matrix) {
      CHKERRQ(NEPNLEIGSEvalNRTFunct(nep,k,s[k],b));
      for (i=0;i<nep->nt;i++) {
        CHKERRQ(FNEvaluateFunction(nep->f[i],s[k],ctx->coeffD+k*nep->nt+i));
        for (j=0;j<k;j++) {
          ctx->coeffD[k*nep->nt+i] -= b[j]*ctx->coeffD[i+nep->nt*j];
        }
        ctx->coeffD[k*nep->nt+i] /= b[k];
      }
    }
    if (hasmnorm) {
      norm = 0.0;
      for (j=0;j<nep->nt;j++) norm += matnorm[j]*PetscAbsScalar(ctx->coeffD[k*nep->nt+j]);
    } else {
      norm = 0.0;
      for (j=0;j<nep->nt;j++) norm = PetscMax(PetscAbsScalar(ctx->coeffD[k*nep->nt+j]),norm);
    }
    if (k>1 && norm/norm0 < ctx->ddtol) {
      ctx->nmat = k+1;
      break;
    }
  }
  if (!ctx->ksp) CHKERRQ(NEPNLEIGSGetKSPs(nep,&ctx->nshiftsw,&ctx->ksp));
  CHKERRQ(MatIsShellAny(nep->A,nep->nt,&shell));
  maxnmat = PetscMax(ctx->ddmaxit,nep->nt);
  for (i=0;i<ctx->nshiftsw;i++) {
    CHKERRQ(NEPNLEIGSEvalNRTFunct(nep,ctx->nmat-1,ctx->shifts[i],coeffs));
    if (!shell) CHKERRQ(MatDuplicate(nep->A[0],MAT_COPY_VALUES,&T));
    else CHKERRQ(NLEIGSMatToMatShellArray(nep->A[0],&T,maxnmat));
    if (nep->P) { /* user-defined preconditioner */
      CHKERRQ(MatDuplicate(nep->P[0],MAT_COPY_VALUES,&P));
    } else P=T;
    alpha = 0.0;
    for (j=0;j<ctx->nmat;j++) alpha += coeffs[j]*ctx->coeffD[j*nep->nt];
    CHKERRQ(MatScale(T,alpha));
    if (nep->P) CHKERRQ(MatScale(P,alpha));
    for (k=1;k<nep->nt;k++) {
      alpha = 0.0;
      for (j=0;j<ctx->nmat;j++) alpha += coeffs[j]*ctx->coeffD[j*nep->nt+k];
      if (shell) CHKERRQ(NLEIGSMatToMatShellArray(nep->A[k],&Ts,maxnmat));
      CHKERRQ(MatAXPY(T,alpha,shell?Ts:nep->A[k],nep->mstr));
      if (nep->P) CHKERRQ(MatAXPY(P,alpha,nep->P[k],nep->mstrp));
      if (shell) CHKERRQ(MatDestroy(&Ts));
    }
    CHKERRQ(NEP_KSPSetOperators(ctx->ksp[i],T,P));
    CHKERRQ(KSPSetUp(ctx->ksp[i]));
    CHKERRQ(MatDestroy(&T));
    if (nep->P) CHKERRQ(MatDestroy(&P));
  }
  CHKERRQ(PetscFree3(b,coeffs,matnorm));
  CHKERRQ(PetscFree2(pK,pH));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSDividedDifferences_callback(NEP nep)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       k,j,i,maxnmat;
  PetscReal      norm0,norm;
  PetscScalar    *s=ctx->s,*beta=ctx->beta,*b,*coeffs;
  Mat            *D=ctx->D,*DP,T,P;
  PetscBool      shell,has,vec=PETSC_FALSE,precond=(nep->function_pre!=nep->function)?PETSC_TRUE:PETSC_FALSE;
  PetscRandom    rand=NULL;
  Vec            w[2];

  PetscFunctionBegin;
  CHKERRQ(PetscMalloc2(ctx->ddmaxit+1,&b,ctx->ddmaxit+1,&coeffs));
  if (nep->V) CHKERRQ(BVGetRandomContext(nep->V,&rand));
  T = nep->function;
  P = nep->function_pre;
  CHKERRQ(NEPComputeFunction(nep,s[0],T,P));
  CHKERRQ(MatIsShell(T,&shell));
  maxnmat = PetscMax(ctx->ddmaxit,nep->nt);
  if (!shell) CHKERRQ(MatDuplicate(T,MAT_COPY_VALUES,&D[0]));
  else CHKERRQ(NLEIGSMatToMatShellArray(T,&D[0],maxnmat));
  if (beta[0]!=1.0) CHKERRQ(MatScale(D[0],1.0/beta[0]));
  CHKERRQ(MatHasOperation(D[0],MATOP_NORM,&has));
  if (has) CHKERRQ(MatNorm(D[0],NORM_FROBENIUS,&norm0));
  else {
    CHKERRQ(MatCreateVecs(D[0],NULL,&w[0]));
    CHKERRQ(VecDuplicate(w[0],&w[1]));
    CHKERRQ(VecDuplicate(w[0],&ctx->vrn));
    CHKERRQ(VecSetRandomNormal(ctx->vrn,rand,w[0],w[1]));
    CHKERRQ(VecNormalize(ctx->vrn,NULL));
    vec = PETSC_TRUE;
    CHKERRQ(MatNormEstimate(D[0],ctx->vrn,w[0],&norm0));
  }
  if (precond) {
    CHKERRQ(PetscMalloc1(ctx->ddmaxit,&DP));
    CHKERRQ(MatDuplicate(P,MAT_COPY_VALUES,&DP[0]));
  }
  ctx->nmat = ctx->ddmaxit;
  for (k=1;k<ctx->ddmaxit;k++) {
    CHKERRQ(NEPNLEIGSEvalNRTFunct(nep,k,s[k],b));
    CHKERRQ(NEPComputeFunction(nep,s[k],T,P));
    if (!shell) CHKERRQ(MatDuplicate(T,MAT_COPY_VALUES,&D[k]));
    else CHKERRQ(NLEIGSMatToMatShellArray(T,&D[k],maxnmat));
    for (j=0;j<k;j++) CHKERRQ(MatAXPY(D[k],-b[j],D[j],nep->mstr));
    CHKERRQ(MatScale(D[k],1.0/b[k]));
    CHKERRQ(MatHasOperation(D[k],MATOP_NORM,&has));
    if (has) CHKERRQ(MatNorm(D[k],NORM_FROBENIUS,&norm));
    else {
      if (!vec) {
        CHKERRQ(MatCreateVecs(D[k],NULL,&w[0]));
        CHKERRQ(VecDuplicate(w[0],&w[1]));
        CHKERRQ(VecDuplicate(w[0],&ctx->vrn));
        CHKERRQ(VecSetRandomNormal(ctx->vrn,rand,w[0],w[1]));
        CHKERRQ(VecNormalize(ctx->vrn,NULL));
        vec = PETSC_TRUE;
      }
      CHKERRQ(MatNormEstimate(D[k],ctx->vrn,w[0],&norm));
    }
    if (precond) {
      CHKERRQ(MatDuplicate(P,MAT_COPY_VALUES,&DP[k]));
      for (j=0;j<k;j++) CHKERRQ(MatAXPY(DP[k],-b[j],DP[j],nep->mstrp));
      CHKERRQ(MatScale(DP[k],1.0/b[k]));
    }
    if (k>1 && norm/norm0 < ctx->ddtol && k>1) {
      ctx->nmat = k+1;
      break;
    }
  }
  if (!ctx->ksp) CHKERRQ(NEPNLEIGSGetKSPs(nep,&ctx->nshiftsw,&ctx->ksp));
  for (i=0;i<ctx->nshiftsw;i++) {
    CHKERRQ(NEPNLEIGSEvalNRTFunct(nep,ctx->nmat-1,ctx->shifts[i],coeffs));
    CHKERRQ(MatDuplicate(D[0],MAT_COPY_VALUES,&T));
    if (coeffs[0]!=1.0) CHKERRQ(MatScale(T,coeffs[0]));
    for (j=1;j<ctx->nmat;j++) CHKERRQ(MatAXPY(T,coeffs[j],D[j],nep->mstr));
    if (precond) {
      CHKERRQ(MatDuplicate(DP[0],MAT_COPY_VALUES,&P));
      if (coeffs[0]!=1.0) CHKERRQ(MatScale(P,coeffs[0]));
      for (j=1;j<ctx->nmat;j++) CHKERRQ(MatAXPY(P,coeffs[j],DP[j],nep->mstrp));
    } else P=T;
    CHKERRQ(NEP_KSPSetOperators(ctx->ksp[i],T,P));
    CHKERRQ(KSPSetUp(ctx->ksp[i]));
    CHKERRQ(MatDestroy(&T));
  }
  CHKERRQ(PetscFree2(b,coeffs));
  if (vec) {
    CHKERRQ(VecDestroy(&w[0]));
    CHKERRQ(VecDestroy(&w[1]));
  }
  if (precond) {
    CHKERRQ(MatDestroy(&P));
    CHKERRQ(MatDestroyMatrices(ctx->nmat,&DP));
  }
  PetscFunctionReturn(0);
}

/*
   NEPKrylovConvergence - This is the analogue to EPSKrylovConvergence.
*/
PetscErrorCode NEPNLEIGSKrylovConvergence(NEP nep,PetscBool getall,PetscInt kini,PetscInt nits,PetscReal betah,PetscScalar betak,PetscInt *kout,Vec *w)
{
  PetscInt       k,newk,marker,inside;
  PetscScalar    re,im;
  PetscReal      resnorm,tt;
  PetscBool      istrivial;
  NEP_NLEIGS     *ctx = (NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  CHKERRQ(RGIsTrivial(nep->rg,&istrivial));
  marker = -1;
  if (nep->trackall) getall = PETSC_TRUE;
  for (k=kini;k<kini+nits;k++) {
    /* eigenvalue */
    re = nep->eigr[k];
    im = nep->eigi[k];
    if (!istrivial) {
      if (!ctx->nshifts) CHKERRQ(NEPNLEIGSBackTransform((PetscObject)nep,1,&re,&im));
      CHKERRQ(RGCheckInside(nep->rg,1,&re,&im,&inside));
      if (marker==-1 && inside<0) marker = k;
    }
    newk = k;
    CHKERRQ(DSVectors(nep->ds,DS_MAT_X,&newk,&resnorm));
    tt = (ctx->nshifts)?SlepcAbsEigenvalue(betak-nep->eigr[k]*betah,nep->eigi[k]*betah):betah;
    resnorm *=  PetscAbsReal(tt);
    /* error estimate */
    CHKERRQ((*nep->converged)(nep,nep->eigr[k],nep->eigi[k],resnorm,&nep->errest[k],nep->convergedctx));
    if (marker==-1 && nep->errest[k] >= nep->tol) marker = k;
    if (newk==k+1) {
      nep->errest[k+1] = nep->errest[k];
      k++;
    }
    if (marker!=-1 && !getall) break;
  }
  if (marker!=-1) k = marker;
  *kout = k;
  PetscFunctionReturn(0);
}

PetscErrorCode NEPSetUp_NLEIGS(NEP nep)
{
  PetscInt       k,in;
  PetscScalar    zero=0.0;
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  SlepcSC        sc;
  PetscBool      istrivial;

  PetscFunctionBegin;
  CHKERRQ(NEPSetDimensions_Default(nep,nep->nev,&nep->ncv,&nep->mpd));
  PetscCheck(nep->ncv<=nep->nev+nep->mpd,PetscObjectComm((PetscObject)nep),PETSC_ERR_USER_INPUT,"The value of ncv must not be larger than nev+mpd");
  if (nep->max_it==PETSC_DEFAULT) nep->max_it = PetscMax(5000,2*nep->n/nep->ncv);
  if (!ctx->ddmaxit) ctx->ddmaxit = LBPOINTS;
  CHKERRQ(RGIsTrivial(nep->rg,&istrivial));
  PetscCheck(!istrivial,PetscObjectComm((PetscObject)nep),PETSC_ERR_SUP,"NEPNLEIGS requires a nontrivial region defining the target set");
  if (!nep->which) nep->which = NEP_TARGET_MAGNITUDE;
  PetscCheck(nep->which==NEP_TARGET_MAGNITUDE || nep->which==NEP_TARGET_REAL || nep->which==NEP_TARGET_IMAGINARY || nep->which==NEP_WHICH_USER,PetscObjectComm((PetscObject)nep),PETSC_ERR_SUP,"This solver supports only target selection of eigenvalues");

  /* Initialize the NLEIGS context structure */
  k = ctx->ddmaxit;
  CHKERRQ(PetscMalloc4(k,&ctx->s,k,&ctx->xi,k,&ctx->beta,k,&ctx->D));
  nep->data = ctx;
  if (nep->tol==PETSC_DEFAULT) nep->tol = SLEPC_DEFAULT_TOL;
  if (ctx->ddtol==PETSC_DEFAULT) ctx->ddtol = nep->tol/10.0;
  if (!ctx->keep) ctx->keep = 0.5;

  /* Compute Leja-Bagby points and scaling values */
  CHKERRQ(NEPNLEIGSLejaBagbyPoints(nep));
  if (nep->problem_type!=NEP_RATIONAL) {
    CHKERRQ(RGCheckInside(nep->rg,1,&nep->target,&zero,&in));
    PetscCheck(in>=0,PetscObjectComm((PetscObject)nep),PETSC_ERR_SUP,"The target is not inside the target set");
  }

  /* Compute the divided difference matrices */
  if (nep->fui==NEP_USER_INTERFACE_SPLIT) CHKERRQ(NEPNLEIGSDividedDifferences_split(nep));
  else CHKERRQ(NEPNLEIGSDividedDifferences_callback(nep));
  CHKERRQ(NEPAllocateSolution(nep,ctx->nmat-1));
  CHKERRQ(NEPSetWorkVecs(nep,4));
  if (!ctx->fullbasis) {
    PetscCheck(!nep->twosided,PetscObjectComm((PetscObject)nep),PETSC_ERR_SUP,"Two-sided variant requires the full-basis option, rerun with -nep_nleigs_full_basis");
    /* set-up DS and transfer split operator functions */
    CHKERRQ(DSSetType(nep->ds,ctx->nshifts?DSGNHEP:DSNHEP));
    CHKERRQ(DSAllocate(nep->ds,nep->ncv+1));
    CHKERRQ(DSGetSlepcSC(nep->ds,&sc));
    if (!ctx->nshifts) sc->map = NEPNLEIGSBackTransform;
    CHKERRQ(DSSetExtraRow(nep->ds,PETSC_TRUE));
    sc->mapobj        = (PetscObject)nep;
    sc->rg            = nep->rg;
    sc->comparison    = nep->sc->comparison;
    sc->comparisonctx = nep->sc->comparisonctx;
    CHKERRQ(BVDestroy(&ctx->V));
    CHKERRQ(BVCreateTensor(nep->V,ctx->nmat-1,&ctx->V));
    nep->ops->solve          = NEPSolve_NLEIGS;
    nep->ops->computevectors = NEPComputeVectors_Schur;
  } else {
    CHKERRQ(NEPSetUp_NLEIGS_FullBasis(nep));
    nep->ops->solve          = NEPSolve_NLEIGS_FullBasis;
    nep->ops->computevectors = NULL;
  }
  PetscFunctionReturn(0);
}

/*
  Extend the TOAR basis by applying the the matrix operator
  over a vector which is decomposed on the TOAR way
  Input:
    - S,V: define the latest Arnoldi vector (nv vectors in V)
  Output:
    - t: new vector extending the TOAR basis
    - r: temporally coefficients to compute the TOAR coefficients
         for the new Arnoldi vector
  Workspace: t_ (two vectors)
*/
static PetscErrorCode NEPTOARExtendBasis(NEP nep,PetscInt idxrktg,PetscScalar *S,PetscInt ls,PetscInt nv,BV W,BV V,Vec t,PetscScalar *r,PetscInt lr,Vec *t_)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       deg=ctx->nmat-1,k,j;
  Vec            v=t_[0],q=t_[1],w;
  PetscScalar    *beta=ctx->beta,*s=ctx->s,*xi=ctx->xi,*coeffs,sigma;

  PetscFunctionBegin;
  if (!ctx->ksp) CHKERRQ(NEPNLEIGSGetKSPs(nep,&ctx->nshiftsw,&ctx->ksp));
  sigma = ctx->shifts[idxrktg];
  CHKERRQ(BVSetActiveColumns(nep->V,0,nv));
  CHKERRQ(PetscMalloc1(ctx->nmat,&coeffs));
  PetscCheck(PetscAbsScalar(s[deg-2]-sigma)>100*PETSC_MACHINE_EPSILON,PETSC_COMM_SELF,PETSC_ERR_CONV_FAILED,"Breakdown in NLEIGS");
  /* i-part stored in (i-1) position */
  for (j=0;j<nv;j++) {
    r[(deg-2)*lr+j] = (S[(deg-2)*ls+j]+(beta[deg-1]/xi[deg-2])*S[(deg-1)*ls+j])/(s[deg-2]-sigma);
  }
  CHKERRQ(BVSetActiveColumns(W,0,deg));
  CHKERRQ(BVGetColumn(W,deg-1,&w));
  CHKERRQ(BVMultVec(V,1.0/beta[deg],0,w,S+(deg-1)*ls));
  CHKERRQ(BVRestoreColumn(W,deg-1,&w));
  CHKERRQ(BVGetColumn(W,deg-2,&w));
  CHKERRQ(BVMultVec(V,1.0,0.0,w,r+(deg-2)*lr));
  CHKERRQ(BVRestoreColumn(W,deg-2,&w));
  for (k=deg-2;k>0;k--) {
    PetscCheck(PetscAbsScalar(s[k-1]-sigma)>100*PETSC_MACHINE_EPSILON,PETSC_COMM_SELF,PETSC_ERR_CONV_FAILED,"Breakdown in NLEIGS");
    for (j=0;j<nv;j++) r[(k-1)*lr+j] = (S[(k-1)*ls+j]+(beta[k]/xi[k-1])*S[k*ls+j]-beta[k]*(1.0-sigma/xi[k-1])*r[(k)*lr+j])/(s[k-1]-sigma);
    CHKERRQ(BVGetColumn(W,k-1,&w));
    CHKERRQ(BVMultVec(V,1.0,0.0,w,r+(k-1)*lr));
    CHKERRQ(BVRestoreColumn(W,k-1,&w));
  }
  if (nep->fui==NEP_USER_INTERFACE_SPLIT) {
    for (j=0;j<ctx->nmat-2;j++) coeffs[j] = ctx->coeffD[nep->nt*j];
    coeffs[ctx->nmat-2] = ctx->coeffD[nep->nt*(ctx->nmat-1)];
    CHKERRQ(BVMultVec(W,1.0,0.0,v,coeffs));
    CHKERRQ(MatMult(nep->A[0],v,q));
    for (k=1;k<nep->nt;k++) {
      for (j=0;j<ctx->nmat-2;j++) coeffs[j] = ctx->coeffD[nep->nt*j+k];
      coeffs[ctx->nmat-2] = ctx->coeffD[nep->nt*(ctx->nmat-1)+k];
      CHKERRQ(BVMultVec(W,1.0,0,v,coeffs));
      CHKERRQ(MatMult(nep->A[k],v,t));
      CHKERRQ(VecAXPY(q,1.0,t));
    }
    CHKERRQ(KSPSolve(ctx->ksp[idxrktg],q,t));
    CHKERRQ(VecScale(t,-1.0));
  } else {
    for (k=0;k<deg-1;k++) {
      CHKERRQ(BVGetColumn(W,k,&w));
      CHKERRQ(MatMult(ctx->D[k],w,q));
      CHKERRQ(BVRestoreColumn(W,k,&w));
      CHKERRQ(BVInsertVec(W,k,q));
    }
    CHKERRQ(BVGetColumn(W,deg-1,&w));
    CHKERRQ(MatMult(ctx->D[deg],w,q));
    CHKERRQ(BVRestoreColumn(W,k,&w));
    CHKERRQ(BVInsertVec(W,k,q));
    for (j=0;j<ctx->nmat-1;j++) coeffs[j] = 1.0;
    CHKERRQ(BVMultVec(W,1.0,0.0,q,coeffs));
    CHKERRQ(KSPSolve(ctx->ksp[idxrktg],q,t));
    CHKERRQ(VecScale(t,-1.0));
  }
  CHKERRQ(PetscFree(coeffs));
  PetscFunctionReturn(0);
}

/*
  Compute TOAR coefficients of the blocks of the new Arnoldi vector computed
*/
static PetscErrorCode NEPTOARCoefficients(NEP nep,PetscScalar sigma,PetscInt nv,PetscScalar *S,PetscInt ls,PetscScalar *r,PetscInt lr,PetscScalar *x,PetscScalar *work)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       k,j,d=ctx->nmat-1;
  PetscScalar    *t=work;

  PetscFunctionBegin;
  CHKERRQ(NEPNLEIGSEvalNRTFunct(nep,d-1,sigma,t));
  for (k=0;k<d-1;k++) {
    for (j=0;j<=nv;j++) r[k*lr+j] += t[k]*x[j];
  }
  for (j=0;j<=nv;j++) r[(d-1)*lr+j] = t[d-1]*x[j];
  PetscFunctionReturn(0);
}

/*
  Compute continuation vector coefficients for the Rational-Krylov run.
  dim(work) >= (end-ini)*(end-ini+1) + end+1 + 2*(end-ini+1), dim(t) = end.
*/
static PetscErrorCode NEPNLEIGS_RKcontinuation(NEP nep,PetscInt ini,PetscInt end,PetscScalar *K,PetscScalar *H,PetscInt ld,PetscScalar sigma,PetscScalar *S,PetscInt lds,PetscScalar *cont,PetscScalar *t,PetscScalar *work)
{
  PetscScalar    *x,*W,*tau,sone=1.0,szero=0.0;
  PetscInt       i,j,n1,n,nwu=0;
  PetscBLASInt   info,n_,n1_,one=1,dim,lds_;
  NEP_NLEIGS     *ctx = (NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (!ctx->nshifts || !end) {
    t[0] = 1;
    CHKERRQ(PetscArraycpy(cont,S+end*lds,lds));
  } else {
    n   = end-ini;
    n1  = n+1;
    x   = work+nwu;
    nwu += end+1;
    tau = work+nwu;
    nwu += n;
    W   = work+nwu;
    nwu += n1*n;
    for (j=ini;j<end;j++) {
      for (i=ini;i<=end;i++) W[(j-ini)*n1+i-ini] = K[j*ld+i] -H[j*ld+i]*sigma;
    }
    CHKERRQ(PetscBLASIntCast(n,&n_));
    CHKERRQ(PetscBLASIntCast(n1,&n1_));
    CHKERRQ(PetscBLASIntCast(end+1,&dim));
    CHKERRQ(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
    PetscStackCallBLAS("LAPACKgeqrf",LAPACKgeqrf_(&n1_,&n_,W,&n1_,tau,work+nwu,&n1_,&info));
    SlepcCheckLapackInfo("geqrf",info);
    for (i=0;i<end;i++) t[i] = 0.0;
    t[end] = 1.0;
    for (j=n-1;j>=0;j--) {
      for (i=0;i<ini+j;i++) x[i] = 0.0;
      x[ini+j] = 1.0;
      for (i=j+1;i<n1;i++) x[i+ini] = W[i+n1*j];
      tau[j] = PetscConj(tau[j]);
      PetscStackCallBLAS("LAPACKlarf",LAPACKlarf_("L",&dim,&one,x,&one,tau+j,t,&dim,work+nwu));
    }
    CHKERRQ(PetscBLASIntCast(lds,&lds_));
    PetscStackCallBLAS("BLASgemv",BLASgemv_("N",&lds_,&n1_,&sone,S,&lds_,t,&one,&szero,cont,&one));
    CHKERRQ(PetscFPTrapPop());
  }
  PetscFunctionReturn(0);
}

/*
  Compute a run of Arnoldi iterations
*/
PetscErrorCode NEPNLEIGSTOARrun(NEP nep,PetscScalar *K,PetscScalar *H,PetscInt ldh,BV W,PetscInt k,PetscInt *M,PetscBool *breakdown,Vec *t_)
{
  NEP_NLEIGS     *ctx = (NEP_NLEIGS*)nep->data;
  PetscInt       i,j,m=*M,lwa,deg=ctx->nmat-1,lds,nqt,ld,l;
  Vec            t;
  PetscReal      norm;
  PetscScalar    *x,*work,*tt,sigma,*cont,*S;
  PetscBool      lindep;
  Mat            MS;

  PetscFunctionBegin;
  CHKERRQ(BVTensorGetFactors(ctx->V,NULL,&MS));
  CHKERRQ(MatDenseGetArray(MS,&S));
  CHKERRQ(BVGetSizes(nep->V,NULL,NULL,&ld));
  lds = ld*deg;
  CHKERRQ(BVGetActiveColumns(nep->V,&l,&nqt));
  lwa = PetscMax(ld,deg)+(m+1)*(m+1)+4*(m+1);
  CHKERRQ(PetscMalloc4(ld,&x,lwa,&work,m+1,&tt,lds,&cont));
  CHKERRQ(BVSetActiveColumns(ctx->V,0,m));
  for (j=k;j<m;j++) {
    sigma = ctx->shifts[(++(ctx->idxrk))%ctx->nshiftsw];

    /* Continuation vector */
    CHKERRQ(NEPNLEIGS_RKcontinuation(nep,0,j,K,H,ldh,sigma,S,lds,cont,tt,work));

    /* apply operator */
    CHKERRQ(BVGetColumn(nep->V,nqt,&t));
    CHKERRQ(NEPTOARExtendBasis(nep,(ctx->idxrk)%ctx->nshiftsw,cont,ld,nqt,W,nep->V,t,S+(j+1)*lds,ld,t_));
    CHKERRQ(BVRestoreColumn(nep->V,nqt,&t));

    /* orthogonalize */
    CHKERRQ(BVOrthogonalizeColumn(nep->V,nqt,x,&norm,&lindep));
    if (!lindep) {
      x[nqt] = norm;
      CHKERRQ(BVScaleColumn(nep->V,nqt,1.0/norm));
      nqt++;
    } else x[nqt] = 0.0;

    CHKERRQ(NEPTOARCoefficients(nep,sigma,nqt-1,cont,ld,S+(j+1)*lds,ld,x,work));

    /* Level-2 orthogonalization */
    CHKERRQ(BVOrthogonalizeColumn(ctx->V,j+1,H+j*ldh,&norm,breakdown));
    H[j+1+ldh*j] = norm;
    if (ctx->nshifts) {
      for (i=0;i<=j;i++) K[i+ldh*j] = sigma*H[i+ldh*j] + tt[i];
      K[j+1+ldh*j] = sigma*H[j+1+ldh*j];
    }
    if (*breakdown) {
      *M = j+1;
      break;
    }
    CHKERRQ(BVScaleColumn(ctx->V,j+1,1.0/norm));
    CHKERRQ(BVSetActiveColumns(nep->V,l,nqt));
  }
  CHKERRQ(PetscFree4(x,work,tt,cont));
  CHKERRQ(MatDenseRestoreArray(MS,&S));
  CHKERRQ(BVTensorRestoreFactors(ctx->V,NULL,&MS));
  PetscFunctionReturn(0);
}

PetscErrorCode NEPSolve_NLEIGS(NEP nep)
{
  NEP_NLEIGS        *ctx = (NEP_NLEIGS*)nep->data;
  PetscInt          i,k=0,l,nv=0,ld,lds,ldds,nq;
  PetscInt          deg=ctx->nmat-1,nconv=0,dsn,dsk;
  PetscScalar       *H,*pU,*K,betak=0,*eigr,*eigi;
  const PetscScalar *S;
  PetscReal         betah;
  PetscBool         falselock=PETSC_FALSE,breakdown=PETSC_FALSE;
  BV                W;
  Mat               MS,MQ,U;

  PetscFunctionBegin;
  if (ctx->lock) {
    /* undocumented option to use a cheaper locking instead of the true locking */
    CHKERRQ(PetscOptionsGetBool(NULL,NULL,"-nep_nleigs_falselocking",&falselock,NULL));
  }

  CHKERRQ(BVGetSizes(nep->V,NULL,NULL,&ld));
  lds = deg*ld;
  CHKERRQ(DSGetLeadingDimension(nep->ds,&ldds));
  if (!ctx->nshifts) CHKERRQ(PetscMalloc2(nep->ncv,&eigr,nep->ncv,&eigi));
  else { eigr = nep->eigr; eigi = nep->eigi; }
  CHKERRQ(BVDuplicateResize(nep->V,PetscMax(nep->nt-1,ctx->nmat-1),&W));

  /* clean projected matrix (including the extra-arrow) */
  CHKERRQ(DSGetArray(nep->ds,DS_MAT_A,&H));
  CHKERRQ(PetscArrayzero(H,ldds*ldds));
  CHKERRQ(DSRestoreArray(nep->ds,DS_MAT_A,&H));
  if (ctx->nshifts) {
    CHKERRQ(DSGetArray(nep->ds,DS_MAT_B,&H));
    CHKERRQ(PetscArrayzero(H,ldds*ldds));
    CHKERRQ(DSRestoreArray(nep->ds,DS_MAT_B,&H));
  }

  /* Get the starting Arnoldi vector */
  CHKERRQ(BVTensorBuildFirstColumn(ctx->V,nep->nini));

  /* Restart loop */
  l = 0;
  while (nep->reason == NEP_CONVERGED_ITERATING) {
    nep->its++;

    /* Compute an nv-step Krylov relation */
    nv = PetscMin(nep->nconv+nep->mpd,nep->ncv);
    if (ctx->nshifts) CHKERRQ(DSGetArray(nep->ds,DS_MAT_A,&K));
    CHKERRQ(DSGetArray(nep->ds,ctx->nshifts?DS_MAT_B:DS_MAT_A,&H));
    CHKERRQ(NEPNLEIGSTOARrun(nep,K,H,ldds,W,nep->nconv+l,&nv,&breakdown,nep->work));
    betah = PetscAbsScalar(H[(nv-1)*ldds+nv]);
    CHKERRQ(DSRestoreArray(nep->ds,ctx->nshifts?DS_MAT_B:DS_MAT_A,&H));
    if (ctx->nshifts) {
      betak = K[(nv-1)*ldds+nv];
      CHKERRQ(DSRestoreArray(nep->ds,DS_MAT_A,&K));
    }
    CHKERRQ(DSSetDimensions(nep->ds,nv,nep->nconv,nep->nconv+l));
    if (l==0) CHKERRQ(DSSetState(nep->ds,DS_STATE_INTERMEDIATE));
    else CHKERRQ(DSSetState(nep->ds,DS_STATE_RAW));

    /* Solve projected problem */
    CHKERRQ(DSSolve(nep->ds,nep->eigr,nep->eigi));
    CHKERRQ(DSSort(nep->ds,nep->eigr,nep->eigi,NULL,NULL,NULL));
    CHKERRQ(DSUpdateExtraRow(nep->ds));
    CHKERRQ(DSSynchronize(nep->ds,nep->eigr,nep->eigi));

    /* Check convergence */
    CHKERRQ(NEPNLEIGSKrylovConvergence(nep,PETSC_FALSE,nep->nconv,nv-nep->nconv,betah,betak,&k,nep->work));
    CHKERRQ((*nep->stopping)(nep,nep->its,nep->max_it,k,nep->nev,&nep->reason,nep->stoppingctx));

    /* Update l */
    if (nep->reason != NEP_CONVERGED_ITERATING || breakdown) l = 0;
    else {
      l = PetscMax(1,(PetscInt)((nv-k)*ctx->keep));
      CHKERRQ(DSGetTruncateSize(nep->ds,k,nv,&l));
      if (!breakdown) {
        /* Prepare the Rayleigh quotient for restart */
        CHKERRQ(DSGetDimensions(nep->ds,&dsn,NULL,&dsk,NULL));
        CHKERRQ(DSSetDimensions(nep->ds,dsn,k,dsk));
        CHKERRQ(DSTruncate(nep->ds,k+l,PETSC_FALSE));
      }
    }
    nconv = k;
    if (!ctx->lock && nep->reason == NEP_CONVERGED_ITERATING && !breakdown) { l += k; k = 0; }
    if (l) CHKERRQ(PetscInfo(nep,"Preparing to restart keeping l=%" PetscInt_FMT " vectors\n",l));

    /* Update S */
    CHKERRQ(DSGetMat(nep->ds,ctx->nshifts?DS_MAT_Z:DS_MAT_Q,&MQ));
    CHKERRQ(BVMultInPlace(ctx->V,MQ,nep->nconv,k+l));
    CHKERRQ(MatDestroy(&MQ));

    /* Copy last column of S */
    CHKERRQ(BVCopyColumn(ctx->V,nv,k+l));

    if (breakdown && nep->reason == NEP_CONVERGED_ITERATING) {
      /* Stop if breakdown */
      CHKERRQ(PetscInfo(nep,"Breakdown (it=%" PetscInt_FMT " norm=%g)\n",nep->its,(double)betah));
      nep->reason = NEP_DIVERGED_BREAKDOWN;
    }
    if (nep->reason != NEP_CONVERGED_ITERATING) l--;
    /* truncate S */
    CHKERRQ(BVGetActiveColumns(nep->V,NULL,&nq));
    if (k+l+deg<=nq) {
      CHKERRQ(BVSetActiveColumns(ctx->V,nep->nconv,k+l+1));
      if (!falselock && ctx->lock) CHKERRQ(BVTensorCompress(ctx->V,k-nep->nconv));
      else CHKERRQ(BVTensorCompress(ctx->V,0));
    }
    nep->nconv = k;
    if (!ctx->nshifts) {
      for (i=0;i<nv;i++) { eigr[i] = nep->eigr[i]; eigi[i] = nep->eigi[i]; }
      CHKERRQ(NEPNLEIGSBackTransform((PetscObject)nep,nv,eigr,eigi));
    }
    CHKERRQ(NEPMonitor(nep,nep->its,nconv,eigr,eigi,nep->errest,nv));
  }
  nep->nconv = nconv;
  if (nep->nconv>0) {
    CHKERRQ(BVSetActiveColumns(ctx->V,0,nep->nconv));
    CHKERRQ(BVGetActiveColumns(nep->V,NULL,&nq));
    CHKERRQ(BVSetActiveColumns(nep->V,0,nq));
    if (nq>nep->nconv) {
      CHKERRQ(BVTensorCompress(ctx->V,nep->nconv));
      CHKERRQ(BVSetActiveColumns(nep->V,0,nep->nconv));
      nq = nep->nconv;
    }
    if (ctx->nshifts) {
      CHKERRQ(DSGetMat(nep->ds,DS_MAT_B,&MQ));
      CHKERRQ(BVMultInPlace(ctx->V,MQ,0,nep->nconv));
      CHKERRQ(MatDestroy(&MQ));
    }
    CHKERRQ(BVTensorGetFactors(ctx->V,NULL,&MS));
    CHKERRQ(MatDenseGetArrayRead(MS,&S));
    CHKERRQ(PetscMalloc1(nq*nep->nconv,&pU));
    for (i=0;i<nep->nconv;i++) CHKERRQ(PetscArraycpy(pU+i*nq,S+i*lds,nq));
    CHKERRQ(MatDenseRestoreArrayRead(MS,&S));
    CHKERRQ(BVTensorRestoreFactors(ctx->V,NULL,&MS));
    CHKERRQ(MatCreateSeqDense(PETSC_COMM_SELF,nq,nep->nconv,pU,&U));
    CHKERRQ(BVSetActiveColumns(nep->V,0,nq));
    CHKERRQ(BVMultInPlace(nep->V,U,0,nep->nconv));
    CHKERRQ(BVSetActiveColumns(nep->V,0,nep->nconv));
    CHKERRQ(MatDestroy(&U));
    CHKERRQ(PetscFree(pU));
    CHKERRQ(DSTruncate(nep->ds,nep->nconv,PETSC_TRUE));
  }

  /* Map eigenvalues back to the original problem */
  if (!ctx->nshifts) {
    CHKERRQ(NEPNLEIGSBackTransform((PetscObject)nep,nep->nconv,nep->eigr,nep->eigi));
    CHKERRQ(PetscFree2(eigr,eigi));
  }
  CHKERRQ(BVDestroy(&W));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSSetSingularitiesFunction_NLEIGS(NEP nep,PetscErrorCode (*fun)(NEP,PetscInt*,PetscScalar*,void*),void *ctx)
{
  NEP_NLEIGS *nepctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (fun) nepctx->computesingularities = fun;
  if (ctx) nepctx->singularitiesctx     = ctx;
  nep->state = NEP_STATE_INITIAL;
  PetscFunctionReturn(0);
}

/*@C
   NEPNLEIGSSetSingularitiesFunction - Sets a user function to compute a discretization
   of the singularity set (where T(.) is not analytic).

   Logically Collective on nep

   Input Parameters:
+  nep - the NEP context
.  fun - user function (if NULL then NEP retains any previously set value)
-  ctx - [optional] user-defined context for private data for the function
         (may be NULL, in which case NEP retains any previously set value)

   Calling Sequence of fun:
$   fun(NEP nep,PetscInt *maxnp,PetscScalar *xi,void *ctx)

+   nep   - the NEP context
.   maxnp - on input number of requested points in the discretization (can be set)
.   xi    - computed values of the discretization
-   ctx   - optional context, as set by NEPNLEIGSSetSingularitiesFunction()

   Notes:
   The user-defined function can set a smaller value of maxnp if necessary.
   It is wrong to return a larger value.

   If the problem type has been set to rational with NEPSetProblemType(),
   then it is not necessary to set the singularities explicitly since the
   solver will try to determine them automatically.

   Level: intermediate

.seealso: NEPNLEIGSGetSingularitiesFunction(), NEPSetProblemType()
@*/
PetscErrorCode NEPNLEIGSSetSingularitiesFunction(NEP nep,PetscErrorCode (*fun)(NEP,PetscInt*,PetscScalar*,void*),void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSSetSingularitiesFunction_C",(NEP,PetscErrorCode(*)(NEP,PetscInt*,PetscScalar*,void*),void*),(nep,fun,ctx)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetSingularitiesFunction_NLEIGS(NEP nep,PetscErrorCode (**fun)(NEP,PetscInt*,PetscScalar*,void*),void **ctx)
{
  NEP_NLEIGS *nepctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (fun) *fun = nepctx->computesingularities;
  if (ctx) *ctx = nepctx->singularitiesctx;
  PetscFunctionReturn(0);
}

/*@C
   NEPNLEIGSGetSingularitiesFunction - Returns the Function and optionally the user
   provided context for computing a discretization of the singularity set.

   Not Collective

   Input Parameter:
.  nep - the nonlinear eigensolver context

   Output Parameters:
+  fun - location to put the function (or NULL)
-  ctx - location to stash the function context (or NULL)

   Level: advanced

.seealso: NEPNLEIGSSetSingularitiesFunction()
@*/
PetscErrorCode NEPNLEIGSGetSingularitiesFunction(NEP nep,PetscErrorCode (**fun)(NEP,PetscInt*,PetscScalar*,void*),void **ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  CHKERRQ(PetscUseMethod(nep,"NEPNLEIGSGetSingularitiesFunction_C",(NEP,PetscErrorCode(**)(NEP,PetscInt*,PetscScalar*,void*),void**),(nep,fun,ctx)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSSetRestart_NLEIGS(NEP nep,PetscReal keep)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (keep==PETSC_DEFAULT) ctx->keep = 0.5;
  else {
    PetscCheck(keep>=0.1 && keep<=0.9,PetscObjectComm((PetscObject)nep),PETSC_ERR_ARG_OUTOFRANGE,"The keep argument must be in the range [0.1,0.9]");
    ctx->keep = keep;
  }
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSSetRestart - Sets the restart parameter for the NLEIGS
   method, in particular the proportion of basis vectors that must be kept
   after restart.

   Logically Collective on nep

   Input Parameters:
+  nep  - the nonlinear eigensolver context
-  keep - the number of vectors to be kept at restart

   Options Database Key:
.  -nep_nleigs_restart - Sets the restart parameter

   Notes:
   Allowed values are in the range [0.1,0.9]. The default is 0.5.

   Level: advanced

.seealso: NEPNLEIGSGetRestart()
@*/
PetscErrorCode NEPNLEIGSSetRestart(NEP nep,PetscReal keep)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidLogicalCollectiveReal(nep,keep,2);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSSetRestart_C",(NEP,PetscReal),(nep,keep)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetRestart_NLEIGS(NEP nep,PetscReal *keep)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  *keep = ctx->keep;
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSGetRestart - Gets the restart parameter used in the NLEIGS method.

   Not Collective

   Input Parameter:
.  nep - the nonlinear eigensolver context

   Output Parameter:
.  keep - the restart parameter

   Level: advanced

.seealso: NEPNLEIGSSetRestart()
@*/
PetscErrorCode NEPNLEIGSGetRestart(NEP nep,PetscReal *keep)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidRealPointer(keep,2);
  CHKERRQ(PetscUseMethod(nep,"NEPNLEIGSGetRestart_C",(NEP,PetscReal*),(nep,keep)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSSetLocking_NLEIGS(NEP nep,PetscBool lock)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  ctx->lock = lock;
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSSetLocking - Choose between locking and non-locking variants of
   the NLEIGS method.

   Logically Collective on nep

   Input Parameters:
+  nep  - the nonlinear eigensolver context
-  lock - true if the locking variant must be selected

   Options Database Key:
.  -nep_nleigs_locking - Sets the locking flag

   Notes:
   The default is to lock converged eigenpairs when the method restarts.
   This behaviour can be changed so that all directions are kept in the
   working subspace even if already converged to working accuracy (the
   non-locking variant).

   Level: advanced

.seealso: NEPNLEIGSGetLocking()
@*/
PetscErrorCode NEPNLEIGSSetLocking(NEP nep,PetscBool lock)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidLogicalCollectiveBool(nep,lock,2);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSSetLocking_C",(NEP,PetscBool),(nep,lock)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetLocking_NLEIGS(NEP nep,PetscBool *lock)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  *lock = ctx->lock;
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSGetLocking - Gets the locking flag used in the NLEIGS method.

   Not Collective

   Input Parameter:
.  nep - the nonlinear eigensolver context

   Output Parameter:
.  lock - the locking flag

   Level: advanced

.seealso: NEPNLEIGSSetLocking()
@*/
PetscErrorCode NEPNLEIGSGetLocking(NEP nep,PetscBool *lock)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidBoolPointer(lock,2);
  CHKERRQ(PetscUseMethod(nep,"NEPNLEIGSGetLocking_C",(NEP,PetscBool*),(nep,lock)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSSetInterpolation_NLEIGS(NEP nep,PetscReal tol,PetscInt degree)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (tol == PETSC_DEFAULT) {
    ctx->ddtol = PETSC_DEFAULT;
    nep->state = NEP_STATE_INITIAL;
  } else {
    PetscCheck(tol>0.0,PetscObjectComm((PetscObject)nep),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of tol. Must be > 0");
    ctx->ddtol = tol;
  }
  if (degree == PETSC_DEFAULT || degree == PETSC_DECIDE) {
    ctx->ddmaxit = 0;
    if (nep->state) CHKERRQ(NEPReset(nep));
    nep->state = NEP_STATE_INITIAL;
  } else {
    PetscCheck(degree>0,PetscObjectComm((PetscObject)nep),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of degree. Must be > 0");
    if (ctx->ddmaxit != degree) {
      ctx->ddmaxit = degree;
      if (nep->state) CHKERRQ(NEPReset(nep));
      nep->state = NEP_STATE_INITIAL;
    }
  }
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSSetInterpolation - Sets the tolerance and maximum degree
   when building the interpolation via divided differences.

   Logically Collective on nep

   Input Parameters:
+  nep    - the nonlinear eigensolver context
.  tol    - tolerance to stop computing divided differences
-  degree - maximum degree of interpolation

   Options Database Key:
+  -nep_nleigs_interpolation_tol <tol> - Sets the tolerance to stop computing divided differences
-  -nep_nleigs_interpolation_degree <degree> - Sets the maximum degree of interpolation

   Notes:
   Use PETSC_DEFAULT for either argument to assign a reasonably good value.

   Level: advanced

.seealso: NEPNLEIGSGetInterpolation()
@*/
PetscErrorCode NEPNLEIGSSetInterpolation(NEP nep,PetscReal tol,PetscInt degree)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidLogicalCollectiveReal(nep,tol,2);
  PetscValidLogicalCollectiveInt(nep,degree,3);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSSetInterpolation_C",(NEP,PetscReal,PetscInt),(nep,tol,degree)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetInterpolation_NLEIGS(NEP nep,PetscReal *tol,PetscInt *degree)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (tol)    *tol    = ctx->ddtol;
  if (degree) *degree = ctx->ddmaxit;
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSGetInterpolation - Gets the tolerance and maximum degree
   when building the interpolation via divided differences.

   Not Collective

   Input Parameter:
.  nep - the nonlinear eigensolver context

   Output Parameters:
+  tol    - tolerance to stop computing divided differences
-  degree - maximum degree of interpolation

   Level: advanced

.seealso: NEPNLEIGSSetInterpolation()
@*/
PetscErrorCode NEPNLEIGSGetInterpolation(NEP nep,PetscReal *tol,PetscInt *degree)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSGetInterpolation_C",(NEP,PetscReal*,PetscInt*),(nep,tol,degree)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSSetRKShifts_NLEIGS(NEP nep,PetscInt ns,PetscScalar *shifts)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       i;

  PetscFunctionBegin;
  PetscCheck(ns>=0,PetscObjectComm((PetscObject)nep),PETSC_ERR_ARG_WRONG,"Number of shifts must be non-negative");
  if (ctx->nshifts) CHKERRQ(PetscFree(ctx->shifts));
  for (i=0;i<ctx->nshiftsw;i++) CHKERRQ(KSPDestroy(&ctx->ksp[i]));
  CHKERRQ(PetscFree(ctx->ksp));
  ctx->ksp = NULL;
  if (ns) {
    CHKERRQ(PetscMalloc1(ns,&ctx->shifts));
    for (i=0;i<ns;i++) ctx->shifts[i] = shifts[i];
  }
  ctx->nshifts = ns;
  nep->state   = NEP_STATE_INITIAL;
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSSetRKShifts - Sets a list of shifts to be used in the Rational
   Krylov method.

   Logically Collective on nep

   Input Parameters:
+  nep    - the nonlinear eigensolver context
.  ns     - number of shifts
-  shifts - array of scalar values specifying the shifts

   Options Database Key:
.  -nep_nleigs_rk_shifts - Sets the list of shifts

   Notes:
   If only one shift is provided, the built subspace built is equivalent to
   shift-and-invert Krylov-Schur (provided that the absolute convergence
   criterion is used).

   In the case of real scalars, complex shifts are not allowed. In the
   command line, a comma-separated list of complex values can be provided with
   the format [+/-][realnumber][+/-]realnumberi with no spaces, e.g.
   -nep_nleigs_rk_shifts 1.0+2.0i,1.5+2.0i,1.0+1.5i

   Use ns=0 to remove previously set shifts.

   Level: advanced

.seealso: NEPNLEIGSGetRKShifts()
@*/
PetscErrorCode NEPNLEIGSSetRKShifts(NEP nep,PetscInt ns,PetscScalar shifts[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidLogicalCollectiveInt(nep,ns,2);
  if (ns) PetscValidScalarPointer(shifts,3);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSSetRKShifts_C",(NEP,PetscInt,PetscScalar*),(nep,ns,shifts)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetRKShifts_NLEIGS(NEP nep,PetscInt *ns,PetscScalar **shifts)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscInt       i;

  PetscFunctionBegin;
  *ns = ctx->nshifts;
  if (ctx->nshifts) {
    CHKERRQ(PetscMalloc1(ctx->nshifts,shifts));
    for (i=0;i<ctx->nshifts;i++) (*shifts)[i] = ctx->shifts[i];
  }
  PetscFunctionReturn(0);
}

/*@C
   NEPNLEIGSGetRKShifts - Gets the list of shifts used in the Rational
   Krylov method.

   Not Collective

   Input Parameter:
.  nep - the nonlinear eigensolver context

   Output Parameters:
+  ns     - number of shifts
-  shifts - array of shifts

   Note:
   The user is responsible for deallocating the returned array.

   Level: advanced

.seealso: NEPNLEIGSSetRKShifts()
@*/
PetscErrorCode NEPNLEIGSGetRKShifts(NEP nep,PetscInt *ns,PetscScalar *shifts[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidIntPointer(ns,2);
  PetscValidPointer(shifts,3);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSGetRKShifts_C",(NEP,PetscInt*,PetscScalar**),(nep,ns,shifts)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetKSPs_NLEIGS(NEP nep,PetscInt *nsolve,KSP **ksp)
{
  NEP_NLEIGS     *ctx = (NEP_NLEIGS*)nep->data;
  PetscInt       i;
  PC             pc;

  PetscFunctionBegin;
  if (!ctx->ksp) {
    CHKERRQ(NEPNLEIGSSetShifts(nep,&ctx->nshiftsw));
    CHKERRQ(PetscMalloc1(ctx->nshiftsw,&ctx->ksp));
    for (i=0;i<ctx->nshiftsw;i++) {
      CHKERRQ(KSPCreate(PetscObjectComm((PetscObject)nep),&ctx->ksp[i]));
      CHKERRQ(PetscObjectIncrementTabLevel((PetscObject)ctx->ksp[i],(PetscObject)nep,1));
      CHKERRQ(KSPSetOptionsPrefix(ctx->ksp[i],((PetscObject)nep)->prefix));
      CHKERRQ(KSPAppendOptionsPrefix(ctx->ksp[i],"nep_nleigs_"));
      CHKERRQ(PetscLogObjectParent((PetscObject)nep,(PetscObject)ctx->ksp[i]));
      CHKERRQ(PetscObjectSetOptions((PetscObject)ctx->ksp[i],((PetscObject)nep)->options));
      CHKERRQ(KSPSetErrorIfNotConverged(ctx->ksp[i],PETSC_TRUE));
      CHKERRQ(KSPSetTolerances(ctx->ksp[i],1e-3*SlepcDefaultTol(nep->tol),PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT));
      CHKERRQ(KSPGetPC(ctx->ksp[i],&pc));
      if ((nep->fui==NEP_USER_INTERFACE_SPLIT && nep->P) || (nep->fui==NEP_USER_INTERFACE_CALLBACK && nep->function_pre!=nep->function)) {
        CHKERRQ(KSPSetType(ctx->ksp[i],KSPBCGS));
        CHKERRQ(PCSetType(pc,PCBJACOBI));
      } else {
        CHKERRQ(KSPSetType(ctx->ksp[i],KSPPREONLY));
        CHKERRQ(PCSetType(pc,PCLU));
      }
    }
  }
  if (nsolve) *nsolve = ctx->nshiftsw;
  if (ksp)    *ksp    = ctx->ksp;
  PetscFunctionReturn(0);
}

/*@C
   NEPNLEIGSGetKSPs - Retrieve the array of linear solver objects associated with
   the nonlinear eigenvalue solver.

   Not Collective

   Input Parameter:
.  nep - nonlinear eigenvalue solver

   Output Parameters:
+  nsolve - number of returned KSP objects
-  ksp - array of linear solver object

   Notes:
   The number of KSP objects is equal to the number of shifts provided by the user,
   or 1 if the user did not provide shifts.

   Level: advanced

.seealso: NEPNLEIGSSetRKShifts()
@*/
PetscErrorCode NEPNLEIGSGetKSPs(NEP nep,PetscInt *nsolve,KSP **ksp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  CHKERRQ(PetscUseMethod(nep,"NEPNLEIGSGetKSPs_C",(NEP,PetscInt*,KSP**),(nep,nsolve,ksp)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSSetFullBasis_NLEIGS(NEP nep,PetscBool fullbasis)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (fullbasis!=ctx->fullbasis) {
    ctx->fullbasis = fullbasis;
    nep->state     = NEP_STATE_INITIAL;
    nep->useds     = PetscNot(fullbasis);
  }
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSSetFullBasis - Choose between TOAR-basis (default) and full-basis
   variants of the NLEIGS method.

   Logically Collective on nep

   Input Parameters:
+  nep  - the nonlinear eigensolver context
-  fullbasis - true if the full-basis variant must be selected

   Options Database Key:
.  -nep_nleigs_full_basis - Sets the full-basis flag

   Notes:
   The default is to use a compact representation of the Krylov basis, that is,
   V = (I otimes U) S, with a tensor BV. This behaviour can be changed so that
   the full basis V is explicitly stored and operated with. This variant is more
   expensive in terms of memory and computation, but is necessary in some cases,
   particularly for two-sided computations, see NEPSetTwoSided().

   In the full-basis variant, the NLEIGS solver uses an EPS object to explicitly
   solve the linearized eigenproblem, see NEPNLEIGSGetEPS().

   Level: advanced

.seealso: NEPNLEIGSGetFullBasis(), NEPNLEIGSGetEPS(), NEPSetTwoSided(), BVCreateTensor()
@*/
PetscErrorCode NEPNLEIGSSetFullBasis(NEP nep,PetscBool fullbasis)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidLogicalCollectiveBool(nep,fullbasis,2);
  CHKERRQ(PetscTryMethod(nep,"NEPNLEIGSSetFullBasis_C",(NEP,PetscBool),(nep,fullbasis)));
  PetscFunctionReturn(0);
}

static PetscErrorCode NEPNLEIGSGetFullBasis_NLEIGS(NEP nep,PetscBool *fullbasis)
{
  NEP_NLEIGS *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  *fullbasis = ctx->fullbasis;
  PetscFunctionReturn(0);
}

/*@
   NEPNLEIGSGetFullBasis - Gets the flag that indicates if NLEIGS is using the
   full-basis variant.

   Not Collective

   Input Parameter:
.  nep - the nonlinear eigensolver context

   Output Parameter:
.  fullbasis - the flag

   Level: advanced

.seealso: NEPNLEIGSSetFullBasis()
@*/
PetscErrorCode NEPNLEIGSGetFullBasis(NEP nep,PetscBool *fullbasis)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(nep,NEP_CLASSID,1);
  PetscValidBoolPointer(fullbasis,2);
  CHKERRQ(PetscUseMethod(nep,"NEPNLEIGSGetFullBasis_C",(NEP,PetscBool*),(nep,fullbasis)));
  PetscFunctionReturn(0);
}

#define SHIFTMAX 30

PetscErrorCode NEPSetFromOptions_NLEIGS(PetscOptionItems *PetscOptionsObject,NEP nep)
{
  NEP_NLEIGS     *ctx = (NEP_NLEIGS*)nep->data;
  PetscInt       i=0,k;
  PetscBool      flg1,flg2,b;
  PetscReal      r;
  PetscScalar    array[SHIFTMAX];

  PetscFunctionBegin;
  CHKERRQ(PetscOptionsHead(PetscOptionsObject,"NEP NLEIGS Options"));

    CHKERRQ(PetscOptionsReal("-nep_nleigs_restart","Proportion of vectors kept after restart","NEPNLEIGSSetRestart",0.5,&r,&flg1));
    if (flg1) CHKERRQ(NEPNLEIGSSetRestart(nep,r));

    CHKERRQ(PetscOptionsBool("-nep_nleigs_locking","Choose between locking and non-locking variants","NEPNLEIGSSetLocking",PETSC_FALSE,&b,&flg1));
    if (flg1) CHKERRQ(NEPNLEIGSSetLocking(nep,b));

    CHKERRQ(PetscOptionsBool("-nep_nleigs_full_basis","Choose between TOAR and full-basis variants","NEPNLEIGSSetFullBasis",PETSC_FALSE,&b,&flg1));
    if (flg1) CHKERRQ(NEPNLEIGSSetFullBasis(nep,b));

    CHKERRQ(NEPNLEIGSGetInterpolation(nep,&r,&i));
    if (!i) i = PETSC_DEFAULT;
    CHKERRQ(PetscOptionsInt("-nep_nleigs_interpolation_degree","Maximum number of terms for interpolation via divided differences","NEPNLEIGSSetInterpolation",i,&i,&flg1));
    CHKERRQ(PetscOptionsReal("-nep_nleigs_interpolation_tol","Tolerance for interpolation via divided differences","NEPNLEIGSSetInterpolation",r,&r,&flg2));
    if (flg1 || flg2) CHKERRQ(NEPNLEIGSSetInterpolation(nep,r,i));

    k = SHIFTMAX;
    for (i=0;i<k;i++) array[i] = 0;
    CHKERRQ(PetscOptionsScalarArray("-nep_nleigs_rk_shifts","Shifts for Rational Krylov","NEPNLEIGSSetRKShifts",array,&k,&flg1));
    if (flg1) CHKERRQ(NEPNLEIGSSetRKShifts(nep,k,array));

  CHKERRQ(PetscOptionsTail());

  if (!ctx->ksp) CHKERRQ(NEPNLEIGSGetKSPs(nep,&ctx->nshiftsw,&ctx->ksp));
  for (i=0;i<ctx->nshiftsw;i++) CHKERRQ(KSPSetFromOptions(ctx->ksp[i]));

  if (ctx->fullbasis) {
    if (!ctx->eps) CHKERRQ(NEPNLEIGSGetEPS(nep,&ctx->eps));
    CHKERRQ(EPSSetFromOptions(ctx->eps));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode NEPView_NLEIGS(NEP nep,PetscViewer viewer)
{
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;
  PetscBool      isascii;
  PetscInt       i;
  char           str[50];

  PetscFunctionBegin;
  CHKERRQ(PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii));
  if (isascii) {
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  %d%% of basis vectors kept after restart\n",(int)(100*ctx->keep)));
    if (ctx->fullbasis) CHKERRQ(PetscViewerASCIIPrintf(viewer,"  using the full-basis variant\n"));
    else CHKERRQ(PetscViewerASCIIPrintf(viewer,"  using the %slocking variant\n",ctx->lock?"":"non-"));
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  divided difference terms: used=%" PetscInt_FMT ", max=%" PetscInt_FMT "\n",ctx->nmat,ctx->ddmaxit));
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  tolerance for divided difference convergence: %g\n",(double)ctx->ddtol));
    if (ctx->nshifts) {
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"  RK shifts: "));
      CHKERRQ(PetscViewerASCIIUseTabs(viewer,PETSC_FALSE));
      for (i=0;i<ctx->nshifts;i++) {
        CHKERRQ(SlepcSNPrintfScalar(str,sizeof(str),ctx->shifts[i],PETSC_FALSE));
        CHKERRQ(PetscViewerASCIIPrintf(viewer,"%s%s",str,(i<ctx->nshifts-1)?",":""));
      }
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"\n"));
      CHKERRQ(PetscViewerASCIIUseTabs(viewer,PETSC_TRUE));
    }
    if (!ctx->ksp) CHKERRQ(NEPNLEIGSGetKSPs(nep,&ctx->nshiftsw,&ctx->ksp));
    CHKERRQ(PetscViewerASCIIPushTab(viewer));
    CHKERRQ(KSPView(ctx->ksp[0],viewer));
    CHKERRQ(PetscViewerASCIIPopTab(viewer));
    if (ctx->fullbasis) {
      if (!ctx->eps) CHKERRQ(NEPNLEIGSGetEPS(nep,&ctx->eps));
      CHKERRQ(PetscViewerASCIIPushTab(viewer));
      CHKERRQ(EPSView(ctx->eps,viewer));
      CHKERRQ(PetscViewerASCIIPopTab(viewer));
    }
  }
  PetscFunctionReturn(0);
}

PetscErrorCode NEPReset_NLEIGS(NEP nep)
{
  PetscInt       k;
  NEP_NLEIGS     *ctx=(NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  if (nep->fui==NEP_USER_INTERFACE_SPLIT) CHKERRQ(PetscFree(ctx->coeffD));
  else {
    for (k=0;k<ctx->nmat;k++) CHKERRQ(MatDestroy(&ctx->D[k]));
  }
  CHKERRQ(PetscFree4(ctx->s,ctx->xi,ctx->beta,ctx->D));
  for (k=0;k<ctx->nshiftsw;k++) CHKERRQ(KSPReset(ctx->ksp[k]));
  CHKERRQ(VecDestroy(&ctx->vrn));
  if (ctx->fullbasis) {
    CHKERRQ(MatDestroy(&ctx->A));
    CHKERRQ(EPSReset(ctx->eps));
    for (k=0;k<4;k++) CHKERRQ(VecDestroy(&ctx->w[k]));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode NEPDestroy_NLEIGS(NEP nep)
{
  PetscInt       k;
  NEP_NLEIGS     *ctx = (NEP_NLEIGS*)nep->data;

  PetscFunctionBegin;
  CHKERRQ(BVDestroy(&ctx->V));
  for (k=0;k<ctx->nshiftsw;k++) CHKERRQ(KSPDestroy(&ctx->ksp[k]));
  CHKERRQ(PetscFree(ctx->ksp));
  if (ctx->nshifts) CHKERRQ(PetscFree(ctx->shifts));
  if (ctx->fullbasis) CHKERRQ(EPSDestroy(&ctx->eps));
  CHKERRQ(PetscFree(nep->data));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetSingularitiesFunction_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetSingularitiesFunction_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetRestart_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetRestart_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetLocking_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetLocking_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetInterpolation_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetInterpolation_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetRKShifts_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetRKShifts_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetKSPs_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetFullBasis_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetFullBasis_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetEPS_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetEPS_C",NULL));
  PetscFunctionReturn(0);
}

SLEPC_EXTERN PetscErrorCode NEPCreate_NLEIGS(NEP nep)
{
  NEP_NLEIGS     *ctx;

  PetscFunctionBegin;
  CHKERRQ(PetscNewLog(nep,&ctx));
  nep->data  = (void*)ctx;
  ctx->lock  = PETSC_TRUE;
  ctx->ddtol = PETSC_DEFAULT;

  nep->useds = PETSC_TRUE;

  nep->ops->setup          = NEPSetUp_NLEIGS;
  nep->ops->setfromoptions = NEPSetFromOptions_NLEIGS;
  nep->ops->view           = NEPView_NLEIGS;
  nep->ops->destroy        = NEPDestroy_NLEIGS;
  nep->ops->reset          = NEPReset_NLEIGS;

  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetSingularitiesFunction_C",NEPNLEIGSSetSingularitiesFunction_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetSingularitiesFunction_C",NEPNLEIGSGetSingularitiesFunction_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetRestart_C",NEPNLEIGSSetRestart_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetRestart_C",NEPNLEIGSGetRestart_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetLocking_C",NEPNLEIGSSetLocking_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetLocking_C",NEPNLEIGSGetLocking_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetInterpolation_C",NEPNLEIGSSetInterpolation_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetInterpolation_C",NEPNLEIGSGetInterpolation_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetRKShifts_C",NEPNLEIGSSetRKShifts_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetRKShifts_C",NEPNLEIGSGetRKShifts_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetKSPs_C",NEPNLEIGSGetKSPs_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetFullBasis_C",NEPNLEIGSSetFullBasis_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetFullBasis_C",NEPNLEIGSGetFullBasis_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSSetEPS_C",NEPNLEIGSSetEPS_NLEIGS));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)nep,"NEPNLEIGSGetEPS_C",NEPNLEIGSGetEPS_NLEIGS));
  PetscFunctionReturn(0);
}
