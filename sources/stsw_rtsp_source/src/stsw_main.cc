#include "stsw_rtsp_source_app.h"
#include "stream_switch.h"


static void SignalHandler (int signal_num)
{
    RtspSourceApp * app = NULL;
    
    stream_switch::SetGolbalInterrupt(true);
    
    app = RtspSourceApp::Instance();    

    app->SetWatch();
}


int main(int argc, char ** argv)
{
    int ret = 0;
    
    stream_switch::GlobalInit();
    
    RtspSourceApp * app = NULL;
    
    app = RtspSourceApp::Instance();
    
    ret = app->Init(argc, argv);
    if(ret){
        goto err_1;
    }
    
    //install signal handler
    stream_switch::SetIntSigHandler(SignalHandler); 
    
    ret = app->DoLoop();
        
    app->Uninit();
        
err_1:    

    RtspSourceApp::Uninstance();
    
    stream_switch::GlobalUninit();
    
    fprintf(stderr, "stsw_rtsp_source exit with code:%d\n",ret);
    
    return ret;
}