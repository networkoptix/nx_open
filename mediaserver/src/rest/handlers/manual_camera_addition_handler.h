#ifndef QN_MANUAL_CAMERA_ADDITION_HANDLER_H
#define QN_MANUAL_CAMERA_ADDITION_HANDLER_H

#include <QtCore/QUuid>

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>

#include <core/resource_managment/manual_camera_searcher.h>

#include <rest/server/json_rest_handler.h>

class QnManualCameraAdditionHandler: public QnJsonRestHandler {
public:
    QnManualCameraAdditionHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) override;
    virtual QString description() const;

private:
    int searchStartAction(const QnRequestParamList &params, JsonResult &result);
    int searchStatusAction(const QnRequestParamList &params, JsonResult &result);
    int searchStopAction(const QnRequestParamList &params, JsonResult &result);
    int addCamerasAction(const QnRequestParamList &params, JsonResult &result);



    /**
     * @brief getSearchStatus               Get status of the manual camera search process. Thread-safe.
     * @param searchProcessUuid             Uuid of the process.
     * @return                              Status of the process.
     */
    QnManualCameraSearchStatus getSearchStatus(const QUuid &searchProcessUuid);

    /**
     * @brief getSearchResults              Get results of the manual camera search process. Thread-safe.
     * @param searchProcessUuid             Uuid of the process.
     * @return                              Results of the process.
     */
    QnManualCameraSearchCameraList getSearchResults(const QUuid &searchProcessUuid);

    /**
     * @brief isSearchActive                Check if there is running manual camera search process with the uuid provided.
     * @param searchProcessUuid             Uuid of the process.
     * @return                              True if process is running, false otherwise.
     */
    bool isSearchActive(const QUuid &searchProcessUuid);

private:
    /** Mutex that is used to synchronize access to manual camera addition progress. */
    QMutex m_searchProcessMutex;

    QHash<QUuid, QnManualCameraSearcher> m_searchProcesses;
};

#endif // QN_MANUAL_CAMERA_ADDITION_HANDLER_H
