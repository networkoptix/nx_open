// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

/**
 * Wrapper to be used for overloading as a distinct type for camera-related API requests.
 * Flexible camera id string is converted to QnUuid from the HTTP request parameter during
 * deserialization.
 */
class QnCameraUuid: public QnUuid
{
public:
    QnCameraUuid() = default;
    QnCameraUuid(const QnUuid& id): QnUuid(id) {}
};

/**
 * Wrapper to be used for overloading as a distinct type for layout-related API requests.
 * Flexible layout id string is converted to QnUuid from the HTTP request parameter during
 * deserialization.
 */
class QnLayoutUuid: public QnUuid
{
public:
    QnLayoutUuid() = default;
    QnLayoutUuid(const QnUuid& id): QnUuid(id) {}
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
