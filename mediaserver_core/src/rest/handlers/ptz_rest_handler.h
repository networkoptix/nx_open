#pragma once

#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include <core/ptz/ptz_fwd.h>

#include <rest/server/json_rest_handler.h>
#include <functional>
#include <nx/mediaserver/server_module_aware.h>

class QnPtzRestHandler:
    public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT

public:
    QnPtzRestHandler(QnMediaServerModule* serverModule);

    virtual QStringList cameraIdUrlParams() const override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    typedef std::function<int()> AsyncFunc;

    int executeContinuousMove(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeContinuousFocus(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeAbsoluteMove(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeViewportMove(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeRelativeMove(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeRelativeFocus(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetPosition(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeCreatePreset(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeUpdatePreset(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeRemovePreset(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeActivatePreset(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetPresets(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeCreateTour(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result);

    int executeRemoveTour(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeActivateTour(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetTours(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetActiveObject(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeUpdateHomeObject(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetHomeObject(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetAuxilaryTraits(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeRunAuxilaryCommand(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    int executeGetData(
        const QnPtzControllerPtr& controller,
        const QnRequestParams& params,
        QnJsonRestResult& result);

    bool checkSequence(const QString& id, int sequence);
    void cleanupOldSequence();

    int execCommandAsync(const QString& sequence, AsyncFunc function) const;
    void asyncExecutor(const QString& sequence, AsyncFunc function) const;

private:
    struct SequenceInfo
    {
        SequenceInfo(int seq = 0): sequence(seq) { m_timer.restart(); }

        QElapsedTimer m_timer;
        int sequence;
    };

    bool m_detectAvailableOnly;

    QMap<QString, SequenceInfo> m_sequencedRequests;
    QnMutex m_sequenceMutex;


    struct AsyncExecInfo
    {
        AsyncExecInfo(): inProgress() {}

        bool inProgress;
        AsyncFunc nextCommand;
    };

    static QMap<QString, AsyncExecInfo> m_workers;
    static QnMutex m_asyncExecMutex;
};
