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
 * stsw_lock_guard.h
 *      a auto lock guard implementation
 * 
 * author: OpenSight Team
 * date: 2014-11-25
**/ 



#ifndef STSW_LOCK_GUARD_H
#define STSW_LOCK_GUARD_H
#include<pthread.h>



namespace stream_switch {

class LockGuard{
public:    
    LockGuard(pthread_mutex_t *lock): lock_(lock)
    {
        if(lock_ != NULL){
            pthread_mutex_lock(lock_); 
        }
    }
    ~LockGuard()
    {
        if(lock_ != NULL){
            pthread_mutex_unlock(lock_);
        }
        
    }
    
private:
    pthread_mutex_t *lock_;

    
};

}
#endif