#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {
namespace settings {

class PoESettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    PoESettingsWidget(QWidget* parent = nullptr);
    virtual ~PoESettingsWidget() override;

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

public:
    void setServerId(const QnUuid& value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace settings
} // namespace nx::vms::client::desktop
