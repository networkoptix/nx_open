#include "mustache/mustache_helper.h"

#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QFileInfo>

#include <common/common_globals.h>

bool renderTemplateFromFile(
    const QString& filename,
    const QVariantHash& contextMap,
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
    return true;
}
