#include "mustache_helper.h"

#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QFileInfo>
#include <QtCore/qjsondocument.h>
#include <nx/utils/log/log.h>

bool renderTemplateFromFile(
    const QString& filename,
    const QVariantMap& contextMap,
    QString* const renderedMail)
{
    QFile templateFile(filename);
    if( !templateFile.open(QIODevice::ReadOnly) )
        return false;
    QString _template = QString::fromUtf8(templateFile.readAll());

    Mustache::PartialFileLoader partialLoader(QFileInfo(filename).path());
    Mustache::Renderer renderer;
    Mustache::QtVariantContext context(contextMap, &partialLoader);
    *renderedMail = renderer.render(_template, &context);

    QJsonDocument json = QJsonDocument::fromVariant(contextMap);
    QByteArray jsonData = json.toJson();

    NX_LOG(lm("Rendering template %1").arg(filename), cl_logDEBUG2);
    NX_LOG(lm("Body:\n%1").arg(_template), cl_logDEBUG2);
    NX_LOG(lm("Context:\n%1").arg(jsonData), cl_logDEBUG2);

    return true;
}
