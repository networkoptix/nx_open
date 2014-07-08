#include "mustache_helper.h"

#ifdef ENABLE_MUSTACHE

#include <QtCore/QFile>
#include <QtCore/QIODevice>

#include <common/common_globals.h>

QString renderTemplateFromFile(const QString& path, const QString& filename, const QVariantHash& contextMap) {
    QFile templateFile(path + lit("/") + filename);
    templateFile.open(QIODevice::ReadOnly);
    QString _template = QString::fromUtf8(templateFile.readAll());

    Mustache::PartialFileLoader partialLoader(path);
    Mustache::Renderer renderer;
    Mustache::QtVariantContext context(contextMap, &partialLoader);
    return renderer.render(_template, &context);
}

#endif // ENABLE_MUSTACHE
