#!/bin/bash
GETMEM=`cat /proc/sys/vm/dirty_background_ratio`
if(test $GETMEM -gt 1)
        then /sbin/sysctl vm.dirty_background_ratio=1
fi

GETMEM=`cat /proc/sys/vm/dirty_expire_centisecs`
if(test $GETMEM -gt 500)
        then /sbin/sysctl vm.dirty_expire_centisecs=500
fi

GETMEM=`cat /proc/sys/vm/dirty_writeback_centisecs`
if(test $GETMEM -gt 100)
        then /sbin/sysctl vm.dirty_writeback_centisecs=100
fi

#GETMEM=`cat /proc/sys/vm/max_writeback_pages`
#if(test $GETMEM -lt 10240)
#        then /sbin/sysctl vm.max_writeback_pages=10240
#fi

GETMINFREE=`cat /proc/sys/vm/min_free_kbytes`
if(test $GETMINFREE -lt 32768)
	then /sbin/sysctl vm.min_free_kbytes=32768
fi

