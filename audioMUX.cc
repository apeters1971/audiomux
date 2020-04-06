/** @file audioMUX.cc
	@brief Record input into an array; Encode and send via UDP
	@author Andreas-Joachim Peters
*/


#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"
#include "audiobuffer.hpp"
#include <sys/time.h>
#include <thread>

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (48000)
#define MPEG_BIT_RATE 192000
#define FRAMES_PER_BUFFER (120)
#define NUM_SECONDS     (5)
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG     (0) /**/
/** Set to 1 if you want to capture the recording to a file. */
#define WRITE_TO_FILE   (0)

/* Select sample format. */

#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"


audiobuffermanager audiomanager_w;
audioqueue audioq_w;
audiocodec audiocoder_w;

audiobuffermanager audiomanager_r;
audioqueue audioq_r;
audiocodec audiocoder_r;


double interval(struct timeval& tv1, struct timeval& tv2)
{
    return (((tv2.tv_sec-tv1.tv_sec)*1000000) + (tv2.tv_usec-tv1.tv_usec))/1000.0;
}


/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    struct timezone tz;
    struct timeval tv1,tv2,tv3,tv4;
    static size_t frameindex = 0;
    static size_t callbacks=0;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    long framesToCalc;
    long i;
    int finished;
    
    
    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;


    framesToCalc = framesPerBuffer;
    finished = paContinue;

    gettimeofday(&tv1,&tz);
    
    frameindex += framesToCalc;
    
    callbacks++;
    audiobuffermanager::shared_buffer audio = audiomanager_w.get_buffer();
    audio->set_frameindex(frameindex);
    audio->store((char*)inputBuffer);
    audioq_w.add_output(audio);
    gettimeofday(&tv2,&tz);
    int code_len = audio->wav2mpeg(audiocoder_w);
    gettimeofday(&tv3,&tz);
    int decode_len = 0; //audio->mpeg2wav(audiocoder_w);
    gettimeofday(&tv4,&tz);

    if (!(callbacks%400)) {
        printf("output-queue: %lu len=%d decode-len=%d music=%lx t_store:%.03f t_enc:%.03f t_dec=%.03f\n",
               audioq_w.output_size(),
               code_len,
               decode_len,
               audio->music(),
               interval(tv2,tv1),
               interval(tv3,tv2),
               interval(tv4,tv3)
               );
    }
    
    return finished;
}

/* This routine will be called by the PortAudio engin
 e when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    static size_t frameindex=0;
    SAMPLE *rptr = 0;//&data->recordedSamples[frameindex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE*)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = frameindex;

    (void) inputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    audiobuffermanager::shared_buffer audio;
    fprintf(stdout, "info: queue size = %lu\n", audioq_w.output_size());
    if (audioq_w.output_size()) {
        // we have some audio to play
        audio = audioq_w.get_output();
        
        if (audio) {
            switch(audio->type) {
                case audiobuffer::eWAV:
                    fprintf(stderr,"info: found wav\n");
                    break;
                case audiobuffer::eMPEG:
                    fprintf(stderr,"info: found mpeg\n");
                    if (audio->mpeg2wav(audiocoder_w) != audio->getFramesize()) {
                        audiomanager_w.put_buffer(audio);
                        // play silence
                        audio = nullptr;
                    }
                    break;
                default:
                    audiomanager_w.put_buffer(audio);
                    // play silence
                    audio = nullptr;
                    break;
                    
            }
        }
        if (audio) {
            // copy the audio buffer
            memcpy(outputBuffer, audio->ptr(), audio->size());
            fprintf(stdout,"age=%.02f\n", audio->age_in_ms());
        }
    }
    
    if (!audio) {
        // play some silence
        for( i=0; i<framesPerBuffer; i++ )
        {
            *wptr++ = 0;
            if( NUM_CHANNELS == 2 ) *wptr++ = 0;
            //*wptr++ = *rptr++;  /* left */
            //if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
    }
    frameindex+= framesPerBuffer;
    finished = paContinue;

    return finished;
}

void recorder()
{
    PaStreamParameters  inputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
       
    audiomanager_w.configure(SAMPLE_RATE/FRAMES_PER_BUFFER, SAMPLE_RATE, NUM_CHANNELS, FRAMES_PER_BUFFER, sizeof(SAMPLE) );
    audiomanager_w.reserve(SAMPLE_RATE/FRAMES_PER_BUFFER);
    audiocoder_w.configure(SAMPLE_RATE, NUM_CHANNELS, MPEG_BIT_RATE );

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done_rec;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
              0 );
    if( err != paNoError ) goto done_rec;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done_rec;
    printf("\n=== Now recording!! Please speak into the microphone. ===\n"); fflush(stdout);

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(1000);
    }
    if( err < 0 ) goto done_rec;

    err = Pa_CloseStream( stream );
    
    
done_rec:
    Pa_Terminate();
    
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
}

void player()
{
    PaStreamParameters  outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;


    audiomanager_r.configure(SAMPLE_RATE/FRAMES_PER_BUFFER, SAMPLE_RATE, NUM_CHANNELS, FRAMES_PER_BUFFER, sizeof(SAMPLE) );
    audiomanager_r.reserve(SAMPLE_RATE/FRAMES_PER_BUFFER);
    audiocoder_r.configure(SAMPLE_RATE, NUM_CHANNELS, MPEG_BIT_RATE );
      
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        goto done;
    }
    outputParameters.channelCount = 2;                     /* stereo output */
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    
    printf("\n=== Now playing back. ===\n"); fflush(stdout);
    err = Pa_OpenStream(
                        &stream,
                        NULL, /* no input */
                        &outputParameters,
                        SAMPLE_RATE,
                        FRAMES_PER_BUFFER,
                        paClipOff,      /* we won't output out of range samples so don't bother clipping them */
                        playCallback,
                        0 );
    if( err != paNoError ) goto done;
    
    if( stream )
    {
        err = Pa_StartStream( stream );
        if( err != paNoError ) goto done;
        
        printf("Waiting for playback to finish.\n"); fflush(stdout);
        
        while( ( err = Pa_IsStreamActive( stream ) ) == 1 ) Pa_Sleep(100);
        if( err < 0 ) goto done;
        
        err = Pa_CloseStream( stream );
        if( err != paNoError ) goto done;
        
        printf("Done.\n"); fflush(stdout);
    }
    
done:
    Pa_Terminate();

    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
}


int main(void)
{
    if( Pa_Initialize() != paNoError ) {
        fprintf(stderr,"error: failed to initialize port audio\n");
        exit(-1);
    }
    std::thread recorderThread(recorder);
    std::thread playerThread(player);
    recorderThread.join();
    playerThread.join();
}

