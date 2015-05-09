#include "stsw_rtsp_source_app.h"


int main(int argc, char ** argv)
{
    int ret = 0;
    
    RtspSourceApp * app = NULL;
    
    app = RtspSourceApp::Instance();
    
    ret = app->Init(argc, argv);
    if(ret){
        goto err_1;
    }
    
    ret = app->DoLoop();
        
    app->Uninit();
        
err_1:    

    RtspSourceApp::Uninstance();
    
    fprintf(stderr, "stsw_rtsp_source exit with code:%d\n",ret);
    
    return ret;
}