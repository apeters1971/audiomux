//
//  audiobuffer.cpp
//  
//
//  Created by Andreas Peters on 03.04.20.
//

#include "audiobuffer.hpp"



int
audiocodec::configure(int samplingrate, int channels, int bitrate)
{
    int err=0;
    decoder = opus_decoder_create(samplingrate, channels, &err);
    
    if(err!=OPUS_OK) {
        fprintf(stderr,"error: failed to create decoder\n");
        return err;
    }
    
    encoder = opus_encoder_create(samplingrate, channels, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &err);
    
    if(err!=OPUS_OK) {
        fprintf(stderr,"error: failed to create encoder\n");
        return err;
    }
    
    if(opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
        fprintf(stderr,"error: failed to set encoder bit rate\n");
    }
    
    return 0;
}



int
audiobuffer::wav2mpg(){
    
    return 0;
}

int
audiobuffer::mpg2wav()
{
    return 0;
}

int
audiobuffer::wav2udp()
{
    return 0;
}

int
audiobuffer::upd2wav()
{
    return 0;
}
