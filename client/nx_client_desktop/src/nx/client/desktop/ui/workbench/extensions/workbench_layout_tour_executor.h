#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <client/client_model_types.h>

#include <core/resource/client_resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

#include <nx/utils/uuid.h>

class QnGraphicsMessageBox;
class QTimerEvent;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutTourExecutor: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    LayoutTourExecutor(QObject* parent = nullptr);
    virtual ~LayoutTourExecutor() override;

    void startTour(const ec2::ApiLayoutTourData& tour);
    void updateTour(const ec2::ApiLayoutTourData& tour);

    /** Stop tour with given id if it is running. */
    void stopTour(const QnUuid& id);

    /** Start tour over single layout. */
    void startSingleLayoutTour();

    /** Stop currently running tour (if any). */
    void stopCurrentTour();

    QnUuid runningTour() const;

    void prevTourStep();
    void nextTourStep();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    void resetTourItems(const ec2::ApiLayoutTourItemDataList& items);
    void processTourStepInternal(bool forward, bool force);

    void clearWorkbenchState();
    void restoreWorkbenchState(const QnUuid& tourId);

    void setHintVisible(bool visible);

    void startTimer();
    void stopTimer();
    void startTourInternal();
    bool isTimerRunning() const;

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
        // Section for layout tour only
        QnUuid id;
        QnLayoutTourItemList items;
        int currentIndex{0};

        // Common section
        int timerId{0};
        QElapsedTimer elapsed;
    } m_tour;

    QnWorkbenchState m_lastState;
    QPointer<QnGraphicsMessageBox> m_hintLabel;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
