#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

struct QnMediaDewarpingParams;

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

    void preview(const QnMediaResourcePtr& resource, const QnMediaDewarpingParams& params);

private:
    void rollback();

private:
    // Last resource we were operating on.
    QnMediaResourcePtr m_resource;
};

} // namespace nx::vms::client::desktop
