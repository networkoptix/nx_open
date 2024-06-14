// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/system_context.h>

struct NX_VMS_COMMON_API QnAllowAnyCameraPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnAllowAnyCameraPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext*, const QnVirtualCameraResourcePtr&) { return true; }
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
    static bool showAllCamerasSwitch() { return false; }
};

struct NX_VMS_COMMON_API QnRequireCameraPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnRequireCameraPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext*, const QnVirtualCameraResourcePtr&) { return true; }
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
    static bool showAllCamerasSwitch() { return false; }
};

class NX_VMS_COMMON_API QnCameraInputPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraInputPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
    static bool showAllCamerasSwitch() { return true; }
};

class NX_VMS_COMMON_API QnCameraOutputPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraOutputPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
    static bool showAllCamerasSwitch() { return true; }
};

class NX_VMS_COMMON_API QnExecPtzPresetPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnExecPtzPresetPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return false; }
    static bool showRecordingIndicator() { return false; }
    static bool canUseSourceCamera() { return false; }
    static bool showAllCamerasSwitch() { return true; }
};

class NX_VMS_COMMON_API QnCameraMotionPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraMotionPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return true; }
    static bool showAllCamerasSwitch() { return true; }
};

class NX_VMS_COMMON_API QnCameraAudioTransmitPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraAudioTransmitPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr &camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
    static bool showAllCamerasSwitch() { return true; }
};

class NX_VMS_COMMON_API QnCameraRecordingPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraRecordingPolicy)

public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr &camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return true; }
    static bool showAllCamerasSwitch() { return true; }
};

class NX_VMS_COMMON_API QnCameraAnalyticsPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraAnalyticsPolicy)

public:
    using resource_type = QnVirtualCameraResource;
    static bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera);
    static QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
    static bool showAllCamerasSwitch() { return true; }
};

using QnBookmarkActionPolicy = QnCameraRecordingPolicy;

class NX_VMS_COMMON_API QnFullscreenCameraPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnFullscreenCameraPolicy)

public:
    typedef QnVirtualCameraResource resource_type;

    void setLayouts(QnLayoutResourceList layouts);
    bool isResourceValid(
        nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera) const;
    QString getText(
        nx::vms::common::SystemContext* context,
        const QnResourceList& resources,
        const bool detailed = true) const;

    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return false; }
    static bool showRecordingIndicator() { return false; }
    static bool canUseSourceCamera() { return true; }
    static bool showAllCamerasSwitch() { return false; }

private:
    QnLayoutResourceList m_layouts;
};
