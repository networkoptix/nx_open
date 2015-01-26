#include "mustache_helper.h"

#ifdef ENABLE_SENDMAIL

#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QFileInfo>

#include <common/common_globals.h>

QString renderTemplateFromFile(const QString& filename, const QVariantHash& contextMap) {
    QFile templateFile(filename);
    templateFile.open(QIODevice::ReadOnly);
    QString _template = QString::fromUtf8(templateFile.readAll());

    Mustache::PartialFileLoader partialLoader(QFileInfo(filename).path());
    Mustache::Renderer renderer;
    Mustache::QtVariantContext context(contextMap, &partialLoader);
    return renderer.render(_template, &context);
}

#endif // ENABLE_SENDMAIL
