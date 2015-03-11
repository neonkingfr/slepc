#!/usr/bin/env python
#
#  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  SLEPc - Scalable Library for Eigenvalue Problem Computations
#  Copyright (c) 2002-2014, Universitat Politecnica de Valencia, Spain
#
#  This file is part of SLEPc.
#
#  SLEPc is free software: you can redistribute it and/or modify it under  the
#  terms of version 3 of the GNU Lesser General Public License as published by
#  the Free Software Foundation.
#
#  SLEPc  is  distributed in the hope that it will be useful, but WITHOUT  ANY
#  WARRANTY;  without even the implied warranty of MERCHANTABILITY or  FITNESS
#  FOR  A  PARTICULAR PURPOSE. See the GNU Lesser General Public  License  for
#  more details.
#
#  You  should have received a copy of the GNU Lesser General  Public  License
#  along with SLEPc. If not, see <http://www.gnu.org/licenses/>.
#  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#

import os, sys, time, shutil

def CreateDir(basedir,dirname,log):
  ''' Create directory basedir/dirname and return path string '''
  newdir = os.path.join(basedir,dirname)
  if not os.path.exists(newdir):
    try:
      os.mkdir(newdir)
    except:
      log.Exit('ERROR: Cannot create '+dirname+' directory: '+newdir)
  return newdir

def CreateDirTwo(basedir,dir1,dir2,log):
  ''' Create directory basedir/dir1/dir2 and return path string '''
  newbasedir = os.path.join(basedir,dir1)
  if not os.path.exists(newbasedir):
    try:
      os.mkdir(newbasedir)
    except:
      log.Exit('ERROR: Cannot create '+dir1+' directory: '+newbasedir)
  newdir = os.path.join(newbasedir,dir2)
  if not os.path.exists(newdir):
    try:
      os.mkdir(newdir)
    except:
      log.Exit('ERROR: Cannot create '+dirname+' directory: '+newdir)
  return newdir

def CreateDirTest(basedir,dirname,log):
  ''' Create directory, return path string and flag indicating if already existed '''
  newdir = os.path.join(basedir,dirname)
  if not os.path.exists(newdir):
    existed = False
    try:
      os.mkdir(newdir)
    except:
      log.Exit('ERROR: Cannot create '+dirname+' directory: '+newdir)
  else:
    existed = True
  return newdir, existed

# Use en_US as language so that compiler messages are in English
if 'LC_LOCAL' in os.environ and os.environ['LC_LOCAL'] != '' and os.environ['LC_LOCAL'] != 'en_US' and os.environ['LC_LOCAL']!= 'en_US.UTF-8': os.environ['LC_LOCAL'] = 'en_US.UTF-8'
if 'LANG' in os.environ and os.environ['LANG'] != '' and os.environ['LANG'] != 'en_US' and os.environ['LANG'] != 'en_US.UTF-8': os.environ['LANG'] = 'en_US.UTF-8'

# Check python version
if not hasattr(sys, 'version_info') or not sys.version_info[0] == 2 or not sys.version_info[1] >= 4:
  print '*****  You must have Python2 version 2.4 or higher to run ./configure  *****'
  print '*           Python is easy to install for end users or sys-admin.          *'
  print '*                   http://www.python.org/download/                        *'
  print '*                                                                          *'
  print '*            You CANNOT configure SLEPc without Python                     *'
  print '****************************************************************************'
  sys.exit(4)

# Set python path
configdir = os.path.abspath('config')
if not os.path.isdir(configdir):
  sys.exit('ERROR: Run configure from $SLEPC_DIR, not '+os.path.abspath('.'))
sys.path.insert(0,configdir)
sys.path.insert(0,os.path.join(configdir,'packages'))

# Load auxiliary classes
import argdb, log
argdb = argdb.ArgDB(sys.argv)
log = log.Log()

# Load classes for packages and process command-line options
import slepc, petsc, arpack, blzpack, trlan, feast, primme, blopex, sowing, lapack
slepc = slepc.SLEPc(argdb,log)
petsc = petsc.PETSc(argdb,log)
arpack = arpack.Arpack(argdb,log)
blzpack = blzpack.Blzpack(argdb,log)
trlan = trlan.Trlan(argdb,log)
primme = primme.Primme(argdb,log)
feast = feast.Feast(argdb,log)
blopex = blopex.Blopex(argdb,log)
sowing = sowing.Sowing(argdb,log)
lapack = lapack.Lapack(argdb,log)

