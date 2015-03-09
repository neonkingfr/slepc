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

import os, commands
import petscconf, log, argdb

class Package:

  def ProcessArgs(self,argdb):
    string,found = argdb.PopPath('with-'+self.packagename+'-dir')
    if found:
      self.packagedir = string
      self.havepackage = 1
    string,found = argdb.PopString('with-'+self.packagename+'-flags')
    if found:
      self.packagelibs = string.split(',')
      self.havepackage = 1
    if argdb.PopBool('with-'+self.packagename):
      self.havepackage = 1

  def ProcessDownloadArgs(self,argdb):
    url,flag,found = argdb.PopUrl('download-'+self.packagename)
    if found:
      self.packageurl = url
      self.downloadpackage = flag

  def Process(self,conf,vars,cmake,tmpdir,archdir=''):
    name = self.packagename.upper()
    self.log.write('='*80)
    if hasattr(self,'downloadpackage'):
      if self.downloadpackage:
        self.log.Println('Installing '+name+'...')
        self.Install(conf,vars,cmake,tmpdir,archdir)
    else:
      if self.havepackage:
        self.log.Println('Checking '+name+' library...')
        self.Check(conf,vars,cmake,tmpdir)

  def ShowHelp(self):
    if hasattr(self,'downloadpackage'):
      print self.packagename.upper()+':'
      print ('  --download-'+self.packagename).ljust(35)+': Download and install '+self.packagename.upper()+' in SLEPc directory'
    else:
      print self.packagename.upper()+':'
      print ('  --with-'+self.packagename).ljust(35)+': Indicate if you wish to test for '+self.packagename.upper()
      print ('  --with-'+self.packagename+'-dir=<dir>').ljust(35)+': Indicate the directory for '+self.packagename.upper()+' libraries'
      print ('  --with-'+self.packagename+'-flags=<flags>').ljust(35)+': Indicate comma-separated flags for linking '+self.packagename.upper()

  def ShowInfo(self):
    if self.havepackage:
      self.log.Println(self.packagename.upper()+' library flags:')
      self.log.Println(' '+' '.join(self.packagelibs))

  def LinkWithOutput(self,tmpdir,functions,callbacks,flags):
    code = '#include "petscksp.h"\n'
    for f in functions:
      code += 'PETSC_EXTERN int\n' + f + '();\n'

    for c in callbacks:
      code += 'int '+ c + '() { return 0; } \n'

    code += 'int main() {\n'
    code += 'Vec v; Mat m; KSP k;\n'
    code += 'PetscInitializeNoArguments();\n'
    code += 'VecCreate(PETSC_COMM_WORLD,&v);\n'
    code += 'MatCreate(PETSC_COMM_WORLD,&m);\n'
    code += 'KSPCreate(PETSC_COMM_WORLD,&k);\n'
    for f in functions:
      code += f + '();\n'
    code += 'return 0;\n}\n'

    cfile = open(os.path.join(tmpdir,'checklink.c'),'w')
    cfile.write(code)
    cfile.close()
    (result, output) = commands.getstatusoutput('cd ' + tmpdir + ';' + petscconf.MAKE + ' checklink TESTFLAGS="'+' '.join(flags)+'"')
    try: os.remove('checklink.c')
    except OSError: pass
    if result:
      return (0,code + output)
    else:
      return (1,code + output)

  def Link(self,tmpdir,functions,callbacks,flags):
    (result, output) = self.LinkWithOutput(tmpdir,functions,callbacks,flags)
    self.log.write(output)
    return result

  def FortranLink(self,tmpdir,functions,callbacks,flags):
    output = '\n=== With linker flags: '+' '.join(flags)

    f = []
    for i in functions:
      f.append(i+'_')
    c = []
    for i in callbacks:
      c.append(i+'_')
    (result, output1) = self.LinkWithOutput(tmpdir,f,c,flags)
    output1 = '\n====== With underscore Fortran names\n' + output1
    if result: return ('UNDERSCORE',output1)

    f = []
    for i in functions:
      f.append(i.upper())
    c = []
    for i in callbacks:
      c.append(i.upper())
    (result, output2) = self.LinkWithOutput(tmpdir,f,c,flags)
    output2 = '\n====== With capital Fortran names\n' + output2
    if result: return ('CAPS',output2)

    (result, output3) = self.LinkWithOutput(tmpdir,functions,callbacks,flags)
    output3 = '\n====== With unmodified Fortran names\n' + output3
    if result: return ('STDCALL',output3)

    return ('',output + output1 + output2 + output3)

  def GenerateGuesses(self,name):
    installdirs = [os.path.join(os.path.sep,'usr','local'),os.path.join(os.path.sep,'opt')]
    if 'HOME' in os.environ:
      installdirs.insert(0,os.environ['HOME'])

    dirs = []
    for i in installdirs:
      dirs = dirs + [os.path.join(i,'lib')]
      for d in [name,name.upper(),name.lower()]:
        dirs = dirs + [os.path.join(i,d)]
        dirs = dirs + [os.path.join(i,d,'lib')]
        dirs = dirs + [os.path.join(i,'lib',d)]

    for d in dirs[:]:
      if not os.path.exists(d):
        dirs.remove(d)
    dirs = [''] + dirs
    return dirs

  def FortranLib(self,tmpdir,conf,vars,cmake,dirs,libs,functions,callbacks = []):
    name = self.packagename.upper()
    error = ''
    mangling = ''
    for d in dirs:
      for l in libs:
        if d:
          if 'rpath' in petscconf.SLFLAG:
            flags = [petscconf.SLFLAG + d] + ['-L' + d] + l
          else:
            flags = ['-L' + d] + l
        else:
          flags = l
        (mangling, output) = self.FortranLink(tmpdir,functions,callbacks,flags)
        error += output
        if mangling: break
      if mangling: break

    if mangling:
      self.log.write(output)
    else:
      self.log.write(error)
      self.log.Println('ERROR: Unable to link with library '+ name)
      self.log.Println('ERROR: In directories '+' '.join(dirs))
      self.log.Println('ERROR: With flags '+' '.join(flags))
      self.log.Exit('')

    conf.write('#ifndef SLEPC_HAVE_' + name + '\n#define SLEPC_HAVE_' + name + ' 1\n#define SLEPC_' + name + '_HAVE_'+mangling+' 1\n#endif\n\n')
    vars.write(name + '_LIB = '+' '.join(flags)+'\n')
    cmake.write('set (SLEPC_HAVE_' + name + ' YES)\n')
    libname = ' '.join([s.lstrip('-l') for s in l])
    cmake.write('set (' + name + '_LIB "")\nforeach (libname ' + libname + ')\n  string (TOUPPER ${libname} LIBNAME)\n  find_library (${LIBNAME}LIB ${libname} HINTS '+ d +')\n  list (APPEND ' + name + '_LIB "${${LIBNAME}LIB}")\nendforeach()\n')
    return flags
