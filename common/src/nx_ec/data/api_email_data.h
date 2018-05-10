#pragma once

#include "api_globals.h"
#include "api_data.h"

#include <api/model/email_attachment.h>

#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>

namespace ec2 {

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
#define ApiEmailSettingsData_Fields (host)(port)(user)(from)(password)(connectionType)

struct ApiEmailData: ApiData
{
    ApiEmailData() {}

    ApiEmailData(
        const QStringList& to,
        const QString& subject,
        const QString& body,
        const QString& plainBody,
        int timeout,
        const QnEmailAttachmentList& attachments)
        :
        to(to),
        subject(subject),
        body(body),
        plainBody(plainBody),
        timeout(timeout),
        attachments(attachments)
    {
    }

    QStringList to;
    QString subject;
    QString body;
    QString plainBody;
    int timeout;

    QnEmailAttachmentList attachments;
};
#define ApiEmailData_Fields (to)(subject)(body)(plainBody)(timeout)

} // namespace ec2