externalpackages = [arpack, blopex, blzpack, feast, primme, trlan]
optionalpackages = [arpack, blopex, blzpack, feast, primme, trlan, sowing]
checkpackages    = [arpack, blopex, blzpack, feast, primme, trlan, lapack]

# Print help if requested and check for wrong command-line options
if argdb.PopHelp():
  print 'SLEPc Configure Help'
  print '-'*80
  print '''SLEPc:
  --with-clean=<bool>          : Delete prior build files including externalpackages
  --prefix=<dir>               : Specify location to install SLEPc (e.g., /usr/local)
  --DATAFILESPATH=<dir>        : Specify location of datafiles (for SLEPc developers)'''
  for pk in optionalpackages:
    pk.ShowHelp()
  sys.exit(0)
argdb.ErrorIfNotEmpty()

# Check if enviroment is ok
print 'Checking environment...',
if 'SLEPC_DIR' in os.environ:
  slepcdir = os.environ['SLEPC_DIR']
  if not os.path.exists(slepcdir) or not os.path.exists(os.path.join(slepcdir,'config')):
    sys.exit('ERROR: SLEPC_DIR enviroment variable is not valid')
  if os.path.realpath(os.getcwd()) != os.path.realpath(slepcdir):
    sys.exit('ERROR: SLEPC_DIR is not the current directory')
else:
  slepcdir = os.getcwd()
  if not os.path.exists(os.path.join(slepcdir,'config')):
    sys.exit('ERROR: Current directory is not valid')

if 'PETSC_DIR' in os.environ:
  petscdir = os.environ['PETSC_DIR']
  if not os.path.exists(petscdir):
    sys.exit('ERROR: PETSC_DIR enviroment variable is not valid')
else:
  if slepc.prefixdir:
    petscdir = slepc.prefixdir
    os.environ['PETSC_DIR'] = petscdir
  else:
    sys.exit('ERROR: PETSC_DIR enviroment variable is not set')

# Check PETSc version
petsc.LoadVersion(petscdir)
slepc.LoadVersion(slepcdir)
if petsc.version < slepc.version:
  sys.exit('ERROR: This SLEPc version is not compatible with PETSc version '+petsc.version)

# Check some information about PETSc configuration
import petscconf
petscconf.Load(petscdir)
if not petscconf.PRECISION in ['double','single','__float128']:
  sys.exit('ERROR: This SLEPc version does not work with '+petscconf.PRECISION+' precision')
if slepc.isinstall and not petscconf.ISINSTALL:
  sys.exit('ERROR: SLEPc cannot be configured for non-source installation if PETSc is not configured in the same way.')

# Check for empty PETSC_ARCH
emptyarch = not ('PETSC_ARCH' in os.environ and os.environ['PETSC_ARCH'])
if emptyarch:
  archname = 'installed-' + petscconf.ARCH
else:
  archname = petscconf.ARCH

# Create directories for configuration files
archdir, archdirexisted = CreateDirTest(slepcdir,archname,log)
libdir = CreateDir(archdir,'lib',log)
confdir = CreateDir(libdir,'slepc-conf',log)

# Open log file
log.Open(os.path.join(confdir,'configure.log'))
log.write('='*80)
log.write('Starting Configure Run at '+time.ctime(time.time()))
log.write('Configure Options: '+' '.join(sys.argv[1:]))
log.write('Working directory: '+os.getcwd())
log.write('Python version:\n' + sys.version)
log.write('make: ' + petscconf.MAKE)
log.write('PETSc source directory: ' + petscdir)
log.write('PETSc install directory: ' + petscconf.DESTDIR)
log.write('PETSc version: ' + petsc.lversion)
if not emptyarch:
  log.write('PETSc architecture: ' + petscconf.ARCH)
log.write('SLEPc source directory: ' + slepcdir)
log.write('SLEPc install directory: ' + slepc.prefixdir)
log.write('SLEPc version: ' + slepc.lversion)

# Clean previous configuration if needed
if archdirexisted:
  try:
    f = open(os.path.join(confdir,'slepcvariables'),"r")
    searchlines = f.readlines()
    f.close()
    found = 0
    for library in ['ARPACK','BLZPACK','TRLAN','PRIMME','FEAST','BLOPEX']:
      if library in ''.join(searchlines):
        found = 1
    if found and not any(pk.requested for pk in externalpackages):
      print 'WARNING: forcing --with-clean=1 because previous configuration had external packages'
      slepc.clean = True
  except: pass
  if slepc.clean:
    try:
      shutil.rmtree(archdir)
    except:
      log.Exit('ERROR: Cannot remove existing directory ' + archdir)
    archdir = CreateDir(slepcdir,archname,log)
    libdir  = CreateDir(archdir,'lib',log)
    confdir = CreateDir(libdir,'slepc-conf',log)

