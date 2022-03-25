/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

static char help[] = "Demonstrates SlepcGetVersionNumber().\n\n";

#include <slepcsys.h>

int main(int argc,char **argv)
{
  char           version[128];
  PetscInt       major,minor,subminor;
  PetscBool      verbose;

  CHKERRQ(SlepcInitialize(&argc,&argv,(char*)0,help));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"Checking SLEPc version.\n"));

  CHKERRQ(SlepcGetVersion(version,sizeof(version)));
  CHKERRQ(PetscOptionsHasName(NULL,NULL,"-verbose",&verbose));
  if (verbose) CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"Version information:\n%s\n",version));

  CHKERRQ(SlepcGetVersionNumber(&major,&minor,&subminor,NULL));
  PetscCheck(major==SLEPC_VERSION_MAJOR,PETSC_COMM_WORLD,PETSC_ERR_PLIB,"Library major %" PetscInt_FMT " does not equal include %d",major,SLEPC_VERSION_MAJOR);
  PetscCheck(minor==SLEPC_VERSION_MINOR,PETSC_COMM_WORLD,PETSC_ERR_PLIB,"Library minor %" PetscInt_FMT " does not equal include %d",minor,SLEPC_VERSION_MINOR);
  PetscCheck(subminor==SLEPC_VERSION_SUBMINOR,PETSC_COMM_WORLD,PETSC_ERR_PLIB,"Library subminor %" PetscInt_FMT " does not equal include %d",subminor,SLEPC_VERSION_SUBMINOR);

  CHKERRQ(SlepcFinalize());
  return 0;
}

/*TEST

   test:
      suffix: 1

TEST*/
