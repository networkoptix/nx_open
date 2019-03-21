#pragma once
#if !defined(EDGE_SERVER)

#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/audio_data_packet.h>

class QnSpeechSynthesisDataProvider: public QnAbstractStreamDataProvider
{
    Q_OBJECT

public:
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
    size_t m_curPos;
};

#endif // !defined(EDGE_SERVER)
