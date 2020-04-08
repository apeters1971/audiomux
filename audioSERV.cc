
#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"
#include "audiobuffer.hpp"
#include <sys/time.h>
#include <thread>

#define SAMPLE_RATE  (48000)
#define MPEG_BIT_RATE 192000
#define FRAMES_PER_BUFFER (120)
#define NUM_CHANNELS    (2)
typedef short SAMPLE;

audiobuffermanager audiomanager;
audioqueue audioq;
audiocodec audiocoder;
audiosocket audiosock;

void udpreceiver()
{
    size_t lastframe=0;
    do {
        audiobuffermanager::shared_buffer audio = audiomanager.get_buffer();
        struct audio_t* udpaudio = audiosock.receive();
        if (udpaudio) {
            fprintf(stdout,"frame=%lu last-frame=%lu diff=%d\n", udpaudio->frame, lastframe, udpaudio->frame-lastframe);
            lastframe = udpaudio->frame;
            audio->udp2mpeg(udpaudio);
            audioq.add_output(audio);
        } else {
            fprintf(stdout,"udpreceive failed ...\n");
        }
    } while(1);
}

void udpsender()
{
    while (!audioq.output_size()) {
        Pa_Sleep(2);
    }
    audiobuffermanager::shared_buffer audio = audioq.get_output();
    audio->mpeg2udp(audiosock);
}



int main()
{
    if (audiosock.bind()) {
        exit(-1);
    }
        
    audiomanager.configure(SAMPLE_RATE/FRAMES_PER_BUFFER, SAMPLE_RATE, NUM_CHANNELS, FRAMES_PER_BUFFER, sizeof(SAMPLE) );
    audiomanager.reserve(SAMPLE_RATE/FRAMES_PER_BUFFER);
    audiocoder.configure(SAMPLE_RATE, NUM_CHANNELS, MPEG_BIT_RATE );
    
    std::thread updReceiverThread(udpreceiver);
    std::thread udpSenderThread(udpsender);
    
    updReceiverThread.join();
    udpSenderThread.join();
}
