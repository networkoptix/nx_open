#ifndef ABSTRACTVIDEODECODER_H
#define ABSTRACTVIDEODECODER_H

#include "videodata.h"

class CLAbstractVideoDecoder
{
public:
    CLAbstractVideoDecoder();
    virtual ~CLAbstractVideoDecoder();

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      */
    virtual bool decode(CLVideoData &) = 0;

    virtual void showMotion(bool show ) = 0;

    void setMTDecoding(bool value)
    {
        if (m_mtDecoding != value)
            m_needRecreate = true;

        m_mtDecoding = value;
    }

    inline void setTryHardwareAcceleration(bool tryHardwareAcceleration)
    { m_tryHardwareAcceleration = tryHardwareAcceleration; }
    inline bool isHardwareAccelerationEnabled() const
    { return m_hardwareAccelerationEnabled; }

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
    Q_DISABLE_COPY(CLAbstractVideoDecoder)

    Error m_lastError;

protected:
    bool m_tryHardwareAcceleration;
    bool m_hardwareAccelerationEnabled;

    bool m_mtDecoding;
    bool m_needRecreate;
};


class CLVideoDecoderFactory
{
public:
    static CLAbstractVideoDecoder *createDecoder(int codecId, void *context);
};

#endif // ABSTRACTVIDEODECODER_H
