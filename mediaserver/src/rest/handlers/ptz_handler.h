#ifndef QN_PTZ_HANDLER_H
#define QN_PTZ_HANDLER_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include <utils/common/enum_name_mapper.h>

#include <core/ptz/ptz_fwd.h>

#include <rest/server/json_rest_handler.h>

class QnPtzHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnPtzHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
    virtual QString description() const;

private:
    int executeContinuousMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeAbsoluteMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeRelativeMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeGetPosition(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);

    bool checkSequence(const QString& id, int sequence);
    void cleanupOldSequence();

private:
    struct SequenceInfo {
        SequenceInfo(int seq = 0): sequence(seq) { m_timer.restart(); }

        QElapsedTimer m_timer;
        int sequence;
    };

    bool m_detectAvailableOnly;
    
    QMap<QString, SequenceInfo> m_sequencedRequests;
    QMutex m_sequenceMutex;
};

#endif // QN_PTZ_HANDLER_H
