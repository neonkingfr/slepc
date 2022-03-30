#
#  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  SLEPc - Scalable Library for Eigenvalue Problem Computations
#  Copyright (c) 2002-, Universitat Politecnica de Valencia, Spain
#
#  This file is part of SLEPc.
#  SLEPc is distributed under a 2-clause BSD license (see LICENSE).
#  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#

from __future__ import print_function
import os, sys

class Log:

  def __init__(self):
    self.lastfailed = False

  def Open(self,slepcdir,confdir,fname):
    filename = os.path.join(confdir,fname)
    self.fd = open(filename,'w')
    try:
      self.filename = os.path.relpath(filename,slepcdir)  # needs python-2.6
    except AttributeError:
      self.filename = filename
    try: # symbolic link to log file in current directory
      if os.path.isfile(fname) or os.path.islink(fname): os.remove(fname)
      os.symlink(self.filename,fname)
    except: pass

  def Println(self,string):
    print(string)
    if hasattr(self,'fd'):
      self.fd.write(string+'\n')

  def Print(self,string):
    print(string, end=' ')
    if hasattr(self,'fd'):
      self.fd.write(string+' ')

  def NewSection(self,string):
    if self.lastfailed:
      colorfail = '\033[91m'
      colornorm = '\033[0m'
      print(colorfail+'failed'+colornorm+'\n'+string, end=' ')
    else:
      print('done\n'+string, end=' ')
    sys.stdout.flush()
    self.fd.write('='*80+'\n'+string+'\n')
    self.lastfailed = False

  def write(self,string):
    self.fd.write(string+'\n')

  def Warn(self,string):
    msg = '\nxxx'+'='*74+'xxx\nWARNING: '+string+'\nxxx'+'='*74+'xxx'
    print(msg)
    if hasattr(self,'fd'):
      self.fd.write(msg+'\n')

  def Exit(self,string):
    msg = '\nERROR: '+string
    print(msg)
    if hasattr(self,'fd'):
      self.fd.write(msg+'\n')
      self.fd.close()
      msg = 'ERROR: See "' + self.filename + '" file for details'
    else:
      msg = 'ERROR during configure (log file not open yet)'
    sys.exit(msg)

  def setLastFailed(self):
    self.lastfailed = True

