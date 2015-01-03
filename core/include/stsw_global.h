/**
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
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
 * author: jamken
 * date: 2015-1-3
**/ 

#ifndef STSW_GLOBAL_H
#define STSW_GLOBAL_H



#include <stsw_defs.h>


namespace stream_switch {


// GlobalInit()
// init stream switch core lib, and install the default SIGINT/SIGTERM signal
// handler which simple set global interrupt flag to true.
// User should invoke this function before any other functions of the library
int GlobalInit();

// GlobalUninit()
// shutdown stream switch core lib, and reset SIGINT/SIGTERM signal handler
// to the original before GlobalInit().
// User should invoke this fucntion after other functions of the librayry
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
void SetIntSigHandler(SignaleHandler handler);



}

#endif