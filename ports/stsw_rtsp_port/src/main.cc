/**
 * This file is part of stsw_rtsp_port, which belongs to StreamSwitch
 * project. And it's derived from Feng prject originally
 * 
 * Copyright (C) 2015  OpenSight team (www.opensight.cn)
 * Copyright (C) 2009 by LScube team <team@lscube.org>
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

/**
 * @file main.c
 * server main loop
 */
#include "config.h"
#include "stream_switch.h"


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <errno.h>
#ifndef __WIN32__
#include <grp.h>
#endif
#include <string.h>
#include <stdbool.h>

//#include "conf/buffer.h"
//#include "conf/array.h"
//#include "conf/configfile.h"

#include "feng.h"
#include "bufferqueue.h"
#include "fnc_log.h"
#include "incoming.h"
#include "network/rtp.h"
#include "network/rtsp.h"
#include <glib.h>
#include <netembryo/wsocket.h>



#ifdef __cplusplus
}
#endif


/**
 *  Handler to cleanly shut down feng
 */
static void sigint_cb (struct ev_loop *loop,
                        ev_signal * w,
                        int revents)
{
    ev_unloop (loop, EVUNLOOP_ALL);
}

/**
 * Drop privs to a specified user
 */
static void feng_drop_privs(feng *srv)
{
#ifndef __WIN32__    
    char *id = srv->srvconf.groupname->ptr;

    if (id) {
        struct group *gr = getgrnam(id);
        if (gr) {
            if (setgid(gr->gr_gid) < 0)
                fnc_log(FNC_LOG_WARN,
                    "Cannot setgid to user %s, %s",
                    id, strerror(errno));
        } else {
            fnc_log(FNC_LOG_WARN,
                    "Cannot get group %s id, %s",
                    id, strerror(errno));
        }
    }

    id = srv->srvconf.username->ptr;
    if (id) {
        struct passwd *pw = getpwnam(id);
        if (pw) {
            if (setuid(pw->pw_uid) < 0)
                fnc_log(FNC_LOG_WARN,
                    "Cannot setuid to user %s, %s",
                    id, strerror(errno));
        } else {
            fnc_log(FNC_LOG_WARN,
                    "Cannot get user %s id, %s",
                    id, strerror(errno));
        }
    }
#endif    
}


/**
 * catch TERM and INT signals
 * block PIPE signal
 */

ev_signal signal_watcher_int;
ev_signal signal_watcher_term;


static void feng_handle_signals(feng *srv)
{
    sigset_t block_set;
    ev_signal *sig = &signal_watcher_int;
    ev_signal_init (sig, sigint_cb, SIGINT);
    ev_signal_start (srv->loop, sig);
    sig = &signal_watcher_term;
    ev_signal_init (sig, sigint_cb, SIGTERM);
    ev_signal_start (srv->loop, sig);

    /* block PIPE signal */
#ifndef __WIN32__    
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGPIPE);
    sigprocmask(SIG_BLOCK, &block_set, NULL);
#endif    
}


void fncheader()
{
    printf("\n%s - RTSP Port %s\n Project - www.opensight.cn\n",
            PACKAGE,
            VERSION);
}


void ParseArgv(int argc, char *argv[], 
               stream_switch::ArgParser *parser);
