#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <client/client_model_types.h>

#include <nx_ec/data/api_layout_tour_data.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutToursHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutToursHandler(QObject* parent = nullptr);

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    void openToursLayout();

    void startTour(const ec2::ApiLayoutTourData& tour);
    void processTourStep();
    void stopTour();

private:
    //TODO: #GDM #3.1 move to separate controller
    QnUuid m_runningTourId;
    QnWorkbenchState m_lastState;
    int m_timerId = 0;
    int m_currentIndex = 0;
    QElapsedTimer m_elapsed;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
