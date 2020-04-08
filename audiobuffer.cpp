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
    if (debug) {
        fprintf(stdout,"info: capacity=%lu framesize=%lu\n", mpegbuffer.capacity(), framesize);
    }
    
    int len = opus_encode(codec.getEncoder(),
                          (const opus_int16 *) ptr(),
                          framesize,
                          (unsigned char*) mpegptr(),
                          mpegbuffer.capacity());
    codec.releaseCodec();
    
    if (len < 0) {
        fprintf(stderr,"error: encoder returned %d as len\n", len);
    } else {
        if (debug) {
            fprintf(stdout,"info: encoder returned %d as len\n", len);
        }
        type = eMPEG;
    }
    mpegbuffer.resize(len);
    return len;
}

int
audiobuffer::mpeg2wav(audiocodec& codec)
{
    if (1) {
        fprintf(stdout,"info: capacity=%lu framesize=%lu output=%lu size=%lu\n", mpegbuffer.capacity(), framesize, capacity(), size());
    }
    
    int len = opus_decode(codec.getDecoder(),
                          (const unsigned char*) mpegptr(),
                          mpegbuffer.size(),
                          (opus_int16 *) ptr(),
                          framesize,
                          0);
    
    codec.releaseCodec();
    
    if (len != framesize) {
        fprintf(stderr,"error: deocder returned %d as len\n", len);
    } else {
        type = eWAV;
    }
    
    return len;
}

int
audiobuffer::mpeg2udp(audiosocket& audiosock)
{
    static struct audio_t sendbuffer;
    sendbuffer.frame++;
    if (mpegsize() > sizeof(sendbuffer.buffer)) {
        return -1;
    }
    
    snprintf(sendbuffer.name, sizeof(sendbuffer.name), "%s", audiosock.name());
    sendbuffer.len = mpegsize();
    sendbuffer.c_s = store_tv.tv_sec;
    sendbuffer.c_us = store_tv.tv_usec;
    memcpy(sendbuffer.buffer,mpegptr(), mpegsize());
    
    return audiosock.send((void*)&sendbuffer, 64 + sendbuffer.len);
}

int
audiobuffer::udp2mpeg(audio_t* udpaudio)
{
    set_frameindex(udpaudio->frame);
    storempeg(udpaudio->buffer, udpaudio->len, udpaudio->c_s, udpaudio->c_us);
    return 0;
}



int
audiosocket::connect(std::string destination,
                     std::string yourname,
                     int port)
{
    if (sockfd > 0)
        return -1;
    
    socketname = yourname;
    destinationhost = destination;
    destinationport = port;
    
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        fprintf(stderr,"error: socket creation failed\n");
        return -1;
    }
    
    memset(&destinationaddr, 0, sizeof(destinationaddr));
    
    // Filling server information
    destinationaddr.sin_family = AF_INET;
    destinationaddr.sin_port = htons(destinationport);
    destinationaddr.sin_addr.s_addr = inet_addr(destinationhost.c_str());
    
    return 0;
}

int
audiosocket::disconnect()
{
    if (sockfd < 0) {
        return -1;
    } else {
        close(sockfd);
        sockfd = -1;
    }
    
    return 0;
}

int
audiosocket::bind(int port)
{
    receiverport = port;
    
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        fprintf(stderr,"error: socket creation failed\n");
        return -1;
    }
    
    memset(&receiveraddr, 0, sizeof(receiveraddr));
    
    // Filling server information
    receiveraddr.sin_family    = AF_INET; // IPv4
    receiveraddr.sin_addr.s_addr = INADDR_ANY;
    receiveraddr.sin_port = htons(receiverport);
    
    // Bind the socket with the server address
    if ( ::bind(sockfd, (const struct sockaddr *)&receiveraddr,
                sizeof(receiveraddr)) < 0 )
    {
        fprintf(stderr,"error:bind failed\n");
        return -1;
    }
    
    return 0;
}

std::string
audiosocket::getip(struct sockaddr_in* res)
{
    std::string ip;
    char *s = NULL;
    switch(res->sin_family) {
        case AF_INET: {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)res;
            s = (char*)malloc(INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(addr_in->sin_addr), s, INET_ADDRSTRLEN);
            break;
        }
        case AF_INET6: {
            struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)res;
            s = (char*)malloc(INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &(addr_in6->sin6_addr), s, INET6_ADDRSTRLEN);
            break;
        }
        default:
            break;
    }
    ip = s;
    free(s);
    return ip;
}

int
audiosocket::send(void* buffer, size_t len)
{
    int rc = sendto(sockfd, (char *)buffer, len, 0, (const struct sockaddr *) &destinationaddr, sizeof(destinationaddr));
    return rc;
}


audio_t*
audiosocket::receive(){
    static struct audio_t udpaudio;
    static struct sockaddr_in cliaddr;
    static socklen_t len;
    udpaudio.len = 0;
    ssize_t n = recvfrom(sockfd, (char *)&udpaudio, 64+1440,
                         MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                         &len);
    
    fprintf(stdout,"received %ld\n", n);
    
    if (n == (udpaudio.len+64)) {
        return &udpaudio;
    } else {
        return 0;
    }
}
