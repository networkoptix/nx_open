#include "utils/fs/fileutil.h"
#include "mustache/mustache_helper.h"

QString renderTemplateFromFile(const QString& path, const QString& filename, const QVariantHash& contextMap) {
    QFile templateFile(path + lit("/") + filename);
    templateFile.open(QIODevice::ReadOnly);
    QString _template = QString::fromUtf8(templateFile.readAll());

    Mustache::PartialFileLoader partialLoader(path);
    Mustache::Renderer renderer;
    Mustache::QtVariantContext context(contextMap, &partialLoader);
    return renderer.render(_template, &context);
}

