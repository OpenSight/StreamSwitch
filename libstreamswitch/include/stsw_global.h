/**
 * This file is part of libstreamswtich, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/**
 * stsw_global.h
 *      global functions header file, declare all global functions for stream switch.
 * 
 * author: OpenSight Team
 * date: 2015-1-3
**/ 

#ifndef STSW_GLOBAL_H
#define STSW_GLOBAL_H



#include <stsw_defs.h>


namespace stream_switch {


// GlobalInit()
// init stream switch core lib, and install the default SIGINT/SIGTERM signal
// handler which simple set global interrupt flag to true.
// User should invoke this function before any methods of source/sink
// class. 
// The stream switch core lib cannot work very well for fork(). If 
// user invokes GlobalInit() but not yet init any source nor sink in parent 
// before fork() ,it would be OK if user invoke GlobalUninit() in the child 
// process after fork(). Otherwise, some errors would occurs. It's not 
// recommand using fork() after GlobalInit() in multi-process situation.  
// sink class in parent process
// 
// Note: ArgParser & RotateLogger class don't require invoking GlobalInit()
// 
// Params: 
//        default_signal_handler - whether install the default SIGINT/SIGTERM 
//                                 signal handler or not 
int GlobalInit(bool default_signal_handler = true);

// GlobalUninit()
// shutdown stream switch core lib, and reset SIGINT/SIGTERM signal handler
// to the original before GlobalInit().
// User should invoke this fucntion after using source/sink
// class
void GlobalUninit();

// check global interrupt flag, 
// which is used to determind should exit the program or not
bool isGlobalInterrupt();

// set the global interrupt flag.
void SetGolbalInterrupt(bool isInterrupt);


// SetIntSigHandler()
// Install the customer SIGINT/SIGTERM signal handler, other than the default one.
// If user want to do something when receiving SIGINT/SIGTERM signal, 
// he should invoke this SetIntSigHandler() to install his signal handler to
// override the default one.
void SetIntSigHandler(SignalHandler handler);



}

#endif