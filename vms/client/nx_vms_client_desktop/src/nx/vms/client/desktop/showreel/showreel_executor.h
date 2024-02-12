// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/workbench/state/workbench_state.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::api {
struct ShowreelData;
struct ShowreelItemData;
using ShowreelItemDataList = std::vector<ShowreelItemData>;
} // namespace nx::vms::api

namespace nx::vms::client::desktop {

class SceneBanner;

class ShowreelExecutor: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ShowreelExecutor(QObject* parent = nullptr);
    virtual ~ShowreelExecutor() override;

    void startShowreel(const nx::vms::api::ShowreelData& showreel);
    void updateShowreel(const nx::vms::api::ShowreelData& showreel);

    /** Stop Showreel with given id if it is running. */
    void stopShowreel(const nx::Uuid& id);

    /** Start Showreel over single layout. */
    void startSingleLayoutShowreel();

    /** Stop currently running Showreel (if any). */
    void stopCurrentShowreel();

    /** Suspend items switch until resumed. */
    void suspendCurrentShowreel();
    void resumeCurrentShowreel();

    nx::Uuid runningShowreel() const;

    void prevShowreelStep();
    void nextShowreelStep();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    void resetShowreelItems(const nx::vms::api::ShowreelItemDataList& items);
    void processShowreelStepInternal(bool forward, bool force);

    void clearWorkbenchState();
    void restoreWorkbenchState(const nx::Uuid& showreelId);

    void setHintVisible(bool visible);

    void startTimer();
    void stopTimer();
    void startShowreelInternal();
    bool isTimerRunning() const;

private:
    enum class Mode
    {
        /** No showreels are running. */
        stopped,
        /** Showreel over single layout is running. */
        singleLayout,
        /** Showreel over multiple layouts is running. */
        multipleLayouts
    };

    Mode m_mode{Mode::stopped};

    struct Item
    {
        LayoutResourcePtr layout;
        int delayMs = 0;
    };

    struct
    {
        // Section for Showreel only
        nx::Uuid id;
        std::vector<Item> items;
        int currentIndex{0};

        // Common section
        int timerId{0};
        QElapsedTimer elapsed;
    } m_showreel;

    WorkbenchState m_lastState;
    QPointer<nx::vms::client::desktop::SceneBanner> m_hintLabel;
};

} // namespace nx::vms::client::desktop
