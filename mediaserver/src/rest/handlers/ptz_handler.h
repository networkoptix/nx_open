#ifndef QN_PTZ_HANDLER_H
#define QN_PTZ_HANDLER_H

#include <QElapsedTimer>
#include <QMutex>
#include "rest/server/request_handler.h"

class QnPtzHandler: public QnRestRequestHandler
{
public:
    QnPtzHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const;
private:
    bool checkSequence(const QString& id, int sequence);
    void cleanupOldSequence();
private:
    struct SequenceInfo
    {
        SequenceInfo(int seq = 0): sequence(seq) { m_timer.restart(); }

        QElapsedTimer m_timer;
        int sequence;
    };

    bool m_detectAvailableOnly;
    
    static QMap<QString, SequenceInfo> m_sequencedRequests;
    static QMutex m_sequenceMutex;
};

#endif // QN_PTZ_HANDLER_H
