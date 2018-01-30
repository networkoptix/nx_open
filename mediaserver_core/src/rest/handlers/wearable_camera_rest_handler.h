#pragma once

#include <rest/server/json_rest_handler.h>

class QnWearableLockManager;
class QnWearableUploadManager;

class QnWearableCameraRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
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
    int executeAdd(const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    int executeStatus(const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    int executeLock(const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    int executeExtend(const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    int executeRelease(const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    int executeConsume(const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

private:
    QnWearableLockManager* lockManager(QnJsonRestResult& result) const;
    QnWearableUploadManager* uploadManager(QnJsonRestResult& result) const;
};
