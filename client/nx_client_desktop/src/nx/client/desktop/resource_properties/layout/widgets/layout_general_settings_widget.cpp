#include "layout_general_settings_widget.h"
#include "ui_layout_general_settings_widget.h"

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>

#include "../redux/layout_settings_dialog_store.h"
#include "../redux/layout_settings_dialog_state.h"


namespace nx {
namespace client {
namespace desktop {

LayoutGeneralSettingsWidget::LayoutGeneralSettingsWidget(
    LayoutSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::LayoutGeneralSettingsWidget)
{
    setupUi();
    connect(store, &LayoutSettingsDialogStore::stateChanged, this,
        &LayoutGeneralSettingsWidget::loadState);

    connect(ui->lockedCheckBox, &QCheckBox::clicked, store,
        [store](bool checked)
        {
            store->setLocked(checked);
        });
}

LayoutGeneralSettingsWidget::~LayoutGeneralSettingsWidget()
{
}

void LayoutGeneralSettingsWidget::setupUi()
{
    ui->setupUi(this);
    ui->lockedCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->lockedCheckBox->setForegroundRole(QPalette::ButtonText);
    setHelpTopic(ui->lockedCheckBox, Qn::LayoutSettings_Locking_Help);
}

void LayoutGeneralSettingsWidget::loadState(const LayoutSettingsDialogState& state)
{
    ui->lockedCheckBox->setChecked(state.locked);
}

} // namespace desktop
} // namespace client
} // namespace nx
