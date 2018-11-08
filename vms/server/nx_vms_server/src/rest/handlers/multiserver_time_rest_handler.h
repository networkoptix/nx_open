#pragma once

#include <rest/server/fusion_rest_handler.h>

/**
 * @return Local PC time and time zone for all servers in the system.
 */
class QnMultiserverTimeRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverTimeRestHandler(const QString& getTimeApiMethodPath);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor *owner) override;

private:
    QString m_getTimeApiMethodPath;
};
