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
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) override;
    virtual QString description() const;

private:
    int executeContinuousMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeAbsoluteMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeViewportMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    
    int executeGetPosition(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);

    int executeCreatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeUpdatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeRemovePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeActivatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeGetPresets(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);

    int executeCreateTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);
    int executeRemoveTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeActivateTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeGetTours(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);

    int executeUpdateHomePosition(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);
    int executeGetHomePosition(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);

    int executeGetData(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result);

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
