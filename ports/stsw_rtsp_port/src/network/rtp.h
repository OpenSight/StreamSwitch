/* *
 * This file is part of Feng
 *
 * Copyright (C) 2009 by LScube team <team@lscube.org>
 * See AUTHORS for more details
 *
 * feng is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * feng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with feng; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * */

#ifndef FN_RTP_H
#define FN_RTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#ifdef __WIN32__
# include <winsock2.h>
#else
# include <sys/socket.h>
#endif
#include <ev.h>

#include <netembryo/wsocket.h>

#include "bufferqueue.h"

struct feng;
struct Track;
struct RTSP_Client;
struct RTSP_Range;
struct RTSP_session;

#define RTP_DEFAULT_PORT 5004
#define BUFFERED_FRAMES_DEFAULT 16
#define RTP_DEFAULT_MTU 1500


#define DEFAULT_MAX_RATE 16
#define DEFAULT_BUFFERED_MS 2000


typedef struct {
    int RTP;
    int RTCP;
} port_pair;


typedef struct client_port_pair{
    int RTP_port[2];
    pid_t pid;
    char *host;
    unsigned short port;
    struct feng *srv;
} client_port_pair;

typedef struct RTP_transport {
    Sock *rtp_sock;
    Sock *rtcp_sock;
    struct sockaddr_storage last_stg;
    int rtp_ch, rtcp_ch;
    ev_periodic rtp_writer;
    ev_io rtcp_reader;
    
    char destination[64];

} RTP_transport;

typedef struct RTP_session {

    /** Multicast session (treated in a special way) */
    gboolean multicast;

    uint16_t start_seq;
    uint16_t seq_no;

    uint32_t start_rtptime;

    uint32_t ssrc;

    uint32_t last_packet_send_time;

    time_t last_rtcp_read_time;
    uint32_t isBye;
    double bw_time_slot;
    uint32_t bw_sent_bits;


    struct RTSP_Range *range;
    double send_time;
    double last_timestamp;


    uint32_t last_rtptimestamp;   


    /** URI of the resouce for RTP-Info */
    char *uri;

    /** Pointer to the currently-selected track */
    struct Track *track;



    /**
     * @brief Pool of one thread for filling up data for the session
     *
     * This is a pool consisting of exactly one thread that is used to
     * fill up the session with data when it's running low.
     *
     * Since we do want to do this asynchronously but we don't really
     * want race conditions (and they would anyway just end up waiting
     * on the same lock), there is no need to allow multiple threads
     * to do the same thing here.
     *
     * Please note that this is created during @ref rtp_session_resume
     * rather than during @ref rtp_session_new, and deleted during
     * @ref rtp_session_pause (and eventually during @ref
     * rtp_session_free), so that we don't have reading threads to go
     * around during seeks.
     */
     /* Jamken: this thread pool is moved to rtsp session */
     //GThreadPool *fill_pool;

    /**
     * @brief Consumer for the track buffer queue
     *
     * This provides the interface between the RTP session and the
     * source of the data to send over the network.
     */
    BufferQueue_Consumer *consumer;

    struct feng *srv;
    struct RTSP_Client *client;

    uint32_t octet_count;
    uint32_t pkt_count;

    RTP_transport transport;
} RTP_session;

/**
 * @defgroup RTP RTP Layer
 * @{
 */

/**
 * RTP ports management functions
 * @defgroup rtp_port RTP ports management functions
 * @{
 */

void add_client_list(client_port_pair* client);
client_port_pair* get_client_list_item(pid_t pid);
void reduce_client_list(client_port_pair* client);
void init_client_list();
void free_client_list();


client_port_pair* new_child_port(struct feng *srv, 
                                 char * host, unsigned short port);
void free_child_port(client_port_pair *client);

extern client_port_pair *current_client;

/**
 * @}
 */

/**
 * @defgroup rtp_session RTP session management functions
 * @{
 */

RTP_session *rtp_session_new(struct RTSP_Client *, struct RTSP_session *,
                             RTP_transport *, const char *,
                             struct Track *);

void rtp_session_gslist_resume(GSList *, struct RTSP_Range *range);
void rtp_session_gslist_pause(GSList *);
void rtp_session_gslist_free(GSList *);

void rtp_session_handle_sending(RTP_session *session);



double rtp_scaler(RTP_session *session, double sourceTime);

void rtp_session_fill_cb( gpointer session_p, 
                         gpointer user_data);

/**
 * @}
 */

/**
 * @addtogroup rtcp
 * @{
 */

typedef enum {
    SR = 200,
    RR = 201,
    SDES = 202,
    BYE = 203,
    APP = 204
} rtcp_pkt_type;

gboolean rtcp_send_sr(RTP_session *session, rtcp_pkt_type type);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // FN_RTP_H
