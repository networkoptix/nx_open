#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutTourController;

class LayoutToursHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutToursHandler(QObject* parent = nullptr);
    virtual ~LayoutToursHandler() override;

private:
    void reviewLayoutTour(const ec2::ApiLayoutTourData& tour);
    void saveTourToServer(const ec2::ApiLayoutTourData& tour);
    void removeTourFromServer(const QnUuid& tourId);
    void addItemToReviewLayout(const QnLayoutResourcePtr& layout,
        const ec2::ApiLayoutTourItemData& item);

private:
    LayoutTourController* m_tourController;
    QHash<QnUuid, QnLayoutResourcePtr> m_reviewLayouts;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
