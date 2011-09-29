#ifndef ABSTRACTAUDIODECODER_H
#define ABSTRACTAUDIODECODER_H

#include "core/datapacket/mediadatapacket.h"
#include "audiodata.h"

class CLAbstractAudioDecoder
{
public:
    CLAbstractAudioDecoder();
    virtual ~CLAbstractAudioDecoder();

    virtual bool decode(CLAudioData &) = 0;

    enum Error {
        NoError,
        CodecError,
        CodecContextError,
        FrameDecodeError
    };

    inline Error error() const
    { return m_lastError; }

protected:
    inline void setError(Error error)
    { m_lastError = error; }

private:
    Q_DISABLE_COPY(CLAbstractAudioDecoder)

    Error m_lastError;
};


class CLAudioDecoderFactory
{
public:
    static CLAbstractAudioDecoder *createDecoder(QnCompressedAudioDataPtr data);
};

#endif // ABSTRACTAUDIODECODER_H
