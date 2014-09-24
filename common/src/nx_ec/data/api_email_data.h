#ifndef QN_API_EMAIL_DATA_H
#define QN_API_EMAIL_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiEmailSettingsData: ApiData
    {
        ApiEmailSettingsData(): port(0), connectionType(QnEmail::Unsecure) {}

        QString host;
        int port;
        QString user;
        QString from;
        QString password;
        QnEmail::ConnectionType connectionType;
    };
#define ApiEmailSettingsData_Fields (host)(port)(user)(password)(connectionType)
	

    struct ApiEmailData: ApiData
    {
        ApiEmailData() {}

        ApiEmailData (const QStringList& to_, const QString& subject_, const QString& body_, int timeout_, const QnEmailAttachmentList& attachments_)
            : to(to_),
            subject(subject_),
            body(body_),
            timeout(timeout_),
            attachments(attachments_)
        {}

        QStringList to;
        QString subject;
        QString body;
        int timeout;

        QnEmailAttachmentList attachments;
    };
#define ApiEmailData_Fields (to)(subject)(body)(timeout)

} // namespace ec2

#endif // QN_API_EMAIL_DATA_H