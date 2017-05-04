#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <ui/workbench/workbench_state_manager.h>

namespace nx {
namespace client {
namespace desktop {
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
    virtual void loadState(const QnWorkbenchState& state) override;
    virtual void submitState(QnWorkbenchState* state) override;

private:
    void saveTourToServer(const ec2::ApiLayoutTourData& tour);
    void removeTourFromServer(const QnUuid& tourId);

private:
    LayoutTourExecutor* m_tourExecutor;
    LayoutTourReviewController* m_reviewController;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
