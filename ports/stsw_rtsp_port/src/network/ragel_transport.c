
/**
 * This file is part of stsw_rtsp_port, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2015  OpenSight team (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 **/
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "rtsp.h"
#include "rtp.h"
#include "ragel_parsers.h"



char* strDupSize(char const* str) {
  if (str == NULL) return NULL;
  size_t len = strlen(str) + 1;
  char* copy = malloc(len);

  return copy;
}


gboolean ragel_parse_transport_header(RTSP_Client *rtsp,
                                      RTP_transport *rtp_t,
                                      const char *header) {


    struct ParsedTransport transport;
    unsigned short p1, p2;
    unsigned rtpCid, rtcpCid;
    char const* fields = header;
    char* field = strDupSize(fields);

    // Initialize the result parameters to default values:
    memset(&transport, 0, sizeof(struct ParsedTransport));
    transport.protocol = TransportUDP;
    transport.mode = TransportUnicast;





    // Then, run through each of the fields, looking for ones we handle:
    while (sscanf(fields, "%[^;]", field) == 1) {
        if (strcmp(field, "RTP/AVP/TCP") == 0) {
            transport.protocol = TransportTCP;
        } else if (strcmp(field, "RAW/RAW/UDP") == 0 ) {
            transport.protocol = TransportUDP;
        } else if (strncasecmp(field, "destination=", 12) == 0) {
            strncpy(rtp_t->destination, field+12, 63);

        } else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
            transport.parameters.UDP.Unicast.port_rtp = p1;
            transport.parameters.UDP.Unicast.port_rtcp = p2;

        } else if (sscanf(field, "client_port=%hu", &p1) == 1) {
            transport.parameters.UDP.Unicast.port_rtp = p1;
            transport.parameters.UDP.Unicast.port_rtcp = p1 + 1;
        } else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
            transport.parameters.TCP.ich_rtp = (uint16_t)rtpCid;
            transport.parameters.TCP.ich_rtcp = (uint16_t)rtcpCid;
        }

        fields += strlen(field);
        while (*fields == ';') ++fields; // skip over separating ';' chars
        if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
    }
    free(field);

    /* check param */
    if( (transport.protocol ==  TransportUDP &&
         transport.parameters.UDP.Unicast.port_rtcp == 0 &&
         transport.parameters.UDP.Unicast.port_rtp == 0) ||
        (transport.protocol ==  TransportTCP &&
         transport.parameters.TCP.ich_rtcp == 0 &&
         transport.parameters.TCP.ich_rtp == 0)) {
        return false;
    }


    if ( !check_parsed_transport(rtsp, rtp_t, &transport) )
        return false;

    return true;

}

