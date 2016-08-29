#pragma once

#include <rest/server/fusion_rest_handler.h>

class QnMultiserverBookmarksRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverBookmarksRestHandler(const QString& path);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* processor) override;
};
