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
:next_option_key_(1024)
{

}



ArgParser::~ArgParser()
{
    
}


void ArgParser::RegisterBasicOptions()
{
    RegisterOption("help", 'h', 0, NULL, "print help info", NULL, NULL);       
    RegisterOption("version", 'v', 0, NULL, "print version", NULL, NULL);      
}

void ArgParser::RegisterSourceOptions()
{
    //register the default options
    RegisterOption("stream-name", 's', OPTION_FLAG_WITH_ARG, "STREAM",
                   "stream name", NULL, NULL);
    RegisterOption("port", 'p', OPTION_FLAG_WITH_ARG, "PORT", 
                   "the stream API tcp port", NULL, NULL);
    RegisterOption("log-file", 'l', OPTION_FLAG_WITH_ARG,  "FILE",
                   "log file path for debug", NULL, NULL);   
    RegisterOption("log-size", 'L', OPTION_FLAG_WITH_ARG,  "SIZE",
                   "log file max size in bytes", NULL, NULL);   
    RegisterOption("log-rotate", 'r', OPTION_FLAG_WITH_ARG,  "NUM",
                   "log rotate number, 0 means no rotating", NULL, NULL);       
    RegisterOption("url", 'u', OPTION_FLAG_WITH_ARG,  "URL", 
                   "the url which source would connect to", NULL, NULL);   
    
}
    
    
void ArgParser::Clear()
{
    options_.clear();
    non_options_.clear();
    option_reg_map_.clear();     
    
    next_option_key_ = 1024;
   
}

int ArgParser::RegisterOption(const char *opt_name, 
                              char opt_short, int flags, 
                              const char *value_name, 
                              const char *help_info, 
                              OptionHandler user_parse_handler, 
                              void * user_data)
{
    if(opt_short > 127){
        return ERROR_CODE_PARAM;
    }else if(opt_short == '?'){
        return ERROR_CODE_PARAM;        
    }
    if(opt_name == NULL){
        return ERROR_CODE_PARAM; 
    }
    
    
    ArgParserOptionsEntry entry;
    entry.flags = flags;
    entry.opt_name = opt_name;
    if(help_info != NULL){
        entry.help_info = help_info;        
    }
    if(value_name != NULL){
        entry.value_name = value_name;        
    }
    entry.user_data = user_data;
    entry.user_parse_handler = user_parse_handler;

    if(opt_short){
        entry.opt_key = opt_short;
    }else{
        entry.opt_key = getNextOptionKey();
    }
    
    option_reg_map_[entry.opt_key] = entry;
    
    return 0;
                              
}  

void ArgParser::UnregisterOption(const char *opt_name)
{
    OptionRegMap::iterator it;
    for(it=option_reg_map_.begin();it!=option_reg_map_.end();){
        if(it->second.opt_name == opt_name){
            option_reg_map_.erase(it++);
        }else{
            it++;
        }
    }
}


std::string ArgParser::GetOptionsHelp()
{
    std::string help_info;
    
    //
    //help format 
    //for each option, the format is follow: 
    //  -short_opt, --long_opt[=value]    info
    //                                     more info
    const OptionRegMap & optionMap = option_reg_map_;
    OptionRegMap::const_iterator it;
    for(it = optionMap.begin(); it != optionMap.end(); it++){
        std::string option_help;
        // short option
        if(it->second.opt_key < 128 && it->second.opt_key > 0){
            option_help.append("  -");
            option_help.push_back(it->second.opt_key);
            option_help.push_back(',');            
            
        }else{
            option_help.append("    ");
        }
        //long option
        option_help.append(" --");
        option_help.append(it->second.opt_name);
      
        if(it->second.flags & OPTION_FLAG_OPTIONAL_ARG){
            option_help.append("[=");
            option_help.append(it->second.value_name);
            option_help.append("]");
        }else if (it->second.flags & OPTION_FLAG_WITH_ARG){
            option_help.append("=");
            option_help.append(it->second.value_name);
        }
        
        const std::string &info = it->second.help_info;
        if(option_help.size() < 30){
            option_help.resize(30, ' ');
            
        }else{
            //new line
            option_help.push_back('\n'); 
            option_help.append(std::string(' ', 30));           
        }
        
        // option info
        int col = 30;
        int i = 0;
        while(i < (int)info.size()){
            if(col >= 79){
                option_help.push_back('\n'); 
                option_help.append(std::string(' ', 30));  
                col = 30;                     
            }
            option_help.push_back(info[i++]);
            col++;
        }
        option_help.push_back('\n'); 
        
        help_info += option_help;
        
    }   
    
    return help_info;
    
}   

int ArgParser::Parse(int argc, char ** argv)
{
    std::string optstring; 

    const OptionRegMap & optionMap = option_reg_map_;
    struct option * longopts = (struct option *)calloc((optionMap.size() + 1), sizeof(struct option));
    char ** local_argv = (char **)calloc(argc, sizeof(char *));
    memcpy(local_argv, argv, sizeof(char *) * argc);

    
    OptionRegMap::const_iterator it;
 
    int i = 0;
    for(it = optionMap.begin(), i = 0; it != optionMap.end(); it++, i++){
        if(it->second.flags & OPTION_FLAG_OPTIONAL_ARG){
            longopts[i].has_arg = 2;
        }else if (it->second.flags & OPTION_FLAG_WITH_ARG){
            longopts[i].has_arg = 1;
        }else{
            longopts[i].has_arg = 0; 
        }
        longopts[i].name = it->second.opt_name.c_str();
        longopts[i].flag = NULL;
        longopts[i].val = it->second.opt_key;
        if(it->second.opt_key < 128 && it->second.opt_key > 0){
            optstring.push_back((char)it->second.opt_key);
            if(longopts[i].has_arg == 1){
                optstring.push_back(':');
            }else if(longopts[i].has_arg == 2){
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
                if(it->second.flags & OPTION_FLAG_OPTIONAL_ARG){
                    opt_value = optarg;
                }else if (it->second.flags & OPTION_FLAG_WITH_ARG){
                    opt_value = optarg;
                }               
                ParseOption(it->second.opt_name, 
                            opt_value, 
                            it->second.user_parse_handler,
                            it->second.user_data);
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
                         const char * opt_value, 
                         OptionHandler user_parse_handler, 
                         void * user_data)
{

    std::string value;
    if(opt_value != NULL){
        value = opt_value;
    }
    options_[opt_name] = value;
    
    if(user_parse_handler != NULL){
        user_parse_handler(this, opt_name, opt_value, user_data);
    }
        
    return true;
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


bool ArgParser::CheckOption(const std::string &opt_name)
{
    OptionMap::iterator it;
    it = options_.find(opt_name);
    if(it == options_.end()){
        return false;
    }else{
        return true;
    }
}


std::string ArgParser::OptionValue(const std::string &opt_name, 
                                   const std::string &default_value)
{
    OptionMap::iterator it;
    it = options_.find(opt_name);
    if(it == options_.end()){
        return default_value;
    }else{
        return it->second;
    }      
} 


}





