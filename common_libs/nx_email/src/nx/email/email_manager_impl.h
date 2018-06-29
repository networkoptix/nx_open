#pragma once

#include <utils/email/email_fwd.h>
#include <common/common_module_aware.h>
#include <utility>

class QIODevice;

class EmailManagerImpl: public QnCommonModuleAware
{
public:
    struct Attachment
    {
        Attachment(
            QString filename,
            QString mimetype)
            :
            filename(std::move(filename)),
            mimetype(std::move(mimetype))
        {
        }

        Attachment(
            QString filename,
            const QString& contentFilename,
            QString mimetype);
        Attachment(QString filename, QIODevice& io, QString mimetype);

        QString filename;
        QByteArray content;
        QString mimetype;
    };

    using AttachmentPtr = QSharedPointer<Attachment>;
    using AttachmentList = QList<AttachmentPtr>;

    struct Message
    {
        Message() = default;

        Message(
            QStringList to,
            QString subject,
            QString body,
            QString plainBody,
            int timeout,
            AttachmentList attachments)
            :
            to(std::move(to)),
            subject(std::move(subject)),
            body(std::move(body)),
            plainBody(std::move(plainBody)),
            timeout(timeout),
            attachments(std::move(attachments))
        {
        }

        QStringList to;
        QString subject;
        QString body;
        QString plainBody;
        int timeout = 0;

        AttachmentList attachments;
    };

    EmailManagerImpl(QnCommonModule* commonModule);

    SmtpOperationResult sendEmail(
        const QnEmailSettings& settings,
        const Message& message) const;
    SmtpOperationResult sendEmail(const Message& message) const;
    SmtpOperationResult testConnection(const QnEmailSettings& settings) const;
};
