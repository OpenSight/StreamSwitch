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
 * stsw_arg_parser.h
 *      ArgParser class header file, declare all interfaces of ArgParser.
 * 
 * author: jamken
 * date: 2014-12-26
**/ 


#ifndef STSW_ARG_PARSER_H
#define STSW_ARG_PARSER_H
#include<map>
#include<stsw_defs.h>
#include<stdint.h>


namespace stream_switch {
    
typedef std::map<ArgParserOptionsEntry>  ArgOptionMap 
   
// the comdline args parser class
// Because most stream switch's application, (source or receiver application) 
// needs the command line arguments for initialization, and most of them are the same
// for these application.  The comdline args parser class is designed to 
// provide a common commandline arguments parsing functions for these application.
// This arg Parser make use of getopt_long() API internal, so that the user can follow
// the general argument format when execute the applications of stream switch
// Thread safety: 
//    This arg Parser make use of getopt_long() API internal, which is not 
// thread safe, so that the methods of this class are not thread-safe, 
// even for different instance
    

class ArgParser{
public:
    ArgParser();
    virtual ~ArgParser();
    
    
    // Init() 
    // Register default arguement options for stream switch application.
    // These options would be parsed to the data member of the boject after parsing
    // The default options includes: 
    //    Name            Short                Meaning
    //   stream-name       s                   stream name
    //   port              p                   the stream API tcp port 
    //   host              h                   the remote host IP address
    //   debug-log         l                   log file path for debug
    //  
    virtual int Init();
    
    
    virtual void Uninit();
    

    //flag check methods
    virtual bool IsInit()
    {
        return (flags_ & ARG_PARSER_FLAG_INIT) != 0;     
    }
    
    
    // RegisterUserArg
    // register other argument options for user customer application
    // 
    virtual int RegisterUserArg(const std::string &opt_name, 
                                char opt_short, int has_arg, 
                                const std::string help_info, 
                                ArgParseFunc parse_func, void * user_data);
                                
    virtual int Parse(int argc, char ** argv);    
    
    
    
    
    //access methods
    uint32_t has_bits();
    void set_has_bits(uint32_t has_bits);
    bool has_stream_name();  
    std::string stream_name();
    bool has_port();
    std::string port();
    
    bool has_host();
    std::string host();    
    
    bool has_debug_log();
    std::string debug_log();
    
    
    
    
    
protected:


    
private:

    
// arg parser flags
#define ARG_PARSER_FLAG_INIT 1

    uint32_t flags_;      
    


#define ARG_PARSER_HAS_STREAM_NAME 0x00000001u
#define ARG_PARSER_HAS_PORT 0x00000002u
#define ARG_PARSER_HAS_HOST 0x00000004u
#define ARG_PARSER_HAS_DEBUG_LOG 0x00000008u
    uint32_t has_bits_;
    
    
    std::string stream_name_;
    int port_;
    std::string host_;   
    std::string debug_log_;  
 

};



uint32_t ArgParser::has_bits(){
    return has_bits_;
};
void ArgParser::set_has_bits(uint32_t has_bits){
    has_bits_ = has_bits;
}

bool ArgParser::has_stream_name(){
    return (has_bits_ & ARG_PARSER_HAS_STREAM_NAME) != 0;
}

std::string ArgParser::stream_name(){
    return stream_name_;
}

bool ArgParser::has_port(){
    return (has_bits_& ARG_PARSER_HAS_PORT) != 0;
}

std::string ArgParser::port(){
    return port_;
}


bool ArgParser::has_host(){
    return (has_bits_ & ARG_PARSER_HAS_HOST) != 0;
}

std::string ArgParser::host(){
    return host_;
}



bool ArgParser::has_debug_log(){
    return (has_bits_ & ARG_PARSER_HAS_DEBUG_LOG) != 0;
}

std::string ArgParser::debug_log(){
    return debug_log_;
}



#endif