#pragma once
#include "../../common/src/core/dataprovider/abstract_streamdataprovider.h"
#include <QtCore/QString>

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
    QnMediaContextPtr initializeAudioContext();
    void afterRun();

private:
    QString m_text;
    QnMediaContextPtr m_ctx;
    QByteArray m_rawBuffer;
    size_t m_curPos;
};
