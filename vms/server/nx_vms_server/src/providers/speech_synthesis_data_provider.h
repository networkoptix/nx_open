#pragma once

#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/audio_data_packet.h>

namespace nx::speech_synthesizer { class TextToWaveServer; }

class QnSpeechSynthesisDataProvider: public QnAbstractStreamDataProvider
{
    Q_OBJECT

private:
    using OpaqueBackend = nx::speech_synthesizer::TextToWaveServer;

public:
    /**
     * Whether the speech synthesis backend is available - if not, the instance of this class
     * should not be created.
     */
    static bool isEnabled();

    /**
     * Creates a singleton instance of the backend and performs its initialization. Should be
     * called before creating the instance of this class, and moved to some context where it will
     * be destroyed after all instances of this class. Can be called if not isEnabled() - returns
     * an empty pointer.
     */
    static std::shared_ptr<OpaqueBackend> backendInstance(const QString& applicationDirPath);

    explicit QnSpeechSynthesisDataProvider(const QString& text);
    virtual ~QnSpeechSynthesisDataProvider();

    virtual void run() override;

    virtual QnAbstractMediaDataPtr getNextData();

    void setText(const QString& text);

signals:
    void finished(QnSpeechSynthesisDataProvider*);

private:
    QByteArray doSynthesis(const QString& text, bool* outStatus);
    QnConstMediaContextPtr initializeAudioContext(const QnAudioFormat& format);
    void afterRun();

private:
    QString m_text;
    QnConstMediaContextPtr m_ctx;
    QByteArray m_rawBuffer;
    int m_curPos;
};
