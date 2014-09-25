#ifndef QN_MANUAL_CAMERA_ADDITION_REST_HANDLER_H
#define QN_MANUAL_CAMERA_ADDITION_REST_HANDLER_H

#include <utils/common/uuid.h>

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>

#include <core/resource_management/manual_camera_searcher.h>
#include <rest/server/json_rest_handler.h>
#include <utils/common/concurrent.h>


class QnManualCameraAdditionRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnManualCameraAdditionRestHandler();
    ~QnManualCameraAdditionRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;

private:
    int searchStartAction(const QnRequestParams &params, QnJsonRestResult &result);
    int searchStatusAction(const QnRequestParams &params, QnJsonRestResult &result);
    int searchStopAction(const QnRequestParams &params, QnJsonRestResult &result);
    int addCamerasAction(const QnRequestParams &params, QnJsonRestResult &result);

    /**
     * @brief getSearchStatus               Get status of the manual camera search process. Thread-safe.
     * @param searchProcessUuid             Uuid of the process.
     * @return                              Status of the process.
     */
    QnManualCameraSearchProcessStatus getSearchStatus(const QnUuid &searchProcessUuid);

    /**
     * @brief isSearchActive                Check if there is running manual camera search process with the uuid provided.
     * @param searchProcessUuid             Uuid of the process.
     * @return                              True if process is running, false otherwise.
     */
    bool isSearchActive(const QnUuid &searchProcessUuid);

private:
    /** Mutex that is used to synchronize access to manual camera addition progress. */
    QMutex m_searchProcessMutex;

    QHash<QnUuid, QnManualCameraSearcher*> m_searchProcesses;
    QHash<QnUuid, QnConcurrent::QnFuture<bool> > m_searchProcessRuns;
};

#endif // QN_MANUAL_CAMERA_ADDITION_REST_HANDLER_H
