// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIODevice>
#include <QtCore/QSharedPointer>

namespace nx::email {

struct NX_VMS_COMMON_API Attachment
{
    Attachment() = default;
    Attachment(QString filename, QString mimetype);
    Attachment(QString filename, const QString& contentFilename, QString mimetype);
    Attachment(QString filename, QIODevice& io, QString mimetype);

    QString filename;
    QByteArray content;
    QString mimetype;
};

using AttachmentPtr = QSharedPointer<Attachment>;
using AttachmentList = QList<AttachmentPtr>;

struct Message
{
    QString subject;
    QString body;
    QString plainBody;

    AttachmentList attachments;
};

} // namespace nx::email
