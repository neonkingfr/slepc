/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#include <slepc/private/slepcimpl.h>           /*I "slepcsys.h" I*/

#if defined(SLEPC_HAVE_HPDDM)
#include <petscksp.h>
SLEPC_EXTERN PetscErrorCode KSPCreate_HPDDM(KSP);
SLEPC_EXTERN PetscErrorCode PCCreate_HPDDM(PC);
#endif

/*@C
    SlepcGetVersion - Gets the SLEPc version information in a string.

    Not collective

    Input Parameter:
.   len - length of the string

    Output Parameter:
.   version - version string

    Level: intermediate

.seealso: SlepcGetVersionNumber()
@*/
PetscErrorCode SlepcGetVersion(char version[],size_t len)
{
  PetscFunctionBegin;
#if (SLEPC_VERSION_RELEASE == 1)
  CHKERRQ(PetscSNPrintf(version,len,"SLEPc Release Version %d.%d.%d, %s",SLEPC_VERSION_MAJOR,SLEPC_VERSION_MINOR,SLEPC_VERSION_SUBMINOR,SLEPC_VERSION_DATE));
#else
  CHKERRQ(PetscSNPrintf(version,len,"SLEPc Development GIT revision: %s  GIT Date: %s",SLEPC_VERSION_GIT,SLEPC_VERSION_DATE_GIT));
#endif
  PetscFunctionReturn(0);
}

/*@C
    SlepcGetVersionNumber - Gets the SLEPc version information from the library.

    Not collective

    Output Parameters:
+   major    - the major version
.   minor    - the minor version
.   subminor - the subminor version (patch number)
-   release  - indicates the library is from a release

    Notes:
    Pass NULL in any argument that is not requested.

    The C macros SLEPC_VERSION_MAJOR, SLEPC_VERSION_MINOR, SLEPC_VERSION_SUBMINOR,
    SLEPC_VERSION_RELEASE provide the information at compile time. This can be used to confirm
    that the shared library being loaded at runtime has the appropriate version updates.

    This function can be called before SlepcInitialize().

    Level: intermediate

.seealso: SlepcGetVersion(), SlepcInitialize()
@*/
PetscErrorCode SlepcGetVersionNumber(PetscInt *major,PetscInt *minor,PetscInt *subminor,PetscInt *release)
{
  if (major)    *major    = SLEPC_VERSION_MAJOR;
  if (minor)    *minor    = SLEPC_VERSION_MINOR;
  if (subminor) *subminor = SLEPC_VERSION_SUBMINOR;
  if (release)  *release  = SLEPC_VERSION_RELEASE;
  return 0;
}

/*
   SlepcPrintVersion - Prints SLEPc version info.

   Collective
*/
static PetscErrorCode SlepcPrintVersion(MPI_Comm comm)
{
  char           version[256];

  PetscFunctionBegin;
  CHKERRQ(SlepcGetVersion(version,256));
  CHKERRQ((*PetscHelpPrintf)(comm,"%s\n",version));
  CHKERRQ((*PetscHelpPrintf)(comm,SLEPC_AUTHOR_INFO));
  CHKERRQ((*PetscHelpPrintf)(comm,"See docs/manual.html for help.\n"));
  CHKERRQ((*PetscHelpPrintf)(comm,"SLEPc libraries linked from %s\n",SLEPC_LIB_DIR));
  PetscFunctionReturn(0);
}

/*
   SlepcPrintHelpIntro - Prints introductory SLEPc help info.

   Collective
*/
static PetscErrorCode SlepcPrintHelpIntro(MPI_Comm comm)
{
  PetscFunctionBegin;
  CHKERRQ((*PetscHelpPrintf)(comm,"SLEPc help information includes that for the PETSc libraries, which provide\n"));
  CHKERRQ((*PetscHelpPrintf)(comm,"low-level system infrastructure and linear algebra tools.\n"));
  CHKERRQ((*PetscHelpPrintf)(comm,"----------------------------------------\n"));
  PetscFunctionReturn(0);
}

