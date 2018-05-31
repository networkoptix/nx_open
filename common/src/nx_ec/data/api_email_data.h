#pragma once

#include "api_globals.h"
#include "api_data.h"

#include <api/model/email_attachment.h>

#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>

#include <nx/vms/api/data/email_settings_data.h>

namespace ec2 {
struct ApiEmailData: nx::vms::api::Data
{
    ApiEmailData() = default;

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
    int timeout = 0;

    QnEmailAttachmentList attachments;
};
#define ApiEmailData_Fields (to)(subject)(body)(plainBody)(timeout)

} // namespace ec2
