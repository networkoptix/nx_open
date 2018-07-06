#pragma once

#include <QtCore/QScopedPointer>

#include <rest/server/fusion_rest_handler.h>

class QnMultiserverEventsRestHandler: public QnFusionRestHandler
{
public:
    explicit QnMultiserverEventsRestHandler(const QString& path);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* processor) override;

private:
    class Private;
    QScopedPointer<Private> d;
};