# Create other directories and configuration files
incdir = CreateDir(archdir,'include',log)
modulesdir = CreateDirTwo(confdir,'modules','slepc',log)
pkgconfigdir = CreateDir(libdir,'pkgconfig',log)
try:
  slepcvars = open(os.path.join(confdir,'slepcvariables'),'w')
  if not slepc.prefixdir:
    slepc.prefixdir = archdir
  slepcvars.write('SLEPC_DESTDIR = ' + slepc.prefixdir +'\n')
  if emptyarch:
    slepcvars.write('INSTALLED_PETSC = 1\n')
  testruns = set(petscconf.TEST_RUNS.split())
  testruns = testruns.intersection(set(['C','F90','Fortran','C_Complex','Fortran_Complex','C_NoComplex','Fortran_NoComplex']))
  if petscconf.PRECISION != '__float128':
    testruns = testruns.union(set(['C_NoF128']))
  if slepc.datadir:
    slepcvars.write('DATAFILESPATH = ' + slepc.datadir +'\n')
    testruns = testruns.union(set(['DATAFILESPATH']))
  slepcvars.write('TEST_RUNS = ' + ' '.join(testruns) +'\n')
except:
  log.Exit('ERROR: Cannot create configuration file in ' + confdir)
try:
  slepcrules = open(os.path.join(confdir,'slepcrules'),'w')
except:
  log.Exit('ERROR: Cannot create rules file in ' + confdir)
try:
  slepcconf = open(os.path.join(incdir,'slepcconf.h'),'w')
  slepcconf.write('#if !defined(__SLEPCCONF_H)\n')
  slepcconf.write('#define __SLEPCCONF_H\n\n')
  if slepc.isrepo:
    slepcconf.write('#ifndef SLEPC_VERSION_GIT\n#define SLEPC_VERSION_GIT "' + slepc.gitrev + '"\n#endif\n\n')
    slepcconf.write('#ifndef SLEPC_VERSION_DATE_GIT\n#define SLEPC_VERSION_DATE_GIT "' + slepc.gitdate + '"\n#endif\n\n')
  slepcconf.write('#ifndef SLEPC_LIB_DIR\n#define SLEPC_LIB_DIR "' + os.path.join(slepc.prefixdir,'lib') + '"\n#endif\n\n')
except:
  log.Exit('ERROR: Cannot create configuration header in ' + confdir)
try:
  cmake = open(os.path.join(confdir,'SLEPcConfig.cmake'),'w')
except:
  log.Exit('ERROR: Cannot create CMake configuration file in ' + confdir)
try:
  if archdir != slepc.prefixdir:
    modules = open(os.path.join(modulesdir,slepc.lversion),'w')
  else:
    modules = open(os.path.join(modulesdir,slepc.lversion+'-'+archname),'w')
except:
  log.Exit('ERROR: Cannot create modules file in ' + modulesdir)
try:
  pkgconfig = open(os.path.join(pkgconfigdir,'SLEPc.pc'),'w')
except:
  log.Exit('ERROR: Cannot create pkgconfig file in ' + pkgconfigdir)

# Create global configuration file for the case of empty PETSC_ARCH
if emptyarch:
  globconfdir = os.path.join(slepcdir,'lib','slepc-conf')
  try:
    globconf = open(os.path.join(globconfdir,'slepcvariables'),'w')
    globconf.write('SLEPC_DIR = ' + slepcdir +'\n')
    globconf.write('PETSC_ARCH = ' + archname + '\n')
    globconf.close()
  except:
    log.Exit('ERROR: Cannot create configuration file in ' + globconfdir)

# Check if PETSc is working
log.NewSection('Checking PETSc installation...')
if petsc.version > slepc.version:
  log.Println('WARNING: PETSc version '+petsc.version+' is newer than SLEPc version '+slepc.version)
if petsc.release != slepc.release:
  log.Exit('ERROR: Cannot mix release and development versions of SLEPc and PETSc')
