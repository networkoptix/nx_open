// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

/**
 * Wrapper to be used for overloading as a distinct type for camera-related API requests.
 * Flexible camera id string is converted to nx::Uuid from the HTTP request parameter during
 * deserialization.
 */
class QnCameraUuid: public nx::Uuid
{
public:
    QnCameraUuid() = default;
    QnCameraUuid(const nx::Uuid& id): nx::Uuid(id) {}
};

/**
 * Wrapper to be used for overloading as a distinct type for layout-related API requests.
 * Flexible layout id string is converted to nx::Uuid from the HTTP request parameter during
 * deserialization.
 */
class QnLayoutUuid: public nx::Uuid
{
public:
    QnLayoutUuid() = default;
    QnLayoutUuid(const nx::Uuid& id): nx::Uuid(id) {}
};

/**
 * Api structure for getCamerasEx request to filter out desktop cameras from the result.
 */
struct QnCameraDataExQuery
{
    QnCameraUuid id;
    bool showDesktopCameras = false;
};

#define QnCameraDataExQuery_Fields (id)(showDesktopCameras)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraDataExQuery, (json));
