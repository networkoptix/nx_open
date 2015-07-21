#include <rest/handlers/multiserver_chunks_rest_handler.h>
#include <rest/handlers/camera_history_rest_handler.h>
#include <rest/handlers/multiserver_bookmarks_rest_handler.h>
#include <media_server_main.h>

#include "core/resource/camera_history.h"

    QMutex** qMutex = (QMutex**) mtx;
            *qMutex = new QMutex();
    QMutexLocker lk( &m_mutex );
int main(int argc, char* argv[])
{
    return mediaServerMain(argc, argv);
}
    QMutexLocker lock(&m_stopMutex);
            QMutexLocker lock(&m_stopMutex);
        QnCameraHistoryList cameraHistoryList;
        while (( rez = ec2Connection->getCameraManager()->getCameraHistoryListSync(&cameraHistoryList)) != ec2::ErrorCode::ok)
        for(QnCameraHistoryPtr history: cameraHistoryList)
    QnRestProcessorPool::instance()->registerHandler("api/getCameraParam", new QnGetCameraParamRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/setCameraParam", new QnSetCameraParamRestHandler());

    QnRestProcessorPool::instance()->registerHandler("ec2/recordedTimePeriods", new QnMultiserverChunksRestHandler("ec2/recordedTimePeriods"));
    QnRestProcessorPool::instance()->registerHandler("ec2/cameraHistory", new QnCameraHistoryRestHandler());
    QnRestProcessorPool::instance()->registerHandler("ec2/bookmarks", new QnMultiserverBookmarksRestHandler("ec2/bookmarks"));
