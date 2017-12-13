#pragma once

#include <rest/server/fusion_rest_handler.h>

namespace nx {
namespace mediaserver {
namespace rest {

class Updates2RestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& bodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;

    virtual int executePut(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& bodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;

    virtual int executeDelete(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;

private:
};

} // namespace rest
} // namespace mediaserver
} // namespace nx
