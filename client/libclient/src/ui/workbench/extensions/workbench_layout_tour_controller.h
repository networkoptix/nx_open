#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

#include <nx/utils/uuid.h>

class QTimerEvent;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutTourController: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    LayoutTourController(QObject* parent = nullptr);
    virtual ~LayoutTourController() override;

    void startTour(const ec2::ApiLayoutTourData& tour);
    void updateTour(const ec2::ApiLayoutTourData& tour);

    /** Stop tour with given id if it is running. */
    void stopTour(const QnUuid& id);

    QnUuid runningTour() const;

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    /** Stop currently running tour (if any). */
    void stopTourInternal();
    void processTourStep();

    void clearWorkbenchState();
    void restoreWorkbenchState();

private:
    enum class Mode
    {
        Stopped,        //< No tours are running
        SingleLayout,   //< Tour over single layout is running
        MultipleLayouts //< Tour over multiple layouts is running
    };

    Mode m_mode{Mode::Stopped};

    struct
    {
        QnUuid id;
        QnLayoutTourItemList items;
        int timerId = 0;
        int currentIndex = 0;
        QElapsedTimer elapsed;
    } m_tour;

    QnWorkbenchState m_lastState;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
