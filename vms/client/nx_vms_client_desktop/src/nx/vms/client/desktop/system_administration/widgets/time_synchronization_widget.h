// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace Ui { class TimeSynchronizationWidget; }

namespace nx::vms::client::desktop {

struct TimeSynchronizationWidgetState;
class TimeSynchronizationWidgetStore;
class TimeSynchronizationServersModel;
class TimeSynchronizationServerTimeWatcher;
class TimeSynchronizationServerStateWatcher;
class TimeSynchronizationServersDelegate;

class TimeSynchronizationWidget:
    public QnAbstractPreferencesWidget,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit TimeSynchronizationWidget(SystemContext* context, QWidget* parent = nullptr);
    virtual ~TimeSynchronizationWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    void setupUi();
    void setBaseOffsetIndex(int row);
    void loadState(const TimeSynchronizationWidgetState& state);

private:
    QScopedPointer<Ui::TimeSynchronizationWidget> ui;
    QPointer<TimeSynchronizationWidgetStore> m_store;
    QPointer<TimeSynchronizationServersModel> m_serversModel;
    QPointer<TimeSynchronizationServerTimeWatcher> m_timeWatcher;
    QPointer<TimeSynchronizationServerStateWatcher> m_stateWatcher;
    QPointer<TimeSynchronizationServersDelegate> m_delegate;
    bool m_hasChanges = false;
};

} // namespace nx::vms::client::desktop
