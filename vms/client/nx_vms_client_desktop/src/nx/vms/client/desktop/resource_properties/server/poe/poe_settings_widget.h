#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class PoeSettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    PoeSettingsWidget(QWidget* parent = nullptr);
    virtual ~PoeSettingsWidget() override;

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    void setAutoUpdate(bool value);

public:
    void setServerId(const QnUuid& value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
