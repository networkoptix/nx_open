#pragma once

#include <memory>
#include <QtCore>
#include <QtMultimedia/QAudioInput>
#include <nx/streaming/audio_data_packet.h>
#include <core/resource/resource.h>
#include <ui/screen_recording/video_recorder_settings.h>
#include <core/dataprovider/live_stream_provider.h>
#include <utils/media/voice_spectrum_analyzer.h>

class QnDesktopDataProviderBase : public QnAbstractMediaStreamDataProvider
{
public:

    QnDesktopDataProviderBase(QnResourcePtr ptr);


    static void stereoAudioMux(qint16* a1, qint16* a2, int lenInShort);

    static void monoToStereo(qint16* dst, qint16* src, int lenInShort);

    static void monoToStereo(qint16* dst, qint16* src1, qint16* src2, int lenInShort);



    virtual ~QnDesktopDataProviderBase();

    virtual bool isInitialized() const = 0;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout() = 0;

    virtual void beforeDestroyDataProvider(QnAbstractDataConsumer* dataProviderWrapper) = 0;

    virtual QString lastErrorStr() const;

    virtual bool readyToStop() const = 0;

protected:

    class QnDesktopAudioLayout: public QnResourceAudioLayout
    {
    public:
        QnDesktopAudioLayout(): QnResourceAudioLayout(), m_channels(0) {}
        virtual int channelCount() const override { return m_channels; }
        virtual AudioTrack getAudioTrackInfo(int /*index*/) const override { return m_audioTrack; }
        void setAudioTrackInfo(const AudioTrack& info) { m_audioTrack = info; m_channels = 1;}
    private:
        AudioTrack m_audioTrack;
        int m_channels;
    };


    QPointer<QnVoiceSpectrumAnalyzer> m_soundAnalyzer;
    QSharedPointer<QnDesktopAudioLayout> m_audioLayout;
    QString m_lastErrorStr;
};

