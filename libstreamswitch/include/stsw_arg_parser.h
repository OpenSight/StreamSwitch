/**
 *
 * This file is part of libstreamswtich, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *  *
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
    
typedef std::map<int, ArgParserOptionsEntry>  OptionRegMap; 

typedef std::vector<std::string>  ArgNonOptionVector; 
typedef std::map<std::string, std::string> OptionMap;
   
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

    // RegisterBaseOptions()
    // Register the following basic options for all stream switch application
    //    Name            Short                Meaning
    //   help              h                   print help info
    //   version           v                   print version 
    virtual void RegisterBasicOptions();
    
    // RegisterSourceOptions()
    // Register the following options for source application
    //    Name            Short                Meaning
    //   stream-name       s                   stream name
    //   port              p                   the stream API tcp port 
    //   log-file          l                   log file path for debug
    //   log-size          L                   log file max size in bytes
    //   log-rotate        r                   log rotate number, 0 means no rotating
    //   log-level                             log level
    //   url               u                   the url which source would connect to  
    //   queue-size        q                   the size of the message queue for Publish, 0 means no limit
    virtual void RegisterSourceOptions();    
    

    // RegisterSenderOptions()
    // Register the following options for sender application
    //    Name            Short                Meaning
    //   stream-name       s                   local stream name
    //   host              H                   host IP address of the remote stream
    //   port              p                   tcp port of the remote stream 
    //   log-file          l                   log file path for debug
    //   log-size          L                   log file max size in bytes
    //   log-rotate        r                   log rotate number, 0 means no rotating
    //   log-level                             log level
    //   url               u                   the url of the dest file which the sender send to
    //   format            f                   the dest file format name
    //   queue-size        q                   the size of the message queue for Subscriber    
    virtual void RegisterSenderOptions();    

    // clear this parser
    virtual void Clear();
    


    virtual int RegisterOption(const char *opt_name, 
                               char opt_short, int flags, 
                               const char *value_name, 
                               const char * help_info, 
                               OptionHandler user_parse_handler, 
                               void * user_data);  
    virtual void UnregisterOption(const char *opt_name);  
    virtual void UnregisterAllOption();
                               

    virtual std::string GetOptionsHelp();   
    
    // Parse()
    // parse the given args 
    // The default implementation of this is using getopt_long, 
    // so that it's not thread-safe, even on different parser instance.
    // user can override this method to provide his implemenation
    // params:
    //      argc int in: argc which come from main()'s argument
    //      argv char ** in: argv which come from main()'s argument
    virtual int Parse(int argc, char ** argv, std::string *err_info);
    
    
    virtual inline const ArgNonOptionVector & non_options();

    virtual inline const OptionMap & options();    
    
    //
    //check the given option exist or not
    virtual bool CheckOption(std::string opt_name);

    
    //
    // Get the option value for the specific option name
    // If the option doese not exist, return the default value
    virtual std::string OptionValue(std::string opt_name, 
                                    std::string default_value);
    
protected:               
 
    
    virtual inline void AppendNonOption(std::string value);     
   
    //
    //subclass can overrid the following method to provide customer option parse.
    virtual bool ParseOption(const std::string &opt_name, 
                             const char * opt_value, 
                             OptionHandler user_parse_handler, 
                             void * user_data);

    virtual bool ParseNonOption(const char * value);
    
    virtual bool ParseUnknown(const char * unknown_arg);        
        
    virtual inline int getNextOptionKey();
    
    
private:

    OptionRegMap  option_reg_map_; 
   
    OptionMap options_;
    ArgNonOptionVector non_options_;
   
   
    int next_option_key_; 

};




const OptionMap & ArgParser::options()
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