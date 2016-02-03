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
 * stsw_aac_mux_parser.cc
 *      AacMuxParser class implementation file, define methods of the AacMuxParser 
 * class. 
 *      AacMuxParser is sub class of StreamMuxParser, which is reponsible to 
 * handle the aac codec
 * 
 * author: OpenSight Team
 * date: 2016-2-3
**/ 

#include "stsw_aac_mux_parser.h"

#include <stdint.h>
#include <strings.h>


#include "../stsw_log.h"

extern "C"{

 
#include <libavcodec/avcodec.h>      
}


AacMuxParser::AacMuxParser()
:StreamMuxParser()
{

}

