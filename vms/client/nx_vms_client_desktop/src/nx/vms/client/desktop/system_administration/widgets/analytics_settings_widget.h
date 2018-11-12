#pragma once

#include <nx/utils/uuid.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace nx::vms::client::desktop {

class AnalyticsSettingsWidget: public QnAbstractPreferencesWidget
{
    Q_OBJECT

    using base_type = QnAbstractPreferencesWidget;

public:
    AnalyticsSettingsWidget(QWidget* parent = nullptr);
    virtual ~AnalyticsSettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool hasChanges() const override;
    virtual bool activate(const QUrl& url) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
