// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIODevice>
#include <QtCore/QSharedPointer>

#include <nx/fusion/model_functions_fwd.h>

#include "email_fwd.h"

namespace nx::email {

struct NX_VMS_COMMON_API Attachment
{
    Attachment() = default;
    Attachment(QString filename, QString mimetype);
    Attachment(QString filename, const QString& contentFilename, QString mimetype);
    Attachment(QString filename, QIODevice& io, QString mimetype);

    bool operator==(const Attachment& other) const = default;

    QString filename;
    QByteArray content;
    QString mimetype;
};

#define Attachments_Fields (filename)(content)(mimetype)
QN_FUSION_DECLARE_FUNCTIONS(Attachment, (json), NX_VMS_COMMON_API)

struct NX_VMS_COMMON_API Message
{
    QString subject;
    QString body;
    QString plainBody;

    AttachmentList attachments;

    bool operator==(const Message& other) const = default;
};

#define Message_Fields (subject)(body)(plainBody)(attachments)
QN_FUSION_DECLARE_FUNCTIONS(Message, (json), NX_VMS_COMMON_API)

} // namespace nx::email
