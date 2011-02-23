#ifndef audio_device_h_2146
#define audio_device_h_2146
#include "base\aligned_data.h"

class CLRingBuffer;

// this class encapsulates all hardcore work with qt audio buffer
// audio device starts in constructor. and stops at destructor
// put more data by calling write function only if wantMoreData returns true;
// (!!!)check wantMoreData as frequently as possible. do not put more than one decoded audio packet at the time
// if decoded audio packet will be large class handles all buffers potential overflow by itself(by increasing capacity)
// but to avoid big capacity do not put many data at one time


// about class internals:
// it uses small qt buffer; with large qt buffer we had some issues with qt 4.7.1
// instead it extends qt buffer with ring buffer
class CLAudioDevice
{
public:
    CLAudioDevice(QAudioFormat format);
    ~CLAudioDevice();

    // returns true if data must be down mixed into to channels
    bool downmixing() const;

    // returns true if more data wanted; should be followed by write call
    // more data wanted if we can write at least one chunk of data in qt buffer and in ring buffer there is less than one chunk
    bool wantMoreData();

    // writes data into internal buffer
    void write(const char* data, unsigned long size);

    // returns number of ms buffered
    int msInBuffer() const;

    void suspend();

    void resume();

    // removes all data from audio buffers
    void clearAudioBuff();


private:

    unsigned int ms_from_size(const QAudioFormat& format, unsigned long bytes) const;
    unsigned int bytes_from_time(const QAudioFormat& format, unsigned long ms) const;

    // returns how many more chunks qt buffer needs
    int tryMoveDatafromRingBufferToQtBuffer();

private:

    QAudioOutput* m_audioOutput;
    CLRingBuffer* m_ringbuff;

    QIODevice* m_audiobuff; // not owned by this class 
    bool m_downmixing;

    QAudioFormat m_format;

};


#endif //audio_device_h_2146