/* ------------------------Nasty global variables -------------------------------*/
/*
   Indicates whether SLEPc started PETSc, or whether it was
   already started before SLEPc was initialized.
*/
PetscBool SlepcBeganPetsc       = PETSC_FALSE;
PetscBool SlepcInitializeCalled = PETSC_FALSE;
PetscBool SlepcFinalizeCalled   = PETSC_FALSE;

#if defined(PETSC_HAVE_DYNAMIC_LIBRARIES) && defined(PETSC_USE_SHARED_LIBRARIES)
static PetscErrorCode SlepcLoadDynamicLibrary(const char *name,PetscBool *found)
{
  char           libs[PETSC_MAX_PATH_LEN],dlib[PETSC_MAX_PATH_LEN];

  PetscFunctionBegin;
  CHKERRQ(PetscStrcpy(libs,SLEPC_LIB_DIR));
  CHKERRQ(PetscStrlcat(libs,"/libslepc",sizeof(libs)));
  CHKERRQ(PetscStrlcat(libs,name,sizeof(libs)));
  CHKERRQ(PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,sizeof(dlib),found));
  if (*found) {
    CHKERRQ(PetscDLLibraryAppend(PETSC_COMM_WORLD,&PetscDLLibrariesLoaded,dlib));
  }
  PetscFunctionReturn(0);
}
#endif

#if defined(PETSC_USE_SINGLE_LIBRARY) && !(defined(PETSC_HAVE_DYNAMIC_LIBRARIES) && defined(PETSC_USE_SHARED_LIBRARIES))
SLEPC_EXTERN PetscErrorCode STInitializePackage(void);
SLEPC_EXTERN PetscErrorCode DSInitializePackage(void);
SLEPC_EXTERN PetscErrorCode FNInitializePackage(void);
SLEPC_EXTERN PetscErrorCode BVInitializePackage(void);
SLEPC_EXTERN PetscErrorCode RGInitializePackage(void);
SLEPC_EXTERN PetscErrorCode EPSInitializePackage(void);
SLEPC_EXTERN PetscErrorCode SVDInitializePackage(void);
SLEPC_EXTERN PetscErrorCode PEPInitializePackage(void);
SLEPC_EXTERN PetscErrorCode NEPInitializePackage(void);
SLEPC_EXTERN PetscErrorCode MFNInitializePackage(void);
SLEPC_EXTERN PetscErrorCode LMEInitializePackage(void);
#endif

