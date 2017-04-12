#pragma once

#include <nx/utils/uuid.h>

#include <core/resource_management/manual_camera_searcher.h>
#include <rest/server/json_rest_handler.h>
#include <nx/utils/concurrent.h>
#include <api/model/manual_camera_data.h>

class QnManualCameraAdditionRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnManualCameraAdditionRestHandler();
    ~QnManualCameraAdditionRestHandler();

    virtual int executeGet(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor*) override;

    virtual int executePost(
        const QString& path, const QnRequestParams& params, const QByteArray& body,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;

private:
    int searchStartAction(
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);
    int searchStatusAction(const QnRequestParams& params, QnJsonRestResult& result);
    int searchStopAction(const QnRequestParams& params, QnJsonRestResult& result);
    int addCamerasAction(const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    /**
     * Get status of the manual camera search process. Thread-safe.
     * @param searchProcessUuid Uuid of the process.
     * @return Status of the process.
     */
    QnManualCameraSearchProcessStatus getSearchStatus(const QnUuid& searchProcessUuid);

    /**
     * Check if there is running manual camera search process with the uuid provided.
     * @param searchProcessUuid Uuid of the process.
     * @return True if process is running, false otherwise.
     */
    bool isSearchActive(const QnUuid& searchProcessUuid);

    int addCameras(
        const AddManualCameraData& data,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

private:
    // Mutex that is used to synchronize access to manual camera addition progress.
    QnMutex m_searchProcessMutex;

    QHash<QnUuid, QnManualCameraSearcher*> m_searchProcesses;
    QHash<QnUuid, nx::utils::concurrent::Future<bool>> m_searchProcessRuns;
};
