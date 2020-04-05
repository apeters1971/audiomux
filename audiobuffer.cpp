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
audiobuffer::wav2mpeg(audiocodec& codec){
    fprintf(stdout,"info: capacity=%lu framesize=%lu\n", mpegbuffer.capacity(), framesize);
    
    int len = opus_encode(codec.getEncoder(),
                          (const opus_int16 *) ptr(),
                          framesize,
                          (unsigned char*) mpegptr(),
                          mpegbuffer.capacity());
    if (len < 0) {
        fprintf(stderr,"error: encoder returned %d as len\n", len);
    } else {
        fprintf(stdout,"info: encoder returned %d as len\n", len);
        type = eMPEG;
    }
    mpegbuffer.resize(len);
    return len;
}

int
audiobuffer::mpeg2wav(audiocodec& codec)
{
    fprintf(stdout,"info: capacity=%lu framesize=%lu\n", mpegbuffer.capacity(), framesize);
    
    int len = opus_decode(codec.getDecoder(),
                           (const unsigned char*) mpegptr(),
                           mpegbuffer.size(),
                           (opus_int16 *) ptr(),
                           capacity(),
                           0);
    
    if (len != framesize) {
        fprintf(stderr,"error: deocder returned %d as len\n", len);
    } else {
        type = eWAV;
    }
    return len;
}

int
audiobuffer::mpeg2udp()
{
    return 0;
}

int
audiobuffer::udp2mpeg()
{
    return 0;
}
