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
    
typedef std::map<int, ArgParserOptionsEntry>  ArgOptionMap; 
typedef std::vector<std::string>  ArgNonOptionVector; 

   
// the comdline args parser class
// Because most stream switch's application, (source or receiver application) 
// needs the command line arguments for initialization, and most of them are the same
// for these application.  The comdline args parser class is designed to 
// provide a common commandline arguments parsing functions for these application.
// This arg Parser make use of getopt_long() API internal, so that the user can follow
// the general argument format when execute the applications of stream switch
//
// Thread safety: 
//    This class are not thread-safe, User cannot invoke its methods
// on the same instance in different thread 
//    The static function ParseArgs() is global thread unsafe, that means User
// should call the functions in the same thread even for dirrent parser instance

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
    //   url               u                   the url which source would connect to  
    //
    // Subclass can override this method to add its customer options.
    virtual int Init();
    
    // 
    // clear this parser to the uninit state
    virtual void Uninit();
    

    //flag check methods
    virtual bool IsInit();

    
    
    // Parse()
    // parse the given args 
    // The default implementation of this is using getopt_long, 
    // so that it's not thread-safe, even on different parser instance.
    // user can override this method to provide his implemenation
    // params:
    //      argc int in: argc which come from main()'s argument
    //      argv char ** in: argv which come from main()'s argument
    virtual int Parse(int argc, char ** argv);
    
    const ArgNonOptionVector & non_options();
  
    
    //access methods
    uint32_t has_bits();
    void set_has_bits(uint32_t has_bits);
    
    bool has_stream_name();  
    std::string stream_name();
    
    bool has_port();
    int port();
    
    bool has_host();
    std::string host();    
    
    bool has_debug_log();
    std::string debug_log();        

    bool has_url();
    std::string url();    
    
protected:
    
    virtual void SetInit(bool is_init);   

    // AddOption
    // register argument options for parsing
    virtual int AddOption(const char *opt_name, 
                          char opt_short, int has_arg, 
                          const char * help_info, 
                          void * user_data); 
                               
    const ArgOptionMap & options();   
    
    virtual void AppendNonOption(std::string value);     
   

    //
    //subclass can overrid the following method to provide customer option parse.
    virtual bool ParseOption(const std::string &opt_name, 
                          const char * opt_value, void * user_data);

    virtual bool ParseNonOption(const char * value);
    
    virtual bool ParseUnknown(const char * unknown_arg);    
    

                          
    int getNextOptionKey();

    
private:

    
// arg parser flags
#define ARG_PARSER_FLAG_INIT 1

    uint32_t flags_;      
    
    ArgOptionMap options_;
    ArgNonOptionVector non_options_;
   
   
#define ARG_PARSER_HAS_STREAM_NAME 0x00000001u
#define ARG_PARSER_HAS_PORT 0x00000002u
#define ARG_PARSER_HAS_HOST 0x00000004u
#define ARG_PARSER_HAS_DEBUG_LOG 0x00000008u
#define ARG_PARSER_HAS_URL 0x00000010u
    uint32_t has_bits_;
    
    
    std::string stream_name_;
    int port_;
    std::string host_;   
    std::string debug_log_;  
    std::string url_;
    
    
    
    int next_option_key_; 

};




bool ArgParser::IsInit()
{
    return (flags_ & ARG_PARSER_FLAG_INIT) != 0;     
}
void ArgParser::SetInit(bool is_init)
{
    if(is_init){
        flags_ |= ARG_PARSER_FLAG_INIT;  
    }else{
        flags_ &= ~(ARG_PARSER_FLAG_INIT); 
    }
}  


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

int ArgParser::port(){
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


bool ArgParser::has_url(){
    return (has_bits_ & ARG_PARSER_HAS_URL) != 0;
}

std::string ArgParser::url(){
    return url_;
}


const ArgOptionMap & ArgParser::options()
{
    return options_;
}

const ArgNonOptionVector & ArgParser::non_options()
{
    return non_options_;
}


int ArgParser::getNextOptionKey()
{
    return next_option_key_++;
}

void ArgParser::AppendNonOption(std::string value)
{
    non_options_.push_back(value);
}  


}

#endif