#pragma once

#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/audio_data_packet.h>

class QnSpeachSynthesisDataProvider : public QnAbstractStreamDataProvider
{
Q_OBJECT
public:
    explicit QnSpeachSynthesisDataProvider(const QString& text);
    virtual ~QnSpeachSynthesisDataProvider();

    virtual void run() override;

    virtual QnAbstractMediaDataPtr getNextData();

    void setText(const QString& text);

signals:
    void finished(QnSpeachSynthesisDataProvider*);

private:
    QByteArray doSynthesis(const QString& text);
    QnConstMediaContextPtr initializeAudioContext();
    void afterRun();

private:
    QString m_text;
    QnConstMediaContextPtr m_ctx;
    QByteArray m_rawBuffer;
    size_t m_curPos;
};
