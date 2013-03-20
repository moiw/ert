#  Copyright (C) 2012  Statoil ASA, Norway. 
#   
#  The file 'enkf_state.py' is part of ERT - Ensemble based Reservoir Tool. 
#   
#  ERT is free software: you can redistribute it and/or modify 
#  it under the terms of the GNU General Public License as published by 
#  the Free Software Foundation, either version 3 of the License, or 
#  (at your option) any later version. 
#   
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or 
#  FITNESS FOR A PARTICULAR PURPOSE.   
#   
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
#  for more details. 

import  ctypes
from    ert.cwrap.cwrap       import *
from    ert.cwrap.cclass      import CClass
from    ert.util.tvector      import * 
from    enkf_enum             import *
import  libenkf
class EnKFState(CClass):
    
    def __init__(self , c_ptr = None):
        self.owner = False
        self.c_ptr = c_ptr
        
        
    def __del__(self):
        if self.owner:
            cfunc.free( self )


##################################################################

cwrapper = CWrapper( libenkf.lib )
cwrapper.registerType( "enkf_state" , EnKFState )

cfunc = CWrapperNameSpace("enkf_state")

##################################################################
##################################################################
cfunc.free                = cwrapper.prototype("void enkf_state_free( enkf_state )")
cfunc.kill_simulation     = cwrapper.prototype("void enkf_state_kill_simulation(enkf_state)")
cfunc.resubmit_simulation = cwrapper.prototype("void enkf_state_resubmit_simulation(enkf_state, int)")
cfunc.get_run_status      = cwrapper.prototype("int enkf_state_get_run_status(enkf_state)")
cfunc.get_start_time      = cwrapper.prototype("time_t enkf_state_get_start_time(enkf_state)")
cfunc.get_submit_time     = cwrapper.prototype("time_t enkf_state_get_submit_time(enkf_state)")