// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <plugins/resource/desktop_camera/desktop_data_provider_base.h>
#include <nx/media/ffmpeg/audio_encoder.h>
#include <nx/streaming/av_codec_media_context.h>

extern "C"
{
#include <speex/speex_preprocess.h>
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
    virtual AudioLayoutConstPtr getAudioLayout() override;
    virtual void beforeDestroyDataProvider(QnAbstractMediaDataReceptor* dataProviderWrapper) override;
    virtual bool readyToStop() const override;
    virtual void pleaseStop() override;

public slots:
    void processData();

protected:
    virtual void run() override;

private:
    struct AudioSourceInfo;
    typedef std::shared_ptr<AudioSourceInfo> AudioSourceInfoPtr;

    static QAudioFormat getAppropriateAudioFormat(
        const QAudioDeviceInfo& deviceInfo,
        QString* errorString = nullptr);

private:
    bool initSpeex(int frameSize, int sampleRate);
    bool initAudioEncoder();
    bool initInputDevices();
    void resizeBuffers(int frameSize);
    void startInputs();
    void preprocessAudioBuffers(std::vector<AudioSourceInfoPtr>& preprocessList);

private:
    bool m_initialized;
    bool m_stopping;
    int m_frameSize = 0;
    QByteArray m_internalBuffer;
    nx::media::ffmpeg::AudioEncoder m_audioEncoder;
    std::vector<AudioSourceInfoPtr> m_audioSourcesInfo;
    SpeexPreprocessState* m_speexPreprocessState;
};


