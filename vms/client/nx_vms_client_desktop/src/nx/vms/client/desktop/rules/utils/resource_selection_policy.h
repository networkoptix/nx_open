// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>

namespace nx::vms::client::desktop::rules {

template<bool EmptyListIsValid = true,
    bool MultiChoiceListIsValid = true,
    bool ShowRecordingIndicator = false>
struct BaseCameraSelectPolicy
{
    static bool emptyListIsValid() { return EmptyListIsValid; }
    static bool multiChoiceListIsValid() { return MultiChoiceListIsValid; }
    static bool showRecordingIndicator() { return ShowRecordingIndicator; }
};

struct MultiChoicePolicy: public BaseCameraSelectPolicy<>
{
    static bool isResourceValid(const QnResourcePtr& /*resource*/) { return true; }
    static QString getText(const QnResourceList& /*resources*/) { return {}; }

    using resource_type = QnResource;
};

struct SingleChoicePolicy: public BaseCameraSelectPolicy<false, false, false>
{
    static bool isResourceValid(const QnResourcePtr& /*resource*/) { return true; }
    static QString getText(const QnResourceList& resources)
    {
        const auto cameras = resources.filtered<QnVirtualCameraResource>();
        if (cameras.size() != 1)
            return tr("Select exactly one camera");

        return QnResourceDisplayInfo(cameras.first()).toString(Qn::RI_NameOnly);
    }

    using resource_type = QnResource;
    Q_DECLARE_TR_FUNCTIONS(SingleChoicePolicy)
};

} // namespace nx::vms::client::desktop::rules
