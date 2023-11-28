// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/branding.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>
#include <nx/vms/api/data/type_traits.h>
#include <nx/vms/api/types/smtp_types.h>

namespace nx::vms::api {

using namespace std::chrono_literals;

struct NX_VMS_API EmailSettings
{
    static constexpr int kTlsPort = 587;
    static constexpr int kSslPort = 465;
    static constexpr int kInsecurePort = 25;
    static constexpr std::chrono::seconds kDefaultTimeoutLimit = 5min;

    /**%apidoc Sender email. Used as MAIL FROM. */
    QString email;

    /**%apidoc Target SMTP server. */
    QString server;

    /**%apidoc Username for the SMTP authorization. */
    QString user;

    /**%apidoc Password for the SMTP authorization. This field is write-only. */
    std::optional<QString> password = std::nullopt;

    /**%apidoc[opt] Signature text. Used in the email text. */
    QString signature;

    /**%apidoc[opt] Support link. */
    QString supportAddress = nx::branding::supportAddress();

    /**%apidoc[opt] Connection protocol (TLS/SSL/Insecure). */
    ConnectionType connectionType = ConnectionType::insecure;

    /**%apidoc[opt] SMTP server port. */
    int port = kInsecurePort;

    /**%apidoc[opt] Connection timeout in seconds. */
    std::chrono::seconds timeoutS = kDefaultTimeoutLimit;

    /**%apidoc EHLO SMTP command argument. */
    std::optional<QString> smtpEhloName;

    /**%apidoc[opt] Use a cloud service to send email. */
    bool useCloudServiceToSendEmail = false;

    bool operator==(const EmailSettings& other) const = default;
};
#define EmailSettings_Fields (server)(email)(user)(password)(signature)(supportAddress) \
    (connectionType)(port)(timeoutS)(smtpEhloName)(useCloudServiceToSendEmail)

NX_VMS_API_DECLARE_STRUCT_EX(EmailSettings, (json))
NX_REFLECTION_INSTRUMENT(EmailSettings, EmailSettings_Fields)
NX_REFLECTION_TAG_TYPE(EmailSettings, jsonSerializeChronoDurationAsNumber)

struct NX_VMS_API EmailSettingsRequest
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    QnUuid serverId;

    std::optional<EmailSettings> settings;
};
#define EmailSettingsRequest_Fields (serverId)(settings)

NX_VMS_API_DECLARE_STRUCT_EX(EmailSettingsRequest, (json))
NX_REFLECTION_INSTRUMENT(EmailSettingsRequest, EmailSettingsRequest_Fields)
NX_REFLECTION_TAG_TYPE(EmailSettingsRequest, jsonSerializeChronoDurationAsNumber)

struct NX_VMS_API EmailSettingsReply
{
    int smtpReplyCode;
};
#define EmailSettingsReply_Fields (smtpReplyCode)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(EmailSettingsReply, (json))
NX_REFLECTION_INSTRUMENT(EmailSettingsReply, EmailSettingsReply_Fields)

} // namespace nx::vms::api
