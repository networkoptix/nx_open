// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QValidator>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

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
        if (!ValidationPolicy::isResourceValid(camera))
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
ValidationResult cameraValidity(common::SystemContext* context, QnVirtualCameraResourcePtr device)
{
    if (ValidationPolicy::isResourceValid(device))
        return {};

    return {
        QValidator::State::Invalid,
        ValidationPolicy::getText(context, {device})};
}

} // namespace nx::vms::rules::utils
