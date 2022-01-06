// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::api::dewarping { struct MediaData; }

namespace nx::vms::client::desktop {

/**
 * Display fisheye preview on the scene according to settings, which are being edited right now.
 * Used in conjunction with File Settings dialog or Camera Settings dialog.
 */
class FisheyePreviewController: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit FisheyePreviewController(QObject* parent = nullptr);

    void preview(
        const QnMediaResourcePtr& resource, const nx::vms::api::dewarping::MediaData& params);

private:
    void rollback();

private:
    // Last resource we were operating on.
    QnMediaResourcePtr m_resource;
};

} // namespace nx::vms::client::desktop
