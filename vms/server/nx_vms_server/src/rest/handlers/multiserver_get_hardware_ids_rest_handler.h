#pragma once

#include <rest/server/fusion_rest_handler.h>

class QnMultiserverGetHardwareIdsRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverGetHardwareIdsRestHandler(const QString& getHardwareIdsApiMethodPath);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor *owner) override;

private:
    QString m_getHardwareIdsApiMethodPath;
};
