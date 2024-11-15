// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/system_context_aware.h>

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

constexpr auto kAudioReceiverValidationPolicy = "audioReceiver";
constexpr auto kBookmarkManagementValidationPolicy = "bookmarkManagement";
constexpr auto kCloudUserValidationPolicy = "cloudUser";
constexpr auto kLayoutAccessValidationPolicy = "layoutAccess";
constexpr auto kUserWithEmailValidationPolicy = "userWithEmail";

class NX_VMS_RULES_API TargetUsersField:
    public ResourceFilterActionField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "users")

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll NOTIFY acceptAllChanged)

public:
    TargetUsersField(common::SystemContext* context, const FieldDescriptor* descriptor);

    QnUserResourceSet users() const;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);
};

} // namespace nx::vms::rules
