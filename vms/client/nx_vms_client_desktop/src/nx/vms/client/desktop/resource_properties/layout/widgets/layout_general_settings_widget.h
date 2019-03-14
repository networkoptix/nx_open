#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class LayoutGeneralSettingsWidget; }

namespace nx::vms::client::desktop {

struct LayoutSettingsDialogState;
class LayoutSettingsDialogStore;

class LayoutGeneralSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit LayoutGeneralSettingsWidget(
        LayoutSettingsDialogStore* store,
        QWidget* parent = nullptr);
    virtual ~LayoutGeneralSettingsWidget() override;

private:
    void setupUi();
    void loadState(const LayoutSettingsDialogState& state);

private:
    QScopedPointer<Ui::LayoutGeneralSettingsWidget> ui;
};

} // namespace nx::vms::client::desktop
