#pragma once

#include <plugins/resource/desktop_camera/desktop_data_provider_base.h>
#include <nx/streaming/av_codec_media_context.h>

extern "C"
{
#include <dsp_effects/speex/speex_preprocess.h>
#include <libavcodec/avcodec.h>
}

class QnDesktopAudioOnlyDataProvider
    : public QnDesktopDataProviderBase
{
    Q_OBJECT;
public:
    QnDesktopAudioOnlyDataProvider(QnResourcePtr res);

    virtual ~QnDesktopAudioOnlyDataProvider();

    virtual bool isInitialized() const override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual void beforeDestroyDataProvider(QnAbstractDataConsumer* dataProviderWrapper) override;

    virtual bool readyToStop() const override;

    virtual void pleaseStop() override;

public slots:
    void processData();

protected:
    virtual void run() override;

private:
    struct AudioSourceInfo
    {
        ~AudioSourceInfo()
        {
            input->stop();
            if (speexPreprocessState)
            {
                speex_preprocess_state_destroy(speexPreprocessState);
                speexPreprocessState = 0;
            }

            if(frameBuffer)
                qFreeAligned(frameBuffer);
        }

        QAudioDeviceInfo deviceInfo;
        QAudioFormat format;
        QIODevice* ioDevice;
        std::unique_ptr<QAudioInput> input;
        QByteArray buffer;
        char* frameBuffer;
        SpeexPreprocessState* speexPreprocessState;
    };

    typedef std::shared_ptr<AudioSourceInfo> AudioSourceInfoPtr;




private:
    quint32 getFrameSize();
    quint32 getSampleRate();

    bool initSpeex();
    bool initAudioEncoder();
    bool initInputDevices();

    void resizeBuffers();
    void startInputs();

    void preprocessAudioBuffers(std::vector<AudioSourceInfoPtr>& preprocessList);
    void analyzeSpectrum(char* buffer);
    QnWritableCompressedAudioDataPtr encodePacket(char* buffer, int inputFrameSize);

    QnAudioFormat fromQtAudioFormat(const QAudioFormat& format) const;

private:

    bool m_initialized;
    bool m_stopping;

    QByteArray m_internalBuffer;

    std::shared_ptr<const QnAvCodecMediaContext> m_encoderCtxPtr;
    uint8_t* m_encoderBuffer;

    std::vector<AudioSourceInfoPtr> m_audioSourcesInfo;
    SpeexPreprocessState* m_speexPreprocessState;
    AVFrame* m_inputFrame;
};


