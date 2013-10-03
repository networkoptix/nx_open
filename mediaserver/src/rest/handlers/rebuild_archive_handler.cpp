#include "rebuild_archive_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"
#include "qcoreapplication.h"
#include "settings.h"
#include "recorder/device_file_catalog.h"
#include "recorder/storage_manager.h"

QnRestRebuildArchiveHandler::QnRestRebuildArchiveHandler()
{

}

int QnRestRebuildArchiveHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)
    QString method;
    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "action")
            method = params[i].second;
    }
    
    int progress = -1;
    QString messageStr;
    QString stateStr(lit("unknown"));


    if (method == "start") {
        if (QnStorageManager::instance()->rebuildState() == QnStorageManager::RebuildState_None) {
            QnStorageManager::instance()->rebuildCatalogAsync();
            messageStr = lit("Rebuild archive started");
            progress = 0;
        }
        else {
            progress = QnStorageManager::instance()->rebuildProgress()*100 + 0.5;
            messageStr = lit("Rebuild progress: %1%").arg(progress);
        }
    }
    else if (method == "stop") {
        progress = 100;
        if (QnStorageManager::instance()->rebuildState() != QnStorageManager::RebuildState_None)
            messageStr = lit("Rebuild archive canceled");
        else
            messageStr = lit("Rebuild is not running. nothing to do");
        QnStorageManager::instance()->cancelRebuildCatalogAsync();
    }
    else {
        progress = QnStorageManager::instance()->rebuildProgress()*100 + 0.5;
        if (QnStorageManager::instance()->rebuildState() != QnStorageManager::RebuildState_None) {
            messageStr = lit("Rebuild progress: %1%").arg(progress);
        }
        else {
            messageStr = lit("Rebuild operation is not running");
        }
    }

    QnStorageManager::RebuildState state = QnStorageManager::instance()->rebuildState();
    switch(state)
    {
    case QnStorageManager::RebuildState_None:
        stateStr = "none";
        break;
    case QnStorageManager::RebuildState_WaitForRecordersStopped:
        stateStr = "stop recorders";
        break;
    case QnStorageManager::RebuildState_Started:
        stateStr = "started";
        break;
    }

    result.append(QString("<root><message>%1</message><progress>%2</progress><state>%3</state></root>\n").arg(messageStr).arg(progress).arg(stateStr).toUtf8());

    return CODE_OK;
}

int QnRestRebuildArchiveHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
        return executeGet(path, params, result, contentType);
}

QString QnRestRebuildArchiveHandler::description() const
{
    return 
        "Start or stop rebuilding of media server archive.\n"
        "<BR>Param <b>rebuild</b> - 'start' or 'stop' value"
        "<BR>Return<b>rebuild</b> - return rebuild operation progress if no input param specified";
}
