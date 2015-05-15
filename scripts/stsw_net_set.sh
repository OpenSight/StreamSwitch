#!/bin/bash
# this is important system param would affect the performance 
GETMEM=`cat /proc/sys/net/core/rmem_max`
if(test $GETMEM -lt 4194304)
	then /sbin/sysctl net.core.rmem_max=4194304
fi

GETMEM=`cat /proc/sys/net/core/wmem_max`
if(test $GETMEM -lt 8388608)
	then /sbin/sysctl net.core.wmem_max=8388608
fi

GETMEM=`cat /proc/sys/net/core/netdev_max_backlog`
if(test $GETMEM -lt 2000)
	then /sbin/sysctl net.core.netdev_max_backlog=2000
fi

GETUNIXQ=`cat /proc/sys/net/unix/max_dgram_qlen`
if(test $GETUNIXQ -lt 2000)
	then /sbin/sysctl net.unix.max_dgram_qlen=2000
fi
