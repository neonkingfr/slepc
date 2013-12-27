/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2013, Universitat Politecnica de Valencia, Spain

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

#include <petsc-private/fortranimpl.h>
#include <slepc-private/slepcimpl.h>
#include <slepc-private/pepimpl.h>

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define pepdestroy_                 PEPDESTROY
#define pepview_                    PEPVIEW
#define pepsetoptionsprefix_        PEPSETOPTIONSPREFIX
#define pepappendoptionsprefix_     PEPAPPENDOPTIONSPREFIX
#define pepgetoptionsprefix_        PEPGETOPTIONSPREFIX
#define pepcreate_                  PEPCREATE
#define pepsettype_                 PEPSETTYPE
#define pepgettype_                 PEPGETTYPE
#define pepmonitorall_              PEPMONITORALL
#define pepmonitorlg_               PEPMONITORLG
#define pepmonitorlgall_            PEPMONITORLGALL
#define pepmonitorset_              PEPMONITORSET
#define pepmonitorconverged_        PEPMONITORCONVERGED
#define pepmonitorfirst_            PEPMONITORFIRST
#define pepgetip_                   PEPGETIP
#define pepgetds_                   PEPGETDS
#define pepgetwhicheigenpairs_      PEPGETWHICHEIGENPAIRS
#define pepgetproblemtype_          PEPGETPROBLEMTYPE
#define pepgetconvergedreason_      PEPGETCONVERGEDREASON
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define pepdestroy_                 pepdestroy
#define pepview_                    pepview
#define pepsetoptionsprefix_        pepsetoptionsprefix
#define pepappendoptionsprefix_     pepappendoptionsprefix
#define pepgetoptionsprefix_        pepgetoptionsprefix
#define pepcreate_                  pepcreate
#define pepsettype_                 pepsettype
#define pepgettype_                 pepgettype
#define pepmonitorall_              pepmonitorall
#define pepmonitorlg_               pepmonitorlg
#define pepmonitorlgall_            pepmonitorlgall
#define pepmonitorset_              pepmonitorset
#define pepmonitorconverged_        pepmonitorconverged
#define pepmonitorfirst_            pepmonitorfirst
#define pepgetip_                   pepgetip
#define pepgetds_                   pepgetds
#define pepgetwhicheigenpairs_      pepgetwhicheigenpairs
#define pepgetproblemtype_          pepgetproblemtype
#define pepgetconvergedreason_      pepgetconvergedreason
#endif

