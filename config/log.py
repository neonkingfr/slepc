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

import os, sys

class Log:

  def Open(self,filename):
    self.fd = open(filename,'w')
    try:
      self.filename = os.path.relpath(filename)  # needs python-2.6
    except AttributeError:
      self.filename = filename

  def Println(self,string):
    print string
    self.fd.write(string+'\n')

  def Print(self,string):
    print string,
    self.fd.write(string+' ')

  def NewSection(self,string):
    print 'done\n'+string,
    self.fd.write('='*80+'\n'+string+'\n')

  def write(self,string):
    self.fd.write(string+'\n')

  def Exit(self,string):
    try:
      self.fd.write(string+'\n')
      self.fd.close()
    except AttributeError:
      pass
    print string
    if hasattr(self,'fd'):
      msg = 'ERROR: See "' + self.filename + '" file for details'
    else:
      msg = 'ERROR during configure (log file not open yet)'
    sys.exit(msg)

