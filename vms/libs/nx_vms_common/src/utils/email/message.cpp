// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message.h"

#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

namespace nx::email {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Attachment, (json), Attachments_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Message, (json), Message_Fields)

Attachment::Attachment(QString filename, QString mimetype):
    filename(std::move(filename)),
    mimetype(std::move(mimetype))
{
}

Attachment::Attachment(
    QString filename,
    const QString& contentFilename,
    QString mimetype)
    :
    Attachment(std::move(filename), std::move(mimetype))
{
    QFile contentFile(contentFilename);
    contentFile.open(QIODevice::ReadOnly);
    content = contentFile.readAll();
    NX_ASSERT(!content.isEmpty(), "Unable to read '%1' from file '%2'", filename, contentFilename);
}

Attachment::Attachment(
    QString filename,
    QIODevice& io,
    QString mimetype)
    :
    Attachment(std::move(filename), std::move(mimetype))
{
    io.open(QIODevice::ReadOnly);
    content = io.readAll();
    NX_ASSERT(!content.isEmpty(), "Unable to read '%1' from %2", filename, &io);
}

} // namespace nx::email