/*
    SlepcInitialize_DynamicLibraries - Adds the default dynamic link libraries to the
    search path.
*/
PetscErrorCode SlepcInitialize_DynamicLibraries(void)
{
  PetscBool      preload = PETSC_FALSE;

  PetscFunctionBegin;
#if defined(PETSC_HAVE_THREADSAFETY)
  /* These must be all initialized here because it is not safe for individual threads to call these initialize routines */
  preload = PETSC_TRUE;
#endif

  CHKERRQ(PetscOptionsGetBool(NULL,NULL,"-library_preload",&preload,NULL));
  if (preload) {
#if defined(PETSC_HAVE_DYNAMIC_LIBRARIES) && defined(PETSC_USE_SHARED_LIBRARIES)
    PetscBool found;
#if defined(PETSC_USE_SINGLE_LIBRARY)
    CHKERRQ(SlepcLoadDynamicLibrary("",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc dynamic library\nYou cannot move the dynamic libraries!");
#else
    CHKERRQ(SlepcLoadDynamicLibrary("sys",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc sys dynamic library\nYou cannot move the dynamic libraries!");
    CHKERRQ(SlepcLoadDynamicLibrary("eps",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc EPS dynamic library\nYou cannot move the dynamic libraries!");
    CHKERRQ(SlepcLoadDynamicLibrary("pep",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc PEP dynamic library\nYou cannot move the dynamic libraries!");
    CHKERRQ(SlepcLoadDynamicLibrary("nep",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc NEP dynamic library\nYou cannot move the dynamic libraries!");
    CHKERRQ(SlepcLoadDynamicLibrary("svd",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc SVD dynamic library\nYou cannot move the dynamic libraries!");
    CHKERRQ(SlepcLoadDynamicLibrary("mfn",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc MFN dynamic library\nYou cannot move the dynamic libraries!");
    CHKERRQ(SlepcLoadDynamicLibrary("lme",&found));
    PetscCheck(found,PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to locate SLEPc LME dynamic library\nYou cannot move the dynamic libraries!");
#endif
#else /* defined(PETSC_HAVE_DYNAMIC_LIBRARIES) && defined(PETSC_USE_SHARED_LIBRARIES) */
#if defined(PETSC_USE_SINGLE_LIBRARY)
  CHKERRQ(STInitializePackage());
  CHKERRQ(DSInitializePackage());
  CHKERRQ(FNInitializePackage());
  CHKERRQ(BVInitializePackage());
  CHKERRQ(RGInitializePackage());
  CHKERRQ(EPSInitializePackage());
  CHKERRQ(SVDInitializePackage());
  CHKERRQ(PEPInitializePackage());
  CHKERRQ(NEPInitializePackage());
  CHKERRQ(MFNInitializePackage());
  CHKERRQ(LMEInitializePackage());
#else
  SETERRQ(PETSC_COMM_WORLD,PETSC_ERR_SUP,"Cannot use -library_preload with multiple static SLEPc libraries");
#endif
#endif /* defined(PETSC_HAVE_DYNAMIC_LIBRARIES) && defined(PETSC_USE_SHARED_LIBRARIES) */
  }

#if defined(SLEPC_HAVE_HPDDM)
  CHKERRQ(KSPRegister(KSPHPDDM,KSPCreate_HPDDM));
  CHKERRQ(PCRegister(PCHPDDM,PCCreate_HPDDM));
#endif
  PetscFunctionReturn(0);
}

PetscErrorCode SlepcCitationsInitialize()
{
  PetscFunctionBegin;
  CHKERRQ(PetscCitationsRegister("@Article{slepc-toms,\n"
    "   author = \"Vicente Hernandez and Jose E. Roman and Vicente Vidal\",\n"
    "   title = \"{SLEPc}: A Scalable and Flexible Toolkit for the Solution of Eigenvalue Problems\",\n"
    "   journal = \"{ACM} Trans. Math. Software\",\n"
    "   volume = \"31\",\n"
    "   number = \"3\",\n"
    "   pages = \"351--362\",\n"
    "   year = \"2005,\"\n"
    "   doi = \"https://doi.org/10.1145/1089014.1089019\"\n"
    "}\n",NULL));
  CHKERRQ(PetscCitationsRegister("@TechReport{slepc-manual,\n"
    "   author = \"J. E. Roman and C. Campos and L. Dalcin and E. Romero and A. Tomas\",\n"
    "   title = \"{SLEPc} Users Manual\",\n"
    "   number = \"DSIC-II/24/02 - Revision 3.16\",\n"
    "   institution = \"D. Sistemes Inform\\`atics i Computaci\\'o, Universitat Polit\\`ecnica de Val\\`encia\",\n"
    "   year = \"2021\"\n"
    "}\n",NULL));
  PetscFunctionReturn(0);
}

/*@C
   SlepcInitialize - Initializes the SLEPc library. SlepcInitialize() calls
   PetscInitialize() if that has not been called yet, so this routine should
   always be called near the beginning of your program.

   Collective on MPI_COMM_WORLD or PETSC_COMM_WORLD if it has been set

   Input Parameters:
+  argc - count of number of command line arguments
.  args - the command line arguments
.  file - [optional] PETSc database file, defaults to ~username/.petscrc
          (use NULL for default)
-  help - [optional] Help message to print, use NULL for no message

   Fortran Notes:
   Fortran syntax is very similar to that of PetscInitialize()

   Level: beginner

.seealso: SlepcFinalize(), PetscInitialize()
@*/
PetscErrorCode SlepcInitialize(int *argc,char ***args,const char file[],const char help[])
{
  PetscErrorCode ierr;
  PetscBool      flg;

  if (SlepcInitializeCalled) return 0;
  ierr = PetscSetHelpVersionFunctions(SlepcPrintHelpIntro,SlepcPrintVersion);if (ierr) return ierr;
  ierr = PetscInitialized(&flg);if (ierr) return ierr;
  if (!flg) {
    CHKERRQ(PetscInitialize(argc,args,file,help));
    SlepcBeganPetsc = PETSC_TRUE;
  }

  CHKERRQ(SlepcCitationsInitialize());

  /* Load the dynamic libraries (on machines that support them), this registers all the solvers etc. */
  CHKERRQ(SlepcInitialize_DynamicLibraries());

  SlepcInitializeCalled = PETSC_TRUE;
  SlepcFinalizeCalled   = PETSC_FALSE;
  CHKERRQ(PetscInfo(0,"SLEPc successfully started\n"));
  return 0;
}

/*@C
   SlepcFinalize - Checks for options to be called at the conclusion
   of the SLEPc program and calls PetscFinalize().

   Collective on PETSC_COMM_WORLD

   Level: beginner

.seealso: SlepcInitialize(), PetscFinalize()
@*/
PetscErrorCode SlepcFinalize(void)
{
  PetscErrorCode ierr = 0;

  PetscFunctionBegin;
  if (PetscUnlikely(!SlepcInitializeCalled)) {
    fprintf(PETSC_STDOUT,"SlepcInitialize() must be called before SlepcFinalize()\n");
    PetscStackClearTop;
    return PETSC_ERR_ARG_WRONGSTATE;
  }
  CHKERRQ(PetscInfo(NULL,"SlepcFinalize() called\n"));
  if (SlepcBeganPetsc) {
    ierr = PetscFinalize();
    SlepcBeganPetsc = PETSC_FALSE;
  }
  SlepcInitializeCalled = PETSC_FALSE;
  SlepcFinalizeCalled   = PETSC_TRUE;
  /* To match PetscFunctionBegin() at the beginning of this function */
  PetscStackClearTop;
  return ierr;
}

/*@C
   SlepcInitializeNoArguments - Calls SlepcInitialize() from C/C++ without
   the command line arguments.

   Collective

   Level: advanced

.seealso: SlepcInitialize(), SlepcInitializeFortran()
@*/
PetscErrorCode SlepcInitializeNoArguments(void)
{
  PetscErrorCode ierr;
  int            argc = 0;
  char           **args = 0;

  PetscFunctionBegin;
  ierr = SlepcInitialize(&argc,&args,NULL,NULL);
  PetscFunctionReturn(ierr);
}

/*@
   SlepcInitialized - Determine whether SLEPc is initialized.

   Output Parameter:
.  isInitialized - the result

   Level: beginner

.seealso: SlepcInitialize(), SlepcInitializeFortran()
@*/
PetscErrorCode SlepcInitialized(PetscBool *isInitialized)
{
  PetscFunctionBegin;
  *isInitialized = SlepcInitializeCalled;
  PetscFunctionReturn(0);
}

/*@
   SlepcFinalized - Determine whether SlepcFinalize() has been called.

   Output Parameter:
.  isFinalized - the result

   Level: developer

.seealso: SlepcFinalize()
@*/
PetscErrorCode SlepcFinalized(PetscBool *isFinalized)
{
  PetscFunctionBegin;
  *isFinalized = SlepcFinalizeCalled;
  PetscFunctionReturn(0);
}

PETSC_EXTERN PetscBool PetscBeganMPI;

/*@C
   SlepcInitializeNoPointers - Calls SlepcInitialize() from C/C++ without the pointers
   to argc and args (analogue to PetscInitializeNoPointers).

   Collective

   Input Parameters:
+  argc - count of number of command line arguments
.  args - the command line arguments
.  file - [optional] PETSc database file, defaults to ~username/.petscrc
          (use NULL for default)
-  help - [optional] Help message to print, use NULL for no message

   Level: advanced

.seealso: SlepcInitialize()
@*/
PetscErrorCode SlepcInitializeNoPointers(int argc,char **args,const char *file,const char *help)
{
  int            myargc = argc;
  char           **myargs = args;

  PetscFunctionBegin;
  CHKERRQ(SlepcInitialize(&myargc,&myargs,file,help));
  CHKERRQ(PetscPopSignalHandler());
  PetscBeganMPI = PETSC_FALSE;
  PetscFunctionReturn(0);
}
