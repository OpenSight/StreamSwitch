/**
 * This file is part of stsw_proxy_source, which belongs to StreamSwitch
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
 * url.h
 *      the header file of UrlParser class. 
 * 
 * author: jamken
 * date: 2015-6-23
**/ 


#include <string.h>
#include <stdlib.h>
#include <string>


class UrlParser{
  
public:
    UrlParser():port_(0){}

    inline int Parse(const char * urlname){
        
        const char * protocol_start, * hostname_start, * port_start, * path_start;
        size_t protocol_len, hostname_len, port_len, path_len;



        hostname_start = strstr(urlname, (const char*)"://");
        if (hostname_start == NULL) {
            hostname_start = urlname;
            protocol_start = NULL;
            protocol_len = 0;
        } else {
            protocol_len = (size_t)(hostname_start - urlname);
            hostname_start = hostname_start + 3;
            protocol_start = urlname;
        }

        hostname_len = strlen(urlname) - ((size_t)(hostname_start - urlname));

        path_start = strstr(hostname_start, (const char*)"/");
        if (path_start == NULL) {
            path_len = 0;
        } else {
            ++path_start;
            hostname_len = (size_t)(path_start - hostname_start - 1);
            path_len = strlen(urlname) - ((size_t)(path_start - urlname));
        }

        port_start = strstr(hostname_start, ":");
        if ((port_start == NULL) ||
            ((port_start >= path_start) && (path_start != NULL))) {
            port_len = 0;
            port_start = NULL;
        } else {
            ++port_start;
            if (path_len)
                port_len = (size_t)(path_start - port_start - 1);
            else
                port_len = strlen(urlname) - ((size_t)(port_start - urlname));
            hostname_len = (size_t)(port_start - hostname_start - 1);
        }

        if (protocol_len) {
            protocol_.assign(protocol_start, protocol_len);
        }

        if (port_len) {
            char* tmp = new char[port_len+1];
            strncpy(tmp, port_start, port_len);
            tmp[port_len] = '\0';
            port_ = strtol(tmp, NULL, 0);
            delete[] tmp;
        }

        if (path_len) {
            path_.assign(path_start, path_len);
        }
        if(hostname_len){
            host_name_.assign(hostname_start, hostname_len);
        }

        return 0;
    }
        


 
    std::string protocol_;
    std::string host_name_;
    int port_;
    std::string path_;
     

};