if petscconf.ISINSTALL:
  if os.path.realpath(petscconf.DESTDIR) != os.path.realpath(petscdir):
    log.Println('WARNING: PETSC_DIR does not point to PETSc installation path')
petsc.Check()
if not petsc.havepackage:
  log.Exit('ERROR: Unable to link with PETSc')

# Single library installation
if petscconf.SINGLELIB:
  slepcvars.write('SHLIBS = libslepc\n')
  slepcvars.write('LIBNAME = '+os.path.join('${INSTALL_LIB_DIR}','libslepc.${AR_LIB_SUFFIX}')+'\n')
  for module in ['SYS','MFN','EPS','SVD','PEP','NEP']:
    slepcvars.write('SLEPC_'+module+'_LIB = ${CC_LINKER_SLFLAG}${SLEPC_LIB_DIR} -L${SLEPC_LIB_DIR} -lslepc ${SLEPC_EXTERNAL_LIB} ${PETSC_KSP_LIB}\n')
  slepcvars.write('SLEPC_LIB = ${CC_LINKER_SLFLAG}${SLEPC_LIB_DIR} -L${SLEPC_LIB_DIR} -lslepc ${SLEPC_EXTERNAL_LIB} ${PETSC_KSP_LIB}\n')

# Check for external packages and for missing LAPACK functions
for pk in checkpackages:
  pk.Process(slepcconf,slepcvars,cmake,archdir)

# Download sowing if requested and make Fortran stubs if necessary
bfort = petscconf.BFORT
if sowing.downloadpackage:
  bfort = sowing.Install(archdir)

if slepc.isrepo and hasattr(petscconf,'FC'):
  try:
    log.NewSection('Generating Fortran stubs...')
    if not os.path.exists(bfort):
      bfort = os.path.join(archdir,'bin','bfort')
    if not os.path.exists(bfort):
      bfort = sowing.Install(archdir)
    sys.path.insert(0, os.path.abspath(os.path.join('bin','maint')))
    import generatefortranstubs
    generatefortranstubs.main(slepcdir,bfort,os.getcwd(),0)
    generatefortranstubs.processf90interfaces(slepcdir,0)
  except AttributeError:
    log.Exit('ERROR: Cannot generate Fortran stubs; try configuring PETSc with --download-sowing or use a mercurial version of PETSc')

if bfort != petscconf.BFORT:
  slepcvars.write('BFORT = '+bfort+'\n')

# CMake stuff
log.NewSection('Creating CMake files...')
cmake.write('set (SLEPC_PACKAGE_LIBS "${ARPACK_LIB}" "${BLZPACK_LIB}" "${TRLAN_LIB}" "${PRIMME_LIB}" "${FEAST_LIB}" "${BLOPEX_LIB}" )\n')
cmake.write('set (SLEPC_PACKAGE_INCLUDES "${PRIMME_INCLUDE}")\n')
cmake.write('find_library (PETSC_LIB petsc HINTS ${PETSc_BINARY_DIR}/lib )\n')
cmake.write('''
if (NOT PETSC_LIB) # Interpret missing libpetsc to mean that PETSc was built --with-single-library=0
  set (PETSC_LIB "")
  foreach (pkg sys vec mat dm ksp snes ts tao)
    string (TOUPPER ${pkg} PKG)
    find_library (PETSC${PKG}_LIB "petsc${pkg}" HINTS ${PETSc_BINARY_DIR}/lib)
    list (APPEND PETSC_LIB "${PETSC${PKG}_LIB}")
  endforeach ()
endif ()
''')
cmake.close()
cmakeok = False
if sys.version_info >= (2,5) and not petscconf.ISINSTALL and petscconf.BUILD_USING_CMAKE:
  import cmakegen
  try:
    cmakegen.main(slepcdir,petscdir,petscdestdir=petscconf.DESTDIR)
  except (OSError), e:
    log.Exit('ERROR: Generating CMakeLists.txt failed:\n' + str(e))
  import cmakeboot
  try:
    cmakeok = cmakeboot.main(slepcdir,petscdir,log=log)
  except (OSError), e:
    log.Exit('ERROR: Booting CMake in PETSC_ARCH failed:\n' + str(e))
  except (ImportError, KeyError), e:
    log.Exit('ERROR: Importing cmakeboot failed:\n' + str(e))
  except (AttributeError), e:
    log.Println('xxx'+'='*73+'xxx')
    log.Println('WARNING: CMake builds are not available (initialization failed)')
    log.Println('You can ignore this warning (use default build), or try reconfiguring PETSc')
    log.Println('xxx'+'='*73+'xxx')
  # remove files created by PETSc's script
  for f in ['build.log','build.log.bkp','RDict.log']:
    try: os.remove(f)
    except OSError: pass
