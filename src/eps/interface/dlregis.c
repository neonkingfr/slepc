
#include "slepceps.h"

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscDLLibraryRegister"
/*
  PetscDLLibraryRegister - This function is called when the dynamic library 
  it is in is opened.

  This one registers all the EPS and ST methods in the libslepc.a
  library.

  Input Parameter:
  path - library path
 */
int PetscDLLibraryRegister(char *path)
{
  int ierr;

  ierr = PetscInitializeNoArguments(); if (ierr) return 1;

  PetscFunctionBegin;
  /*
      If we got here then PETSc was properly loaded
  */
  ierr = EPSRegisterAll(path); CHKERRQ(ierr);
  ierr = STRegisterAll(path); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* --------------------------------------------------------------------------*/
static char *contents = "Iterative methods for large sparse eigenvalue problems.\n";

static char *authors = SLEPC_AUTHOR_INFO;
static char *version = SLEPC_VERSION_NUMBER;

/* --------------------------------------------------------------------------*/
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscDLLibraryInfo"
int PetscDLLibraryInfo(char *path,char *type,char **mess) 
{
  PetscTruth iscon,isaut,isver;
  int        ierr;

  PetscFunctionBegin; 

  ierr = PetscStrcmp(type,"Contents",&iscon);CHKERRQ(ierr);
  ierr = PetscStrcmp(type,"Authors",&isaut);CHKERRQ(ierr);
  ierr = PetscStrcmp(type,"Version",&isver);CHKERRQ(ierr);
  if (iscon)      *mess = contents;
  else if (isaut) *mess = authors;
  else if (isver) *mess = version;
  else            *mess = 0;

  PetscFunctionReturn(0);
}
EXTERN_C_END

