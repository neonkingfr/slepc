/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

static char help[] = "Test RGIsAxisymmetric.\n\n";

#include <slepcrg.h>

int main(int argc,char **argv)
{
  PetscErrorCode ierr;
  RG             rg;
  PetscBool      vertical=PETSC_FALSE,symm;

  ierr = SlepcInitialize(&argc,&argv,(char*)0,help);if (ierr) return ierr;
  CHKERRQ(PetscOptionsGetBool(NULL,NULL,"-vertical",&vertical,NULL));

  CHKERRQ(RGCreate(PETSC_COMM_WORLD,&rg));
  CHKERRQ(RGSetFromOptions(rg));
  CHKERRQ(RGViewFromOptions(rg,NULL,"-rg_view"));
  CHKERRQ(RGIsAxisymmetric(rg,vertical,&symm));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"The region is %saxisymmetric with respect to the %s axis\n",symm?"":"NOT ",vertical?"vertical":"horizontal"));
  CHKERRQ(RGDestroy(&rg));
  ierr = SlepcFinalize();
  return ierr;
}

/*TEST

   testset:
      output_file: output/test3_hor_yes.out
      test:
         suffix: 1
      test:
         suffix: 1_ellipse
         args: -rg_type ellipse -rg_ellipse_center 2 -rg_ellipse_radius 1
      test:
         suffix: 1_interval
         args: -rg_type interval -rg_interval_endpoints -2,2,-1,1
      test:
         suffix: 1_ring
         args: -rg_type ring -rg_ring_center 2 -rg_ring_radius 1 -rg_ring_startangle 0.1 -rg_ring_endangle 0.9

   testset:
      output_file: output/test3_ver_yes.out
      args: -vertical
      test:
         suffix: 2
      test:
         suffix: 2_ellipse
         args: -rg_type ellipse -rg_ellipse_center 2i -rg_ellipse_radius 1
         requires: complex
      test:
         suffix: 2_interval
         args: -rg_type interval -rg_interval_endpoints -2,2,-1,1
      test:
         suffix: 2_ring
         args: -rg_type ring -rg_ring_center 2i -rg_ring_radius 1 -rg_ring_startangle 0.2 -rg_ring_endangle 0.3
         requires: complex

   testset:
      output_file: output/test3_hor_no.out
      test:
         suffix: 3_ellipse
         args: -rg_type ellipse -rg_ellipse_center 2i -rg_ellipse_radius 1
         requires: complex
      test:
         suffix: 3_interval
         args: -rg_type interval -rg_interval_endpoints -2,2,0,1
         requires: complex
      test:
         suffix: 3_ring
         args: -rg_type ring -rg_ring_center 2 -rg_ring_radius 1 -rg_ring_startangle 0.1 -rg_ring_endangle 0.7
         requires: complex

   testset:
      output_file: output/test3_ver_no.out
      args: -vertical
      test:
         suffix: 4_ellipse
         args: -rg_type ellipse -rg_ellipse_center 2 -rg_ellipse_radius 1
      test:
         suffix: 4_interval
         args: -rg_type interval -rg_interval_endpoints 0,2,-1,1
      test:
         suffix: 4_ring
         args: -rg_type ring -rg_ring_center 2 -rg_ring_radius 1 -rg_ring_startangle 0.1 -rg_ring_endangle 0.9

TEST*/
