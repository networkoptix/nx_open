#ifndef QN_PTZ_HANDLER_H
#define QN_PTZ_HANDLER_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include <rest/server/json_rest_handler.h>

class QnPtzHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnPtzHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParams &params, JsonResult &result) override;
    virtual QString description() const;

private:
    bool checkSequence(const QString& id, int sequence);
    void cleanupOldSequence();

private:
    struct SequenceInfo {
        SequenceInfo(int seq = 0): sequence(seq) { m_timer.restart(); }

        QElapsedTimer m_timer;
        int sequence;
    };

    QHash<QString, int> m_actionByName;
    bool m_detectAvailableOnly;
    
    static QMap<QString, SequenceInfo> m_sequencedRequests;
    static QMutex m_sequenceMutex;
};

#endif // QN_PTZ_HANDLER_H
