#pragma once

#include <rest/server/fusion_rest_handler.h>

namespace nx::vms::common::p2p::downloader { class Downloader; }

class QnDownloadsRestHandler: public QnFusionRestHandler
{
public:
    QnDownloadsRestHandler(
        nx::vms::common::p2p::downloader::Downloader* downloader);

    nx::vms::common::p2p::downloader::Downloader* downloader() const;

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
    nx::vms::common::p2p::downloader::Downloader* m_downloader = nullptr;
};
