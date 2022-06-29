// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <client/client_model_types.h>

#include <core/resource/client_resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>


#include <nx/utils/uuid.h>

#include <nx/vms/api/data/layout_tour_data.h>

class QTimerEvent;

namespace nx::vms::client::desktop {

class SceneBanner;

namespace ui {
namespace workbench {

class LayoutTourExecutor: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutTourExecutor(QObject* parent = nullptr);
    virtual ~LayoutTourExecutor() override;

    void startTour(const nx::vms::api::LayoutTourData& tour);
    void updateTour(const nx::vms::api::LayoutTourData& tour);

    /** Stop tour with given id if it is running. */
    void stopTour(const QnUuid& id);

    /** Start tour over single layout. */
    void startSingleLayoutTour();

    /** Stop currently running tour (if any). */
    void stopCurrentTour();

    /** Suspend items switch until resumed. */
    void suspendCurrentTour();
    void resumeCurrentTour();

    QnUuid runningTour() const;

    void prevTourStep();
    void nextTourStep();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    void resetTourItems(const nx::vms::api::LayoutTourItemDataList& items);
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
    QPointer<nx::vms::client::desktop::SceneBanner> m_hintLabel;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
