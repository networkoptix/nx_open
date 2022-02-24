// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::api { struct LayoutTourData; }

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class LayoutTourExecutor;
class LayoutTourReviewController;

class LayoutToursHandler: public QObject, public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutToursHandler(QObject* parent = nullptr);
    virtual ~LayoutToursHandler() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    QnUuid runningTour() const;

private:
    void saveTourToServer(const nx::vms::api::LayoutTourData& tour);
    void removeTourFromServer(const QnUuid& tourId);

private:
    LayoutTourExecutor* m_tourExecutor;
    LayoutTourReviewController* m_reviewController;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
