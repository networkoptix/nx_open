#pragma once

#include <QtCore/QObject>

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
    void openToursLayout();
    void saveTourToServer(const ec2::ApiLayoutTourData& tour);
    void removeTourFromServer(const QnUuid& tourId);

private:
    LayoutTourController* m_controller;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
