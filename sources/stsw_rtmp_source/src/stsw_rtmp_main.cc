/*
 * stsw_rtmp_main.cc
 *
 *  Created on: Apr 21, 2015
 *      Author: hyt
 */

#include "stsw_rtmp_source.h"


int main() {

    RtmpClientSource* rtmpClient;

    rtmpClient = new RtmpClientSource("rtmp://192.168.1.180:1935/vod/wow.flv");
    if (rtmpClient->Connect() != 0) {
        return -1;
    }


    return 0;
}

