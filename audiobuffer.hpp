//
//  audiobuffer.hpp
//  
//
//  Created by Andreas Peters on 03.04.20.
//

#ifndef audiobuffer_hpp
#define audiobuffer_hpp

#include <stdio.h>
#include <vector>
#include <memory>
#include <string.h>
#include <queue>
#include <mutex>
#include <deque>

#include "opus.h"

class audiocodec {
public:
    audiocodec() {encoder = NULL; decoder = NULL;}
    int configure(int samplingrate, int channels, int bitrate);
    virtual ~audiocodec(){
        if (encoder) {
            opus_encoder_destroy(encoder);
        }
        if (decoder) {
            opus_decoder_destroy(decoder);
        }
    }
    
    OpusEncoder* getEncoder() {return encoder;}
    OpusDecoder* getDecoder() {return decoder;}
private:
    
    OpusEncoder* encoder;
    OpusDecoder* decoder;
};

class audiobuffer : public std::vector<unsigned char> {
public:
    audiobuffer(size_t _samplingrate,
                int _channels,
                size_t _framesize,
                size_t _samplesize) : samplingrate(_samplingrate), channels(_channels), framesize(_framesize), samplesize(_samplesize), type(eEMPTY)
    {
        printf("buffer %lu\n", framesize*channels*samplesize);
        reserve(framesize*channels*samplesize);
        resize(framesize*channels*samplesize);
        silence();
    }
    
    audiobuffer(size_t _samplingrate,
                int _channels,
                size_t _framesize) : samplingrate(_samplingrate), channels(_channels), framesize(_framesize), type(eEMPTY)
    {
        samplesize=2;
        printf("buffer %lu\n", framesize*channels*samplesize);
        reserve(framesize*channels*samplesize);
        resize(framesize*channels*samplesize);
        silence();
        samplesize=2;
        mpegbuffer.reserve(1500);
        mpegbuffer.resize(1500);
  
    }
    
    virtual ~audiobuffer() {}
    
    void set_samplesize(size_t size) {
        samplesize = size;
        reserve(framesize*channels*samplesize);
        resize(framesize*channels*samplesize);
    }   
    
    void set_frameindex(uint64_t index) {
        frameindex = index;
    }
    
    uint64_t frame() {
        return frameindex;
    }
    
    void silence() {
        memset(ptr(), 0, size());
    }
    
    unsigned char* ptr() {
      return &(operator[](0));
    }
    
    unsigned char* mpegptr() {
      return &(mpegbuffer[0]);
    }
    
    size_t music() {
        size_t music_sum=0;
        for (size_t i = 0 ; i< size(); ++i) {
            music_sum += ptr()[i];
        }
        return music_sum;
    }
    
    void store(const char* input) {
        printf("%lu %lu\n", size(), framesize*samplesize*channels);
        memcpy(ptr(), (char*)input, framesize * samplesize * channels);
        type = eWAV;
    }
    
    int wav2mpeg(audiocodec& codec);
    int mpeg2wav(audiocodec& codec);
    int udp2mpeg();
    int mpeg2udp();
    
    enum BufferType {eEMPTY, eWAV, eMPEG, eUDP};
    
    BufferType type;
    
private:
    std::vector<unsigned char> mpegbuffer;
    uint64_t samplingrate;
    int channels;
    size_t framesize;
    size_t samplesize;
    uint64_t frameindex;
};

class audiobuffermanager {
public:
    audiobuffermanager(size_t _max = 128,
                       size_t _default_size = 120*2*2)
    {
        max = _max;
        buffersize = _default_size;
        queued_size = 0;
        inflight_size = 0;
    }
    
    virtual ~audiobuffermanager(){}
    
    typedef std::shared_ptr<audiobuffer> shared_buffer;

    void configure(size_t _max,
                   size_t _samplingrate,
                   int _channels,
                   size_t _framesize,
                   size_t _samplesize
                   )
    {
        max = _max;
        buffersize = _channels*framesize*2;
        samplingrate = _samplingrate;
        channels = _channels;
        framesize = _framesize;
        samplesize = _samplesize;
    }

    
    void reserve(size_t n) {
        // create <n> buffers in the manager if less than max
        std::vector<shared_buffer> vec;
        for (size_t i = 0 ; i < n; ++i) {
            vec.push_back(get_buffer());
        }
        for (size_t i = 0 ; i < n; ++i) {
            put_buffer(vec[i]);
        }
    }
    
    shared_buffer get_buffer()
    {
        std::lock_guard<std::mutex> guard(mMutex);

        if (!queue.size()) {
            
            shared_buffer buf = std::make_shared<audiobuffer>(
                                                 samplingrate, channels, framesize);
            buf->set_samplesize(samplesize);
            inflight_size += buf->size();
            return buf;
        } else {
            shared_buffer buffer = queue.front();
            queued_size -= buffer->capacity();
            buffer->resize(channels*framesize*samplesize);
            buffer->reserve(channels*framesize*samplesize);
            inflight_size += buffer->capacity();
            queue.pop();
            return buffer;
        }
    }
    
    void put_buffer(shared_buffer buffer)
    {
        std::lock_guard<std::mutex> guard(mMutex);

        if (inflight_size >= buffer->capacity()) {
            inflight_size -= buffer->capacity();
        } else {
            inflight_size = 0;
        }

        if (queue.size() == max) {
            printf("# created audio buffer : size=%lu\n", queue.size());
            return;
        } else {
            queue.push(buffer);
            buffer->resize(buffersize);
            buffer->reserve(buffersize);
            buffer->shrink_to_fit();
            buffer->silence();
            queued_size += buffersize;
            printf("# created audio buffer : size=%lu\n", queue.size());
            return;
        }
    }
    
    const size_t queued()
    {
        std::lock_guard<std::mutex> guard(mMutex);
        return queued_size;
    }

    const size_t inflight()
    {
        std::lock_guard<std::mutex> guard(mMutex);
        return inflight_size;
    }
    
private:
    std::mutex mMutex;
    std::queue<shared_buffer> queue;
    size_t max;
    size_t buffersize;
    size_t queued_size;
    size_t inflight_size;
    size_t samplingrate;
    size_t framesize;
    size_t samplesize;
    int channels;
};

class audioqueue {
public:
    audioqueue() {
        n_output = n_input = 0;
    }
    
    virtual ~audioqueue() {}
    
    void add_output(audiobuffermanager::shared_buffer out) {
        std::lock_guard<std::mutex> guard(mMutex);
        output.push_back(out);
        n_output++;
    }
    
    void add_input(audiobuffermanager::shared_buffer in) {
        std::lock_guard<std::mutex> guard(mMutex);
        input.push_back(in);
        n_input++;
    }
    
    size_t output_size() { return n_output; }
    size_t inptut_size() { return n_input; }
    
private:
    size_t n_output;
    size_t n_input;
    std::mutex mMutex;
    std::deque<audiobuffermanager::shared_buffer> input;
    std::deque<audiobuffermanager::shared_buffer> output;
};

#endif /* audiobuffer_hpp */
