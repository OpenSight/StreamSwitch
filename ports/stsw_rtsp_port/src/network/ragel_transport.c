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

