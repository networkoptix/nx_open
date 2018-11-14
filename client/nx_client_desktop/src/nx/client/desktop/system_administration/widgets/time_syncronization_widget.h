#pragma once

#include <client_core/connection_context_aware.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/connective.h>

namespace Ui { class TimeSynchronizationWidget; }

namespace nx::client::desktop {

struct TimeSynchronizationWidgetState;
class TimeSynchronizationWidgetStore;
class TimeSynchronizationServersModel;
class TimeSynchronizationWidgetWatcher;
class TimeSynchronizationServersDelegate;

class TimeSynchronizationWidget:
    public Connective<QnAbstractPreferencesWidget>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QnAbstractPreferencesWidget>;

public:
    explicit TimeSynchronizationWidget(QWidget* parent = nullptr);
    virtual ~TimeSynchronizationWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    void setupUi();
    void loadState(const TimeSynchronizationWidgetState& state);

private:
    QScopedPointer<Ui::TimeSynchronizationWidget> ui;
    QPointer<TimeSynchronizationWidgetStore> m_store;
    QPointer<TimeSynchronizationServersModel> m_serversModel;
    QPointer<TimeSynchronizationWidgetWatcher> m_timeWatcher;
    QPointer<TimeSynchronizationServersDelegate> m_delegate;
    int m_tickCount;
};

} // namespace nx::client::desktop