/*
   These are not usually called from Fortran but allow Fortran users
   to transparently set these monitors from .F code, hence no STDCALL
*/
PETSC_EXTERN void pepmonitorall_(PEP *pep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = PEPMonitorAll(*pep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void pepmonitorlg_(PEP *pep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = PEPMonitorLG(*pep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void pepmonitorlgall_(PEP *pep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = PEPMonitorLGAll(*pep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void pepmonitorconverged_(PEP *pep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = PEPMonitorConverged(*pep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void pepmonitorfirst_(PEP *pep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = PEPMonitorFirst(*pep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

static struct {
  PetscFortranCallbackId monitor;
  PetscFortranCallbackId monitordestroy;
} _cb;

/* These are not extern C because they are passed into non-extern C user level functions */
#undef __FUNCT__
#define __FUNCT__ "ourmonitor"
static PetscErrorCode ourmonitor(PEP pep,PetscInt i,PetscInt nc,PetscScalar *er,PetscScalar *ei,PetscReal *d,PetscInt l,void* ctx)
{
  PetscObjectUseFortranCallback(pep,_cb.monitor,(PEP*,PetscInt*,PetscInt*,PetscScalar*,PetscScalar*,PetscReal*,PetscInt*,void*,PetscErrorCode*),(&pep,&i,&nc,er,ei,d,&l,_ctx,&ierr));
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "ourdestroy"
static PetscErrorCode ourdestroy(void** ctx)
{
  PEP pep = (PEP)*ctx;
  PetscObjectUseFortranCallback(pep,_cb.monitordestroy,(void*,PetscErrorCode*),(_ctx,&ierr));
  return 0;
}

PETSC_EXTERN void PETSC_STDCALL pepdestroy_(PEP *pep,PetscErrorCode *ierr)
{
  *ierr = PEPDestroy(pep);
}

PETSC_EXTERN void PETSC_STDCALL pepview_(PEP *pep,PetscViewer *viewer,PetscErrorCode *ierr)
{
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = PEPView(*pep,v);
}

PETSC_EXTERN void PETSC_STDCALL pepsettype_(PEP *pep,CHAR type PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(type,len,t);
  *ierr = PEPSetType(*pep,t);
  FREECHAR(type,t);
}

PETSC_EXTERN void PETSC_STDCALL pepgettype_(PEP *pep,CHAR name PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  PEPType tname;

  *ierr = PEPGetType(*pep,&tname);if (*ierr) return;
  *ierr = PetscStrncpy(name,tname,len);
  FIXRETURNCHAR(PETSC_TRUE,name,len);
}

PETSC_EXTERN void PETSC_STDCALL pepsetoptionsprefix_(PEP *pep,CHAR prefix PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(prefix,len,t);
  *ierr = PEPSetOptionsPrefix(*pep,t);
  FREECHAR(prefix,t);
}

PETSC_EXTERN void PETSC_STDCALL pepappendoptionsprefix_(PEP *pep,CHAR prefix PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(prefix,len,t);
  *ierr = PEPAppendOptionsPrefix(*pep,t);
  FREECHAR(prefix,t);
}

PETSC_EXTERN void PETSC_STDCALL pepcreate_(MPI_Fint *comm,PEP *pep,PetscErrorCode *ierr)
{
  *ierr = PEPCreate(MPI_Comm_f2c(*(comm)),pep);
}

PETSC_EXTERN void PETSC_STDCALL pepgetoptionsprefix_(PEP *pep,CHAR prefix PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  const char *tname;

  *ierr = PEPGetOptionsPrefix(*pep,&tname); if (*ierr) return;
  *ierr = PetscStrncpy(prefix,tname,len);
}

PETSC_EXTERN void PETSC_STDCALL pepmonitorset_(PEP *pep,void (PETSC_STDCALL *monitor)(PEP*,PetscInt*,PetscInt*,PetscScalar*,PetscScalar*,PetscReal*,PetscInt*,void*,PetscErrorCode*),void *mctx,void (PETSC_STDCALL *monitordestroy)(void *,PetscErrorCode*),PetscErrorCode *ierr)
{
  SlepcConvMonitor ctx;

  CHKFORTRANNULLOBJECT(mctx);
  CHKFORTRANNULLFUNCTION(monitordestroy);
  if ((PetscVoidFunction)monitor == (PetscVoidFunction)pepmonitorall_) {
    *ierr = PEPMonitorSet(*pep,PEPMonitorAll,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)pepmonitorlg_) {
    *ierr = PEPMonitorSet(*pep,PEPMonitorLG,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)pepmonitorlgall_) {
    *ierr = PEPMonitorSet(*pep,PEPMonitorLGAll,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)pepmonitorconverged_) {
    if (mctx) {
      PetscError(PetscObjectComm((PetscObject)*pep),__LINE__,"pepmonitorset_",__FILE__,PETSC_ERR_ARG_WRONG,PETSC_ERROR_INITIAL,"Must provide PETSC_NULL_OBJECT as a context in the Fortran interface to PEPMonitorSet");
      *ierr = 1;
      return;
    }
    *ierr = PetscNew(&ctx);
    if (*ierr) return;
    ctx->viewer = NULL;
    *ierr = PEPMonitorSet(*pep,PEPMonitorConverged,ctx,(PetscErrorCode (*)(void**))SlepcConvMonitorDestroy);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)pepmonitorfirst_) {
    *ierr = PEPMonitorSet(*pep,PEPMonitorFirst,0,0);
  } else {
    *ierr = PetscObjectSetFortranCallback((PetscObject)*pep,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.monitor,(PetscVoidFunction)monitor,mctx); if (*ierr) return;
    if (!monitordestroy) {
      *ierr = PEPMonitorSet(*pep,ourmonitor,*pep,0);
    } else {
      *ierr = PetscObjectSetFortranCallback((PetscObject)*pep,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.monitordestroy,(PetscVoidFunction)monitordestroy,mctx); if (*ierr) return;
      *ierr = PEPMonitorSet(*pep,ourmonitor,*pep,ourdestroy);
    }
  }
}

PETSC_EXTERN void PETSC_STDCALL pepgetip_(PEP *pep,IP *ip,PetscErrorCode *ierr)
{
  *ierr = PEPGetIP(*pep,ip);
}

PETSC_EXTERN void PETSC_STDCALL pepgetds_(PEP *pep,DS *ds,PetscErrorCode *ierr)
{
  *ierr = PEPGetDS(*pep,ds);
}

PETSC_EXTERN void PETSC_STDCALL pepgetwhicheigenpairs_(PEP *pep,PEPWhich *which,PetscErrorCode *ierr)
{
  *ierr = PEPGetWhichEigenpairs(*pep,which);
}

PETSC_EXTERN void PETSC_STDCALL pepgetproblemtype_(PEP *pep,PEPProblemType *type,PetscErrorCode *ierr)
{
  *ierr = PEPGetProblemType(*pep,type);
}

PETSC_EXTERN void PETSC_STDCALL pepgetconvergedreason_(PEP *pep,PEPConvergedReason *reason,PetscErrorCode *ierr)
{
  *ierr = PEPGetConvergedReason(*pep,reason);
}