static gboolean command_environment(feng *srv, int argc, char **argv)
{
    fnc_log_t fn = NULL;
    gchar *progname;
    std::string log_file;
    int log_size;
    int rotate_num;
    
    
    progname = g_path_get_basename(argv[0]);
    
    

    
    stream_switch::ArgParser parser;

    //parse the cmd line
    ParseArgv(argc, argv, &parser); // parse the cmd line        
    
    fncheader();
    
    srv->srvconf.port = 
        strtol(parser.OptionValue("port", "554").c_str(), NULL, 0);
    buffer_copy_string(srv->srvconf.errorlog_file, 
        parser.OptionValue("log-file", "errlog.log").c_str());
    
    srv->srvconf.log_type = FNC_LOG_OUT;
    std::string log_type = parser.OptionValue("log-type", "stderr");
    if(log_type == "stderr"){
        srv->srvconf.log_type = FNC_LOG_OUT;
    }else if(log_type == "file"){
        srv->srvconf.log_type = FNC_LOG_FILE;

        log_file = 
            parser.OptionValue("log-file", "");
        log_size = 
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        rotate_num = 
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);   
        
    }else if(log_type == "syslog"){
        srv->srvconf.log_type = FNC_LOG_SYS;
    }else{
        srv->srvconf.log_type = FNC_LOG_OUT;
    }
    srv->srvconf.loglevel = 
        strtol(parser.OptionValue("log-level", "3").c_str(), NULL, 0);
    
    
    srv->srvconf.buffered_frames = 
        strtol(parser.OptionValue("buffer-num", "2048").c_str(), NULL, 0);    
    srv->srvconf.buffered_ms = 
        strtol(parser.OptionValue("buffer-ms", "2000").c_str(), NULL, 0);
    srv->srvconf.max_conns = 
        strtol(parser.OptionValue("max-conns", "1000").c_str(), NULL, 0);

    srv->srvconf.max_rate = 
        strtol(parser.OptionValue("max-rate", "16").c_str(), NULL, 0);
    srv->srvconf.max_mbps = 
        strtol(parser.OptionValue("max-mbps", "100").c_str(), NULL, 0);
        
    srv->srvconf.rtcp_heartbeat = 0;
    if(parser.CheckOption("enable-rtcp-heartbeat")){
        srv->srvconf.rtcp_heartbeat = 1;
    }
    
    srv->srvconf.stsw_debug_flags = 
        strtol(parser.OptionValue("debug-flags", "0").c_str(), NULL, 0);
    
    srv->srvconf.first_udp_port = 0;    
    srv->srvconf.max_fds = 100;    
    srv->srvconf.bindhost->ptr = NULL;
    
    srv->config_storage.document_root = buffer_init();
    buffer_copy_string(srv->config_storage.document_root, "");

 
    
    //log 
    
    
    
    if( srv->srvconf.log_type == FNC_LOG_SYS ||
        srv->srvconf.log_type == FNC_LOG_OUT){
        fn = fnc_log_init(srv->srvconf.errorlog_file->ptr,
                          srv->srvconf.log_type,
                            srv->srvconf.loglevel,
                              progname);     

    }else{
        fn = fnc_rotate_log_init(progname, srv->srvconf.errorlog_file->ptr,
                            srv->srvconf.loglevel, log_size,
                              rotate_num);           
        
    }

    Sock_init(NULL); //use stderr as the logger for Sock library

    return true;
}

/**
 * allocates a new instance variable
 * @return the new instance or NULL on failure
 */

static feng *feng_alloc(void)
{
    struct feng *srv = g_new0(server, 1);

    if (!srv) return NULL;

#define CLEAN(x) \
    srv->srvconf.x = buffer_init();
    CLEAN(bindhost);
    CLEAN(errorlog_file);
    CLEAN(username);
    CLEAN(groupname);
#undef CLEAN

#define CLEAN(x) \
    srv->x = array_init();
    CLEAN(srvconf.modules);
#undef CLEAN
    srv->pid = getpid();
    
    bq_init();

    return srv;
}

/**
 * @brief Free the feng server object
 *
 * @param srv The object to free
 *
 * This function frees the resources connected to the server object;
 * this function is empty when debug is disabled since it's unneeded
 * for actual production use, exiting the project will free them just
 * as fine.
 *
 * What this is useful for during debug is to avoid false positives in
 * tools like valgrind that expect a complete freeing of all
 * resources.
 */
static void feng_free(feng* srv)
{
#ifndef NDEBUG
    //unsigned int i;

#define CLEAN(x) \
    buffer_free(srv->srvconf.x)
    CLEAN(bindhost);
    CLEAN(errorlog_file);
    CLEAN(username);
    CLEAN(groupname);
#undef CLEAN


    buffer_free(srv->config_storage.document_root);
    buffer_free(srv->config_storage.server_name);
    buffer_free(srv->config_storage.ssl_pemfile);
    buffer_free(srv->config_storage.ssl_ca_file);
    buffer_free(srv->config_storage.ssl_cipher_list);



#define CLEAN(x) \
    array_free(srv->x)
    CLEAN(srvconf.modules);
#undef CLEAN

    g_free(srv);

#endif /* NDEBUG */
}

int main(int argc, char **argv)
{
    feng *srv;
    int res = 0;

    if (!g_thread_supported ()) g_thread_init (NULL);



    if (! (srv = feng_alloc()) ) {
        res = 1;
        goto end_1;
    }

    /* parses the command line and initializes the log*/
    if ( !command_environment(srv, argc, argv) ) {
        res = 1;
        goto end_2;
    }
    
    init_client_list();

    //config_set_defaults(srv);

    /* This goes before feng_bind_ports */
    srv->loop = ev_default_loop(0);

    feng_handle_signals(srv);
    
    feng_start_child_watcher(srv);

    if (!feng_bind_ports(srv)) {
        res = 3;
        goto end_3;
    }

    feng_drop_privs(srv);


    fnc_log(FNC_LOG_INFO, "StreamSwitch RTSP Port Startup\n");
    
    ev_loop (srv->loop, 0);
    
    fnc_log(FNC_LOG_INFO, "StreamSwitch RTSP Port shutdonw\n");

    
    

end_4:
    
    feng_ports_cleanup(srv);
 
end_3: 
    feng_stop_child_watcher(srv);

    free_client_list();

    
    fnc_log_uninit();
    

end_2: 
    feng_free(srv);
end_1:
    return res;
}