if cmakeok:
  slepcvars.write('SLEPC_BUILD_USING_CMAKE = 1\n')

# Modules file
modules.write('#%Module\n\n')
modules.write('proc ModulesHelp { } {\n')
modules.write('    puts stderr "This module sets the path and environment variables for slepc-%s"\n' % slepc.lversion)
modules.write('    puts stderr "     see http://slepc.upv.es/ for more information"\n')
modules.write('    puts stderr ""\n}\n')
modules.write('module-whatis "SLEPc - Scalable Library for Eigenvalue Problem Computations"\n\n')
modules.write('module load petsc\n')
if slepc.isinstall:
  modules.write('set slepc_dir %s\n' % slepc.prefixdir)
else:
  modules.write('set slepc_dir %s\n' % slepcdir)
modules.write('setenv SLEPC_DIR $slepc_dir\n')

# pkg-config file
pkgconfig.write('Name: SLEPc, the Scalable Library for Eigenvalue Problem Computations\n')
pkgconfig.write('Description: A parallel library to compute eigenvalues and eigenvectors of large, sparse matrices with iterative methods. It is based on PETSc.\n')
pkgconfig.write('Version: %s\n' % slepc.lversion)
pkgconfig.write('Requires: PETSc = %s\n' % petsc.lversion)
pkgconfig.write('Cflags: -I' + os.path.join(slepc.prefixdir,'include'))
if not slepc.isinstall:
  pkgconfig.write(' -I' + os.path.join(slepcdir,'include'))
pkgconfig.write('\nLibs: -L%s -lslepc\n' % os.path.join(slepc.prefixdir,'lib'))

# Finish with configuration files
slepcvars.close()
slepcrules.close()
slepcconf.write('#endif\n')
slepcconf.close()
modules.close()
pkgconfig.close()

# Print summary
log.NewSection('\n')
log.Println('='*79)
log.Println('SLEPc Configuration')
log.Println('='*79)
log.Println('')
log.Println('SLEPc directory:')
log.Println(' '+slepcdir)
if slepc.isrepo:
  log.Println('  It is a git repository on branch: '+slepc.branch)
if archdir != slepc.prefixdir:
  log.Println('SLEPc prefix directory:')
  log.Println(' '+slepc.prefixdir)
log.Println('PETSc directory:')
log.Println(' '+petscdir)
if petsc.isrepo:
  log.Println('  It is a git repository on branch: '+petsc.branch)
if petsc.isrepo and slepc.isrepo:
  if petsc.branch!='maint' and slepc.branch!='maint':
    try:
      import dateutil.parser
      import datetime
      petscdate = dateutil.parser.parse(petsc.gitdate)
      slepcdate = dateutil.parser.parse(slepc.gitdate)
      if abs(petscdate-slepcdate)>datetime.timedelta(days=30):
        log.Println('xxx'+'='*73+'xxx')
        log.Println('WARNING: your PETSc and SLEPc repos may not be in sync (more than 30 days apart)')
        log.Println('xxx'+'='*73+'xxx')
    except ImportError: pass
if emptyarch and archdir != slepc.prefixdir:
  log.Println('Prefix install with '+petscconf.PRECISION+' precision '+petscconf.SCALAR+' numbers')
else:
  log.Println('Architecture "'+archname+'" with '+petscconf.PRECISION+' precision '+petscconf.SCALAR+' numbers')
for pk in checkpackages:
  pk.ShowInfo()
log.write('\nFinishing Configure Run at '+time.ctime(time.time()))
log.write('='*79)
print
print 'xxx'+'='*73+'xxx'
if petscconf.MAKE_IS_GNUMAKE: buildtype = 'gnumake'
elif cmakeok: buildtype = 'cmake'
else: buildtype = 'legacy'
print ' Configure stage complete. Now build the SLEPc library with ('+buildtype+' build):'
if emptyarch:
  print '   make SLEPC_DIR=$PWD PETSC_DIR='+petscdir
else:
  print '   make SLEPC_DIR=$PWD PETSC_DIR='+petscdir+' PETSC_ARCH='+archname
print 'xxx'+'='*73+'xxx'
print
