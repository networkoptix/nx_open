// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QValidator>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "../field_validator.h"

namespace nx::vms::rules::utils {

template <class ValidationPolicy>
QValidator::State serversValidity(const QnMediaServerResourceList& servers)
{
    bool hasInvalid{false};

    for (const auto& server: servers)
    {
        if (!ValidationPolicy::isServerValid(server))
        {
            hasInvalid = true;
            break;
        }
    }

    return hasInvalid ? QValidator::State::Intermediate : QValidator::State::Acceptable;
}

template <class ValidationPolicy>
ValidationResult camerasValidity(
    common::SystemContext* context,
    const QnVirtualCameraResourceList& cameras)
{
    bool hasInvalid{false};

    for (const auto& camera: cameras)
    {
        if (!ValidationPolicy::isResourceValid(context, camera))
        {
            hasInvalid = true;
            break;
        }
    }

    if (!hasInvalid)
        return {};

    return {
        QValidator::State::Intermediate,
        ValidationPolicy::getText(context, cameras)};
}

template <class ValidationPolicy>
ValidationResult cameraValidity(
    common::SystemContext* context,
    const ValidationPolicy& policy,
    const QnVirtualCameraResourcePtr& device)
{
    if (policy.isResourceValid(context, device))
        return {};

    return {
        QValidator::State::Intermediate,
        policy.getText(context, {device})};
}

template <class ValidationPolicy>
ValidationResult cameraValidity(
    common::SystemContext* context, const QnVirtualCameraResourcePtr& device)
{
    if (!NX_ASSERT(device, "Camera resource must be provided"))
        return {QValidator::State::Invalid, QString{}};

    ValidationPolicy policy;
    return cameraValidity(context, policy, device);
}

inline ValidationResult usersValidity(
    const QnSubjectValidationPolicy& policy, bool acceptAll, const UuidSet& subjects)
{
    const auto validity = policy.validity(acceptAll, subjects);
    if (validity == QValidator::State::Acceptable)
        return {};

    return {
        QValidator::State::Intermediate,
        policy.calculateAlert(acceptAll, subjects)
    };
}

NX_VMS_RULES_API ValidationResult layoutValidity(
    common::SystemContext* context,
    const QnLayoutResourcePtr& layout,
    const QnUserResourceList& users);

} // namespace nx::vms::rules::utils
