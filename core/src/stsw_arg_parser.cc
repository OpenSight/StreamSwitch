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
 * stsw_rotate_logger.cc
 *      RotateLogger class implementation file, define all methods of RotateLogger.
 * 
 * author: jamken
 * date: 2014-12-8
**/ 

#include <stsw_arg_parser.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

namespace stream_switch {


ArgParser::ArgParser()
:flags_(0), has_bits_(0), port_(0), next_option_key_(1024)
{
    
}



ArgParser::~ArgParser()
{
    
}


int ArgParser::Init()
{
    //register the default options
    AddOption("stream-name", 's', 1, "stream name", NULL);
    AddOption("port", 'p', 1, "the stream API tcp port", NULL);
    AddOption("host", 'h', 1, "the remote host IP address", NULL);
    AddOption("debug-log", 'l', 1, "log file path for debug", NULL);    
    
    SetInit(true);    
}
    
    
void ArgParser::Uninit()
{
    options_.clear();
    non_options_.clear();
    has_bits_ = 0;
    stream_name_.clear();
    port_ = 0;
    host_.clear();
    debug_log_.clear();
    
    next_option_key_ = 1024;
    
    SetInit(false);    
}

 


int ArgParser::AddOption(const char *opt_name, 
                          char opt_short, int has_arg, 
                         const char *help_info, 
                          void * user_data)
{
    if(opt_short > 127){
        return ERROR_CODE_PARAM;
    }else if(opt_short == '?'){
        return ERROR_CODE_PARAM;        
    }
    
    ArgParserOptionsEntry entry;
    entry.has_arg = has_arg;
    entry.opt_name = opt_name;
    entry.help_info = help_info;
    entry.user_data = user_data;
    if(opt_short){
        entry.opt_key = opt_short;
    }else{
        entry.opt_key = getNextOptionKey();
    }
    
    options_[entry.opt_key] = entry;
    
    return 0;
                              
}  


int ArgParser::Parse(int argc, char ** argv)
{
    std::string optstring; 

    const ArgOptionMap & optionMap = options();
    struct option * longopts = (struct option *)calloc((optionMap.size() + 1), sizeof(struct option));
    char ** local_argv = (char **)calloc(argc, sizeof(char *));
    memcpy(local_argv, argv, sizeof(char *) * argc);

    
    ArgOptionMap::const_iterator it;
 
    int i = 0;
    for(it = optionMap.begin(), i = 0; it != optionMap.end(); it++, i++){
        longopts[i].has_arg = it->second.has_arg;
        longopts[i].name = it->second.opt_name.c_str();
        longopts[i].flag = NULL;
        longopts[i].val = it->second.opt_key;
        if(it->second.opt_key < 128 && it->second.opt_key > 0){
            optstring.push_back((char)it->second.opt_key);
            if(it->second.has_arg == 1){
                optstring.push_back(':');
            }else if(it->second.has_arg == 2){
                optstring.push_back(':');
                optstring.push_back(':');                
            }
        }
    }
    
    //parse loop
    int opt;
    int org_opterr = opterr;
    int org_optind = optind;
    opterr = 0;
    optind = 1;    
    
    //parse options
    
    while ((opt = getopt_long(argc, local_argv, optstring.c_str(), longopts, NULL)) != -1) {
        if(opt == '?'){
            //not regonize this option,
            ParseUnknown(local_argv[optind - 1]);
        }else{

            it = optionMap.find(opt);
            if(it != optionMap.end()){
                char *opt_value = NULL;
                //find a option
                if(it->second.has_arg != 0){
                    //this option may has an argument
                    opt_value = optarg;
                }
                ParseOption(it->second.opt_name, opt_value, it->second.user_data);
            }
        }
        
    }
    
    // parse non-option
    for(;optind < argc; optind ++){
        ParseNonOption(local_argv[optind]);
    }
    
    
    opterr = org_opterr;
    optind = org_optind;
    
    
    free(longopts);   
    free(local_argv);
    
    return 0;
  
}



bool ArgParser::ParseOption(const std::string &opt_name, 
                         const char * opt_value, void * user_data)
{
    bool ret = false;
    uint32_t bits =  has_bits();
    
    if(opt_name == "stream-name"){
        bits |= ARG_PARSER_HAS_STREAM_NAME;
        ret = true;
        stream_name_ = opt_value;
        
    }else if(opt_name == "port"){
        bits |= ARG_PARSER_HAS_PORT;
        ret = true;
        port_ = atoi(opt_value);
        
    }else if(opt_name == "host"){
        bits |= ARG_PARSER_HAS_HOST;
        ret = true;
        host_ = opt_value;
        
    }else if(opt_name == "debug-log"){
        bits |= ARG_PARSER_HAS_DEBUG_LOG;
        ret = true; 
        debug_log_ = opt_value;
    }
    
    
    set_has_bits(bits);
    
    return ret;
}

bool ArgParser::ParseNonOption(const char * value)
{
    AppendNonOption(std::string(value));
    return true;
}

bool ArgParser::ParseUnknown(const char * unknown_arg)
{
    //just ignore, user can override this function to print error message and exit
    return false;
}

 
}





