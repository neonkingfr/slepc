/*                       

   SLEPc eigensolver: "lanczos"

   Method: Explicitly Restarted Symmetric/Hermitian Lanczos

   Algorithm:

       Lanczos method for symmetric (Hermitian) problems, with explicit 
       restart and deflation. Several reorthogonalization strategies can
       be selected.

   References:

       [1] "Lanczos Methods in SLEPc", SLEPc Technical Report STR-5, 
           available at http://www.grycap.upv.es/slepc.

   Last update: Feb 2009

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2009, Universidad Politecnica de Valencia, Spain

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

#include "private/epsimpl.h"                /*I "slepceps.h" I*/

typedef struct {
  EPSLanczosReorthogType reorthog;
} EPS_LANCZOS;

#undef __FUNCT__  
#define __FUNCT__ "EPSSetUp_LANCZOS"
PetscErrorCode EPSSetUp_LANCZOS(EPS eps)
{
  EPS_LANCZOS    *lanczos = (EPS_LANCZOS *)eps->data;
  PetscErrorCode ierr;
  PetscInt       N;

  PetscFunctionBegin;
  ierr = VecGetSize(eps->vec_initial,&N);CHKERRQ(ierr);
  if (eps->ncv) { /* ncv set */
    if (eps->ncv<eps->nev) SETERRQ(1,"The value of ncv must be at least nev"); 
  }
  else if (eps->mpd) { /* mpd set */
    eps->ncv = PetscMin(N,eps->nev+eps->mpd);
  }
  else { /* neither set: defaults depend on nev being small or large */
    if (eps->nev<500) eps->ncv = PetscMin(N,PetscMax(2*eps->nev,eps->nev+15));
    else { eps->mpd = 500; eps->ncv = PetscMin(N,eps->nev+eps->mpd); }
  }
  if (!eps->mpd) eps->mpd = eps->ncv;
  if (eps->ncv>eps->nev+eps->mpd) SETERRQ(1,"The value of ncv must not be larger than nev+mpd"); 
  if (!eps->max_it) eps->max_it = PetscMax(100,2*N/eps->ncv);

  if (eps->solverclass==EPS_ONE_SIDE) {
    if (eps->which == EPS_LARGEST_IMAGINARY || eps->which == EPS_SMALLEST_IMAGINARY)
      SETERRQ(1,"Wrong value of eps->which");
    if (!eps->ishermitian)
      SETERRQ(PETSC_ERR_SUP,"Requested method is only available for Hermitian problems");
  } else {
    if (eps->which != EPS_LARGEST_MAGNITUDE)
      SETERRQ(1,"Wrong value of eps->which");
  }
  if (!eps->extraction) {
    ierr = EPSSetExtraction(eps,EPS_RITZ);CHKERRQ(ierr);
  } else if (eps->extraction!=EPS_RITZ) {
    SETERRQ(PETSC_ERR_SUP,"Unsupported extraction type\n");
  }

  ierr = EPSAllocateSolution(eps);CHKERRQ(ierr);
  if (lanczos->reorthog == EPSLANCZOS_REORTHOG_SELECTIVE) {
    ierr = VecDuplicateVecs(eps->vec_initial,eps->ncv,&eps->AV);CHKERRQ(ierr);
  }
  if (eps->solverclass==EPS_TWO_SIDE) {
    ierr = PetscFree(eps->Tl);CHKERRQ(ierr);
    ierr = PetscMalloc(eps->ncv*eps->ncv*sizeof(PetscScalar),&eps->Tl);CHKERRQ(ierr);
  }
  ierr = EPSDefaultGetWork(eps,2);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSLocalLanczos"
/*
   EPSLocalLanczos - Local reorthogonalization.

   This is the simplest variant. At each Lanczos step, the corresponding Lanczos vector 
   is orthogonalized with respect to the two previous Lanczos vectors, according to
   the three term Lanczos recurrence. WARNING: This variant does not track the loss of 
   orthogonality that occurs in finite-precision arithmetic and, therefore, the 
   generated vectors are not guaranteed to be (semi-)orthogonal.
*/
static PetscErrorCode EPSLocalLanczos(EPS eps,PetscReal *alpha,PetscReal *beta,Vec *V,PetscInt k,PetscInt *M,Vec f,PetscTruth *breakdown)
{
  PetscErrorCode ierr;
  PetscInt       i,j,m = *M;
  PetscReal      norm;
  PetscTruth     *which,lwhich[100];
  PetscScalar    *swork,*hwork,lhwork[100];
  
  PetscFunctionBegin;  
  if (eps->nds+m > 100) {
    ierr = PetscMalloc(sizeof(PetscTruth)*(eps->nds+m),&which);CHKERRQ(ierr);
    ierr = PetscMalloc((eps->nds+m)*sizeof(PetscScalar),&swork);CHKERRQ(ierr);
    ierr = PetscMalloc((eps->nds+m)*sizeof(PetscScalar),&hwork);CHKERRQ(ierr);
  } else {
    which = lwhich;
    swork = PETSC_NULL;
    hwork = lhwork;
  }
  for (i=0;i<eps->nds+k;i++)
    which[i] = PETSC_TRUE;

  for (j=k;j<m-1;j++) {
    ierr = STApply(eps->OP,V[j],V[j+1]);CHKERRQ(ierr);
    which[eps->nds+j] = PETSC_TRUE;
    if (j-2>=k) which[eps->nds+j-2] = PETSC_FALSE;
    ierr = IPOrthogonalize(eps->ip,eps->nds+j+1,which,eps->DSV,V[j+1],hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);
    alpha[j-k] = PetscRealPart(hwork[eps->nds+j]);
    beta[j-k] = norm;
    if (*breakdown) {
      *M = j+1;
      if (eps->nds+m > 100) {
        ierr = PetscFree(which);CHKERRQ(ierr);
        ierr = PetscFree(swork);CHKERRQ(ierr);
        ierr = PetscFree(hwork);CHKERRQ(ierr);
      }
      PetscFunctionReturn(0);
    } else {
      ierr = VecScale(V[j+1],1.0/norm);CHKERRQ(ierr);
    }
  }
  ierr = STApply(eps->OP,V[m-1],f);CHKERRQ(ierr);
  ierr = IPOrthogonalize(eps->ip,eps->nds+m,PETSC_NULL,eps->DSV,f,hwork,&norm,PETSC_NULL,eps->work[0],swork);CHKERRQ(ierr);
  alpha[m-1-k] = PetscRealPart(hwork[eps->nds+m-1]); 
  beta[m-1-k] = norm;

  if (eps->nds+m > 100) {
    ierr = PetscFree(which);CHKERRQ(ierr);
    ierr = PetscFree(swork);CHKERRQ(ierr);
    ierr = PetscFree(hwork);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSSelectiveLanczos"
/*
   EPSSelectiveLanczos - Selective reorthogonalization.
*/
static PetscErrorCode EPSSelectiveLanczos(EPS eps,PetscReal *alpha,PetscReal *beta,Vec *V,PetscInt k,PetscInt *M,Vec f,PetscTruth *breakdown,PetscReal anorm)
{
  PetscErrorCode ierr;
  PetscInt       i,j,m = *M,n,nritz=0,nritzo;
  PetscReal      *d,*e,*ritz,norm;
  PetscScalar    *Y,*swork,*hwork,lhwork[100];
  PetscTruth     *which,lwhich[100];

  PetscFunctionBegin;
  ierr = PetscMalloc(m*sizeof(PetscReal),&d);CHKERRQ(ierr);
  ierr = PetscMalloc(m*sizeof(PetscReal),&e);CHKERRQ(ierr);
  ierr = PetscMalloc(m*sizeof(PetscReal),&ritz);CHKERRQ(ierr);
  ierr = PetscMalloc(m*m*sizeof(PetscScalar),&Y);CHKERRQ(ierr);
  if (eps->nds+m > 100) {
    ierr = PetscMalloc(sizeof(PetscTruth)*(eps->nds+m),&which);CHKERRQ(ierr);
    ierr = PetscMalloc((eps->nds+m)*sizeof(PetscScalar),&swork);CHKERRQ(ierr);
    ierr = PetscMalloc((eps->nds+m)*sizeof(PetscScalar),&hwork);CHKERRQ(ierr);
  } else {
    which = lwhich;
    swork = PETSC_NULL;
    hwork = lhwork;
  }
  for (i=0;i<eps->nds+k;i++)
    which[i] = PETSC_TRUE;

  for (j=k;j<m;j++) {
    /* Lanczos step */
    ierr = STApply(eps->OP,V[j],f);CHKERRQ(ierr);
    which[eps->nds+j] = PETSC_TRUE;
    if (j-2>=k) which[eps->nds+j-2] = PETSC_FALSE;
    ierr = IPOrthogonalize(eps->ip,eps->nds+j+1,which,eps->DSV,f,hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);
    alpha[j-k] = PetscRealPart(hwork[eps->nds+j]);
    beta[j-k] = norm;
    if (*breakdown) {
      *M = j+1;
      break;
    }

    /* Compute eigenvalues and eigenvectors Y of the tridiagonal block */
    n = j-k+1;
    for (i=0;i<n;i++) { d[i] = alpha[i]; e[i] = beta[i]; }
    ierr = EPSDenseTridiagonal(n,d,e,ritz,Y);CHKERRQ(ierr);
    
    /* Estimate ||A|| */
    for (i=0;i<n;i++) 
      if (PetscAbsReal(ritz[i]) > anorm) anorm = PetscAbsReal(ritz[i]);

    /* Compute nearly converged Ritz vectors */
    nritzo = 0;
    for (i=0;i<n;i++)
      if (norm*PetscAbsScalar(Y[i*n+n-1]) < PETSC_SQRT_MACHINE_EPSILON*anorm)
	nritzo++;

    if (nritzo>nritz) {
      nritz = 0;
      for (i=0;i<n;i++) {
	if (norm*PetscAbsScalar(Y[i*n+n-1]) < PETSC_SQRT_MACHINE_EPSILON*anorm) {
	  ierr = VecSet(eps->AV[nritz],0.0);CHKERRQ(ierr);
	  ierr = VecMAXPY(eps->AV[nritz],n,Y+i*n,V+k);CHKERRQ(ierr);
          nritz++;
	}
      }
    }

    if (nritz > 0) {
      ierr = IPOrthogonalize(eps->ip,nritz,PETSC_NULL,eps->AV,f,hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);
      if (*breakdown) {
	*M = j+1;
	break;
      }
    }
    
    if (j<m-1) {
      ierr = VecScale(f,1.0 / norm);CHKERRQ(ierr);
      ierr = VecCopy(f,V[j+1]);CHKERRQ(ierr);
    }
  }
  
  ierr = PetscFree(d);CHKERRQ(ierr);
  ierr = PetscFree(e);CHKERRQ(ierr);
  ierr = PetscFree(ritz);CHKERRQ(ierr);
  ierr = PetscFree(Y);CHKERRQ(ierr);
  if (eps->nds+m > 100) {
    ierr = PetscFree(which);CHKERRQ(ierr);
    ierr = PetscFree(swork);CHKERRQ(ierr);
    ierr = PetscFree(hwork);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "update_omega"
static void update_omega(PetscReal *omega,PetscReal *omega_old,PetscInt j,PetscReal *alpha,PetscReal *beta,PetscReal eps1,PetscReal anorm)
{
  PetscInt       k;
  PetscReal      T,binv,temp;

  PetscFunctionBegin;
  /* Estimate of contribution to roundoff errors from A*v 
       fl(A*v) = A*v + f, 
     where ||f|| \approx eps1*||A||.
     For a full matrix A, a rule-of-thumb estimate is eps1 = sqrt(n)*eps. */
  T = eps1*anorm;
  binv = 1.0/beta[j+1];

  /* Update omega(1) using omega(0)==0. */
  omega_old[0]= beta[1]*omega[1] + (alpha[0]-alpha[j])*omega[0] - 
                beta[j]*omega_old[0];
  if (omega_old[0] > 0) 
    omega_old[0] = binv*(omega_old[0] + T);
  else
    omega_old[0] = binv*(omega_old[0] - T);  
  
  /* Update remaining components. */
  for (k=1;k<j-1;k++) {
    omega_old[k] = beta[k+1]*omega[k+1] + (alpha[k]-alpha[j])*omega[k] +
                   beta[k]*omega[k-1] - beta[j]*omega_old[k];
    if (omega_old[k] > 0) 
      omega_old[k] = binv*(omega_old[k] + T);       
    else
      omega_old[k] = binv*(omega_old[k] - T);       
  }
  omega_old[j-1] = binv*T;
  
  /* Swap omega and omega_old. */
  for (k=0;k<j;k++) {
    temp = omega[k];
    omega[k] = omega_old[k];
    omega_old[k] = omega[k];
  }
  omega[j] = eps1;  
  PetscFunctionReturnVoid();
}

#undef __FUNCT__  
#define __FUNCT__ "compute_int"
static void compute_int(PetscTruth *which,PetscReal *mu,PetscInt j,PetscReal delta,PetscReal eta)
{
  PetscInt   i,k,maxpos;
  PetscReal  max;
  PetscTruth found;
  
  PetscFunctionBegin;  
  /* initialize which */
  found = PETSC_FALSE;
  maxpos = 0;
  max = 0.0;
  for (i=0;i<j;i++) {
    if (PetscAbsReal(mu[i]) >= delta) {
      which[i] = PETSC_TRUE;
      found = PETSC_TRUE;
    } else which[i] = PETSC_FALSE;
    if (PetscAbsReal(mu[i]) > max) {
      maxpos = i;
      max = PetscAbsReal(mu[i]);
    }
  }
  if (!found) which[maxpos] = PETSC_TRUE;    
  
  for (i=0;i<j;i++)
    if (which[i]) {
      /* find left interval */
      for (k=i;k>=0;k--) {
        if (PetscAbsReal(mu[k])<eta || which[k]) break;
	else which[k] = PETSC_TRUE;
      }
      /* find right interval */
      for (k=i+1;k<j;k++) {
        if (PetscAbsReal(mu[k])<eta || which[k]) break;
	else which[k] = PETSC_TRUE;
      }
    }
  PetscFunctionReturnVoid();
}

#undef __FUNCT__  
#define __FUNCT__ "EPSPartialLanczos"
/*
   EPSPartialLanczos - Partial reorthogonalization.
*/
static PetscErrorCode EPSPartialLanczos(EPS eps,PetscReal *alpha,PetscReal *beta,Vec *V,PetscInt k,PetscInt *M,Vec f,PetscTruth *breakdown,PetscReal anorm)
{
  EPS_LANCZOS *lanczos = (EPS_LANCZOS *)eps->data;
  PetscErrorCode ierr;
  Mat            A;
  PetscInt       i,j,m = *M;
  PetscInt       n;
  PetscReal      norm,*omega,lomega[100],*omega_old,lomega_old[100],eps1,delta,eta;
  PetscTruth     *which,lwhich[100],*which2,lwhich2[100],
                 reorth = PETSC_FALSE,force_reorth = PETSC_FALSE,fro = PETSC_FALSE,estimate_anorm = PETSC_FALSE;
  PetscScalar    *swork,*hwork,lhwork[100];

  PetscFunctionBegin;
  if (m>100) {
    ierr = PetscMalloc(m*sizeof(PetscReal),&omega);CHKERRQ(ierr);
    ierr = PetscMalloc(m*sizeof(PetscReal),&omega_old);CHKERRQ(ierr);
  } else {
    omega = lomega;
    omega_old = lomega_old;
  }
  if (eps->nds+m > 100) {
    ierr = PetscMalloc(sizeof(PetscTruth)*(eps->nds+m),&which);CHKERRQ(ierr);
    ierr = PetscMalloc(sizeof(PetscTruth)*(eps->nds+m),&which2);CHKERRQ(ierr);
    ierr = PetscMalloc((eps->nds+m)*sizeof(PetscScalar),&swork);CHKERRQ(ierr);
    ierr = PetscMalloc((eps->nds+m)*sizeof(PetscScalar),&hwork);CHKERRQ(ierr);
  } else {
    which = lwhich;
    which2 = lwhich2;
    swork = PETSC_NULL;
    hwork = lhwork;
  }

  ierr = STGetOperators(eps->OP,&A,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatGetSize(A,&n,PETSC_NULL);CHKERRQ(ierr);
  eps1 = sqrt((PetscReal)n)*PETSC_MACHINE_EPSILON/2;
  delta = PETSC_SQRT_MACHINE_EPSILON/sqrt((PetscReal)eps->ncv);
  eta = pow(PETSC_MACHINE_EPSILON,3.0/4.0)/sqrt((PetscReal)eps->ncv);
  if (anorm < 0.0) {
    anorm = 1.0;
    estimate_anorm = PETSC_TRUE;
  }
  for (i=0;i<m-k;i++) 
    omega[i] = omega_old[i] = 0.0;
  for (i=0;i<eps->nds+k;i++)
    which[i] = PETSC_TRUE;  
  
  for (j=k;j<m;j++) {
    ierr = STApply(eps->OP,V[j],f);CHKERRQ(ierr);
    if (fro) {
      /* Lanczos step with full reorthogonalization */
      ierr = IPOrthogonalize(eps->ip,eps->nds+j+1,PETSC_NULL,eps->DSV,f,hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);      
      alpha[j-k] = PetscRealPart(hwork[eps->nds+j]);
    } else {
      /* Lanczos step */
      which[eps->nds+j] = PETSC_TRUE;
      if (j-2>=k) which[eps->nds+j-2] = PETSC_FALSE;
      ierr = IPOrthogonalize(eps->ip,eps->nds+j+1,which,eps->DSV,f,hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);
      alpha[j-k] = PetscRealPart(hwork[eps->nds+j]);
      beta[j-k] = norm;
      
      /* Estimate ||A|| if needed */ 
      if (estimate_anorm) {
        if (j>k) anorm = PetscMax(anorm,PetscAbsReal(alpha[j-k])+norm+beta[j-k-1]);
	else anorm = PetscMax(anorm,PetscAbsReal(alpha[j-k])+norm);
      }

      /* Check if reorthogonalization is needed */
      reorth = PETSC_FALSE;
      if (j>k) {      
	update_omega(omega,omega_old,j-k,alpha,beta-1,eps1,anorm);
	for (i=0;i<j-k;i++)
	  if (PetscAbsScalar(omega[i]) > delta) reorth = PETSC_TRUE;
      }

      if (reorth || force_reorth) {
	if (lanczos->reorthog == EPSLANCZOS_REORTHOG_PERIODIC) {
	  /* Periodic reorthogonalization */
	  if (force_reorth) force_reorth = PETSC_FALSE;
	  else force_reorth = PETSC_TRUE;
	  ierr = IPOrthogonalize(eps->ip,j-k,PETSC_NULL,V+k,f,hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);
	  for (i=0;i<j-k;i++)
            omega[i] = eps1;
	} else {
	  /* Partial reorthogonalization */
	  if (force_reorth) force_reorth = PETSC_FALSE;
	  else {
	    force_reorth = PETSC_TRUE;
	    compute_int(which2,omega,j-k,delta,eta);
	    for (i=0;i<j-k;i++) 
	      if (which2[i]) omega[i] = eps1;
	  }
	  ierr = IPOrthogonalize(eps->ip,j-k,which2,V+k,f,hwork,&norm,breakdown,eps->work[0],swork);CHKERRQ(ierr);	
	}	
      }
    }
    
    if (*breakdown || norm < n*anorm*PETSC_MACHINE_EPSILON) {
      *M = j+1;
      break;
    }
    if (!fro && norm*delta < anorm*eps1) {
      fro = PETSC_TRUE;
      PetscInfo1(eps,"Switching to full reorthogonalization at iteration %i\n",eps->its);	
    }
    
    beta[j-k] = norm;
    if (j<m-1) {
      ierr = VecScale(f,1.0/norm);CHKERRQ(ierr);
      ierr = VecCopy(f,V[j+1]);CHKERRQ(ierr);
    }
  }

  if (m>100) {
    ierr = PetscFree(omega);CHKERRQ(ierr);
    ierr = PetscFree(omega_old);CHKERRQ(ierr);
  }
  if (eps->nds+m > 100) {
    ierr = PetscFree(which);CHKERRQ(ierr);
    ierr = PetscFree(which2);CHKERRQ(ierr);
    ierr = PetscFree(swork);CHKERRQ(ierr);
    ierr = PetscFree(hwork);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSBasicLanczos"
/*
   EPSBasicLanczos - Computes an m-step Lanczos factorization. The first k
   columns are assumed to be locked and therefore they are not modified. On
   exit, the following relation is satisfied:

                    OP * V - V * T = f * e_m^T

   where the columns of V are the Lanczos vectors, T is a tridiagonal matrix, 
   f is the residual vector and e_m is the m-th vector of the canonical basis. 
   The Lanczos vectors (together with vector f) are B-orthogonal (to working
   accuracy) if full reorthogonalization is being used, otherwise they are
   (B-)semi-orthogonal. On exit, beta contains the B-norm of f and the next 
   Lanczos vector can be computed as v_{m+1} = f / beta. 

   This function simply calls another function which depends on the selected
   reorthogonalization strategy.
*/
static PetscErrorCode EPSBasicLanczos(EPS eps,PetscReal *alpha,PetscReal *beta,Vec *V,PetscInt k,PetscInt *m,Vec f,PetscTruth *breakdown,PetscReal anorm)
{
  EPS_LANCZOS *lanczos = (EPS_LANCZOS *)eps->data;
  PetscErrorCode ierr;
  IPOrthogonalizationRefinementType orthog_ref;
  PetscScalar *T;
  PetscInt i,n=*m;
  PetscReal betam;

  PetscFunctionBegin;
  switch (lanczos->reorthog) {
    case EPSLANCZOS_REORTHOG_LOCAL:
      ierr = EPSLocalLanczos(eps,alpha,beta,V,k,m,f,breakdown);CHKERRQ(ierr);
      break;
    case EPSLANCZOS_REORTHOG_SELECTIVE:
      ierr = EPSSelectiveLanczos(eps,alpha,beta,V,k,m,f,breakdown,anorm);CHKERRQ(ierr);
      break;
    case EPSLANCZOS_REORTHOG_FULL:
      ierr = EPSFullLanczos(eps,alpha,beta,V,k,m,f,breakdown);CHKERRQ(ierr);
      break;
    case EPSLANCZOS_REORTHOG_PARTIAL:
    case EPSLANCZOS_REORTHOG_PERIODIC:
      ierr = EPSPartialLanczos(eps,alpha,beta,V,k,m,f,breakdown,anorm);CHKERRQ(ierr);
      break;
    case EPSLANCZOS_REORTHOG_DELAYED:
      ierr = PetscMalloc(n*n*sizeof(PetscScalar),&T);CHKERRQ(ierr);
      ierr = IPGetOrthogonalization(eps->ip,PETSC_NULL,&orthog_ref,PETSC_NULL);CHKERRQ(ierr);
      if (orthog_ref == IP_ORTH_REFINE_NEVER) {
        ierr = EPSDelayedArnoldi1(eps,T,n,V,k,m,f,&betam,breakdown);CHKERRQ(ierr);       
      } else {
        ierr = EPSDelayedArnoldi(eps,T,n,V,k,m,f,&betam,breakdown);CHKERRQ(ierr);
      }
      for (i=k;i<n-1;i++) { alpha[i-k] = PetscRealPart(T[n*i+i]); beta[i-k] = PetscRealPart(T[n*i+i+1]); }
      alpha[n-1] = PetscRealPart(T[n*(n-1)+n-1]);
      beta[n-1] = betam;
      ierr = PetscFree(T);CHKERRQ(ierr);
      break;
    default:
      SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Invalid reorthogonalization type"); 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSSolve_LANCZOS"
PetscErrorCode EPSSolve_LANCZOS(EPS eps)
{
  EPS_LANCZOS *lanczos = (EPS_LANCZOS *)eps->data;
  PetscErrorCode ierr;
  PetscInt       nconv,i,j,k,l,x,n,m,*perm,restart,ncv=eps->ncv;
  Vec            w=eps->work[0],f=eps->work[1];
  PetscScalar    *Y,stmp;
  PetscReal      *d,*e,*ritz,*bnd,anorm,beta,norm,rtmp;
  PetscTruth     breakdown;
  char           *conv,ctmp;

  PetscFunctionBegin;
  ierr = PetscMalloc(ncv*sizeof(PetscReal),&d);CHKERRQ(ierr);
  ierr = PetscMalloc(ncv*sizeof(PetscReal),&e);CHKERRQ(ierr);
  ierr = PetscMalloc(ncv*sizeof(PetscReal),&ritz);CHKERRQ(ierr);
  ierr = PetscMalloc(ncv*ncv*sizeof(PetscScalar),&Y);CHKERRQ(ierr);
  ierr = PetscMalloc(ncv*sizeof(PetscReal),&bnd);CHKERRQ(ierr);
  ierr = PetscMalloc(ncv*sizeof(PetscInt),&perm);CHKERRQ(ierr);
  ierr = PetscMalloc(ncv*sizeof(char),&conv);CHKERRQ(ierr);

  /* The first Lanczos vector is the normalized initial vector */
  ierr = EPSGetStartVector(eps,0,eps->V[0],PETSC_NULL);CHKERRQ(ierr);
  
  anorm = -1.0;
  nconv = 0;
  
  /* Restart loop */
  while (eps->reason == EPS_CONVERGED_ITERATING) {
    eps->its++;
    /* Compute an ncv-step Lanczos factorization */
    m = PetscMin(nconv+eps->mpd,ncv);
    ierr = EPSBasicLanczos(eps,d,e,eps->V,nconv,&m,f,&breakdown,anorm);CHKERRQ(ierr);

    /* Compute eigenvalues and eigenvectors Y of the tridiagonal block */
    n = m - nconv;
    beta = e[n-1];
    ierr = EPSDenseTridiagonal(n,d,e,ritz,Y);CHKERRQ(ierr);
    
    /* Estimate ||A|| */
    for (i=0;i<n;i++) 
      if (PetscAbsReal(ritz[i]) > anorm) anorm = PetscAbsReal(ritz[i]);
    
    /* Compute residual norm estimates as beta*abs(Y(m,:)) + eps*||A|| */
    for (i=0;i<n;i++) {
      bnd[i] = beta*PetscAbsScalar(Y[i*n+n-1]) + PETSC_MACHINE_EPSILON*anorm;
      bnd[i] = bnd[i] / PetscAbsScalar(ritz[i]);
      if (bnd[i] < eps->tol) {
        conv[i] = 'C';
      } else {
        conv[i] = 'N';
      }
    }

    /* purge repeated ritz values */
    if (lanczos->reorthog == EPSLANCZOS_REORTHOG_LOCAL)
      for (i=1;i<n;i++)
        if (conv[i] == 'C')
          if (PetscAbsScalar((ritz[i]-ritz[i-1])/ritz[i]) < eps->tol)
            conv[i] = 'R';

    /* Compute restart vector */
    if (breakdown) {
      PetscInfo2(eps,"Breakdown in Lanczos method (it=%i norm=%g)\n",eps->its,beta);
    } else {
      restart = 0;
      while (restart<n && conv[restart] != 'N') restart++;
      if (restart >= n) {
        breakdown = PETSC_TRUE;
      } else {
        switch (eps->which) {
        case EPS_LARGEST_MAGNITUDE:
        case EPS_SMALLEST_MAGNITUDE:
          rtmp = PetscAbsReal(ritz[restart]);
          break;
        case EPS_LARGEST_REAL:      
        case EPS_SMALLEST_REAL:
          rtmp = ritz[restart];
          break;
        default: SETERRQ(1,"Wrong value of which");
        }
        for (i=restart+1;i<n;i++)
          if (conv[i] == 'N') {
            switch (eps->which) {
            case EPS_LARGEST_MAGNITUDE:
              if (rtmp < PetscAbsReal(ritz[i])) { rtmp = PetscAbsReal(ritz[i]); restart = i; }
              break;
            case EPS_SMALLEST_MAGNITUDE:
              if (rtmp > PetscAbsReal(ritz[i])) { rtmp = PetscAbsReal(ritz[i]); restart = i; }
              break;
            case EPS_LARGEST_REAL:      
              if (rtmp < ritz[i]) { rtmp = ritz[i]; restart = i; }
              break;
            case EPS_SMALLEST_REAL:
              if (rtmp > ritz[i]) { rtmp = ritz[i]; restart = i; }
              break;
            default: SETERRQ(1,"Wrong value of which");
            }
          }
        ierr = VecSet(f,0.0);CHKERRQ(ierr);
        ierr = VecMAXPY(f,n,Y+restart*n,eps->V+nconv);CHKERRQ(ierr);
      }
    }

    /* Count and put converged eigenvalues first */
    for (i=0;i<n;i++) perm[i] = i;
    for (k=0;k<n;k++)
      if (conv[perm[k]] != 'C') {
        j = k + 1;
        while (j<n && conv[perm[j]] != 'C') j++;
        if (j>=n) break;
        l = perm[k]; perm[k] = perm[j]; perm[j] = l;
      }

    /* Sort eigenvectors according to permutation */
    for (i=0;i<k;i++) {
      x = perm[i];
      if (x != i) {
        j = i + 1;
        while (perm[j] != i) j++;
        /* swap eigenvalues i and j */
        rtmp = ritz[x]; ritz[x] = ritz[i]; ritz[i] = rtmp;
        rtmp = bnd[x]; bnd[x] = bnd[i]; bnd[i] = rtmp;
        ctmp = conv[x]; conv[x] = conv[i]; conv[i] = ctmp;
        perm[j] = x; perm[i] = i;
        /* swap eigenvectors i and j */
        for (l=0;l<n;l++) {
          stmp = Y[x*n+l]; Y[x*n+l] = Y[i*n+l]; Y[i*n+l] = stmp;
        }
      }
    }
    
    /* compute converged eigenvectors */
    ierr = SlepcUpdateVectors(n,eps->V+nconv,0,k,Y,n,PETSC_FALSE);CHKERRQ(ierr);
    
    /* purge spurious ritz values */
    if (lanczos->reorthog == EPSLANCZOS_REORTHOG_LOCAL) {
      for (i=0;i<k;i++) {
	ierr = VecNorm(eps->V[nconv+i],NORM_2,&norm);CHKERRQ(ierr);
        ierr = VecScale(eps->V[nconv+i],1.0/norm);CHKERRQ(ierr);
        ierr = STApply(eps->OP,eps->V[nconv+i],w);CHKERRQ(ierr);
	ierr = VecAXPY(w,-ritz[i],eps->V[nconv+i]);CHKERRQ(ierr);
	ierr = VecNorm(w,NORM_2,&norm);CHKERRQ(ierr);
	bnd[i] = norm / PetscAbsScalar(ritz[i]);
        if (bnd[i] >= eps->tol) conv[i] = 'S';
      }
      for (i=0;i<k;i++)
        if (conv[i] != 'C') {
          j = i + 1;
          while (j<k && conv[j] != 'C') j++;
          if (j>=k) break;
          /* swap eigenvalues i and j */
          rtmp = ritz[j]; ritz[j] = ritz[i]; ritz[i] = rtmp;
          rtmp = bnd[j]; bnd[j] = bnd[i]; bnd[i] = rtmp;
          ctmp = conv[j]; conv[j] = conv[i]; conv[i] = ctmp;
          /* swap eigenvectors i and j */
          ierr = VecSwap(eps->V[nconv+i],eps->V[nconv+j]);CHKERRQ(ierr);
        }
      k = i;
    }
    
    /* store ritz values and estimated errors */
    for (i=0;i<n;i++) {
      eps->eigr[nconv+i] = ritz[i];
      eps->errest[nconv+i] = bnd[i];
    }
    EPSMonitor(eps,eps->its,nconv,eps->eigr,eps->eigi,eps->errest,nconv+n);
    nconv = nconv+k;
    if (eps->its >= eps->max_it) eps->reason = EPS_DIVERGED_ITS;
    if (nconv >= eps->nev) eps->reason = EPS_CONVERGED_TOL;
    
     if (eps->reason == EPS_CONVERGED_ITERATING) { /* copy restart vector */
      if (lanczos->reorthog == EPSLANCZOS_REORTHOG_LOCAL && !breakdown) {
        /* Reorthonormalize restart vector */
	ierr = IPOrthogonalize(eps->ip,eps->nds+nconv,PETSC_NULL,eps->DSV,f,PETSC_NULL,&norm,&breakdown,w,PETSC_NULL);CHKERRQ(ierr);
	ierr = VecScale(f,1.0/norm);CHKERRQ(ierr);
      }
      if (breakdown) {
	/* Use random vector for restarting */
	PetscInfo(eps,"Using random vector for restart\n");
	ierr = EPSGetStartVector(eps,nconv,f,&breakdown);CHKERRQ(ierr);
      }
      if (breakdown) { /* give up */
        eps->reason = EPS_DIVERGED_BREAKDOWN;
        PetscInfo(eps,"Unable to generate more start vectors\n");
      } else {
        ierr = VecCopy(f,eps->V[nconv]);CHKERRQ(ierr);
      }
    }    
  }
  
  eps->nconv = nconv;

  ierr = PetscFree(d);CHKERRQ(ierr);
  ierr = PetscFree(e);CHKERRQ(ierr);
  ierr = PetscFree(ritz);CHKERRQ(ierr);
  ierr = PetscFree(Y);CHKERRQ(ierr);
  ierr = PetscFree(bnd);CHKERRQ(ierr);
  ierr = PetscFree(perm);CHKERRQ(ierr);
  ierr = PetscFree(conv);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static const char *lanczoslist[6] = { "local", "full", "selective", "periodic", "partial" , "delayed" };

#undef __FUNCT__  
#define __FUNCT__ "EPSSetFromOptions_LANCZOS"
PetscErrorCode EPSSetFromOptions_LANCZOS(EPS eps)
{
  PetscErrorCode ierr;
  EPS_LANCZOS    *lanczos = (EPS_LANCZOS *)eps->data;
  PetscTruth     flg;
  PetscInt       i;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("LANCZOS options");CHKERRQ(ierr);
  ierr = PetscOptionsEList("-eps_lanczos_reorthog","Lanczos reorthogonalization","EPSLanczosSetReorthog",lanczoslist,6,lanczoslist[lanczos->reorthog],&i,&flg);CHKERRQ(ierr);
  if (flg) lanczos->reorthog = (EPSLanczosReorthogType)i;
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "EPSLanczosSetReorthog_LANCZOS"
PetscErrorCode EPSLanczosSetReorthog_LANCZOS(EPS eps,EPSLanczosReorthogType reorthog)
{
  EPS_LANCZOS *lanczos = (EPS_LANCZOS *)eps->data;

  PetscFunctionBegin;
  switch (reorthog) {
    case EPSLANCZOS_REORTHOG_LOCAL:
    case EPSLANCZOS_REORTHOG_FULL:
    case EPSLANCZOS_REORTHOG_DELAYED:
    case EPSLANCZOS_REORTHOG_SELECTIVE:
    case EPSLANCZOS_REORTHOG_PERIODIC:
    case EPSLANCZOS_REORTHOG_PARTIAL:
      lanczos->reorthog = reorthog;
      break;
    default:
      SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Invalid reorthogonalization type");
  }
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "EPSLanczosSetReorthog"
/*@
   EPSLanczosSetReorthog - Sets the type of reorthogonalization used during the Lanczos
   iteration. 

   Collective on EPS

   Input Parameters:
+  eps - the eigenproblem solver context
-  reorthog - the type of reorthogonalization

   Options Database Key:
.  -eps_lanczos_reorthog - Sets the reorthogonalization type (either 'local', 'selective',
                         'periodic', 'partial', 'full' or 'delayed')
   
   Level: advanced

.seealso: EPSLanczosGetReorthog(), EPSLanczosReorthogType
@*/
PetscErrorCode EPSLanczosSetReorthog(EPS eps,EPSLanczosReorthogType reorthog)
{
  PetscErrorCode ierr, (*f)(EPS,EPSLanczosReorthogType);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)eps,"EPSLanczosSetReorthog_C",(void (**)())&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(eps,reorthog);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "EPSLanczosGetReorthog_LANCZOS"
PetscErrorCode EPSLanczosGetReorthog_LANCZOS(EPS eps,EPSLanczosReorthogType *reorthog)
{
  EPS_LANCZOS *lanczos = (EPS_LANCZOS *)eps->data;
  PetscFunctionBegin;
  *reorthog = lanczos->reorthog;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "EPSLanczosGetReorthog"
/*@C
   EPSLanczosGetReorthog - Gets the type of reorthogonalization used during the Lanczos
   iteration. 

   Collective on EPS

   Input Parameter:
.  eps - the eigenproblem solver context

   Input Parameter:
.  reorthog - the type of reorthogonalization

   Level: advanced

.seealso: EPSLanczosSetReorthog(), EPSLanczosReorthogType
@*/
PetscErrorCode EPSLanczosGetReorthog(EPS eps,EPSLanczosReorthogType *reorthog)
{
  PetscErrorCode ierr, (*f)(EPS,EPSLanczosReorthogType*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)eps,"EPSLanczosGetReorthog_C",(void (**)())&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(eps,reorthog);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSView_LANCZOS"
PetscErrorCode EPSView_LANCZOS(EPS eps,PetscViewer viewer)
{
  PetscErrorCode ierr;
  EPS_LANCZOS    *lanczos = (EPS_LANCZOS *)eps->data;
  PetscTruth     isascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (!isascii) {
    SETERRQ1(1,"Viewer type %s not supported for EPSLANCZOS",((PetscObject)viewer)->type_name);
  }  
  ierr = PetscViewerASCIIPrintf(viewer,"reorthogonalization: %s\n",lanczoslist[lanczos->reorthog]);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
EXTERN PetscErrorCode EPSSolve_TS_LANCZOS(EPS);
*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "EPSCreate_LANCZOS"
PetscErrorCode EPSCreate_LANCZOS(EPS eps)
{
  PetscErrorCode ierr;
  EPS_LANCZOS    *lanczos;

  PetscFunctionBegin;
  ierr = PetscNew(EPS_LANCZOS,&lanczos);CHKERRQ(ierr);
  PetscLogObjectMemory(eps,sizeof(EPS_LANCZOS));
  eps->data                      = (void *) lanczos;
  eps->ops->solve                = EPSSolve_LANCZOS;
/*  eps->ops->solvets              = EPSSolve_TS_LANCZOS;*/
  eps->ops->setup                = EPSSetUp_LANCZOS;
  eps->ops->setfromoptions       = EPSSetFromOptions_LANCZOS;
  eps->ops->destroy              = EPSDestroy_Default;
  eps->ops->view                 = EPSView_LANCZOS;
  eps->ops->backtransform        = EPSBackTransform_Default;
  /*if (eps->solverclass==EPS_TWO_SIDE)
       eps->ops->computevectors       = EPSComputeVectors_Schur;
  else*/ eps->ops->computevectors       = EPSComputeVectors_Hermitian;
  lanczos->reorthog              = EPSLANCZOS_REORTHOG_LOCAL;
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)eps,"EPSLanczosSetReorthog_C","EPSLanczosSetReorthog_LANCZOS",EPSLanczosSetReorthog_LANCZOS);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)eps,"EPSLanczosGetReorthog_C","EPSLanczosGetReorthog_LANCZOS",EPSLanczosGetReorthog_LANCZOS);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

