#include "export_settings_dialog.h"
#include "private/export_settings_dialog_p.h"
#include "ui_export_settings_dialog.h"

#include <ui/style/skin.h>
#include <nx/client/desktop/ui/common/selectable_text_button_group.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportSettingsDialog::ExportSettingsDialog(
    QnMediaResourceWidget* widget,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, parent)
{
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnVirtualCameraResourcePtr& camera,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, parent)
{
}

ExportSettingsDialog::ExportSettingsDialog(const QnTimePeriod& timePeriod, QWidget* parent):
    base_type(parent),
    d(new Private()),
    ui(new Ui::ExportSettingsDialog)
{
    ui->setupUi(this);

    ui->exportSettingsButton->setText(tr("Export Settings"));
    ui->exportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));

    ui->timestampButton->setDeactivatable(true);
    ui->timestampButton->setDeactivatedText(tr("Add Timestamp"));
    ui->timestampButton->setText(tr("Timestamp"));
    ui->timestampButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/timestamp.png")));
    ui->timestampButton->setIcon(qnSkin->icon(
        lit("buttons/timestamp_hovered.png"),
        lit("buttons/timestamp_selected.png")));

    ui->imageButton->setDeactivatable(true);
    ui->imageButton->setDeactivatedText(tr("Add Image"));
    ui->imageButton->setText(tr("Image"));
    ui->imageButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/image.png")));
    ui->imageButton->setIcon(qnSkin->icon(
        lit("buttons/image_hovered.png"),
        lit("buttons/image_selected.png")));

    ui->textButton->setDeactivatable(true);
    ui->textButton->setDeactivatedText(tr("Add Text"));
    ui->textButton->setText(tr("Text"));
    ui->textButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/text.png")));
    ui->textButton->setIcon(qnSkin->icon(
        lit("buttons/text_hovered.png"),
        lit("buttons/text_selected.png")));

    ui->speedButton->setDeactivatable(true);
    ui->speedButton->setDeactivatedText(tr("Speed Up"));
    ui->speedButton->setText(tr("Speed"));
    ui->speedButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/rapid_review.png")));
    ui->speedButton->setIcon(qnSkin->icon(
        lit("buttons/rapid_review_hovered.png"),
        lit("buttons/rapid_review_selected.png")));

    auto group = new SelectableTextButtonGroup(this);
    group->add(ui->exportSettingsButton);
    group->add(ui->timestampButton);
    group->add(ui->imageButton);
    group->add(ui->textButton);
    group->add(ui->speedButton);
    group->setSelectionFallback(ui->exportSettingsButton);

    connect(group, &SelectableTextButtonGroup::selectionChanged,
        [this](SelectableTextButton* /*old*/, SelectableTextButton* selected)
        {
            if (!selected)
                return;
            if (selected == ui->exportSettingsButton)
                ui->stackedWidget->setCurrentWidget(ui->exportSettingsPage);
            else if (selected == ui->timestampButton)
                ui->stackedWidget->setCurrentWidget(ui->timestampSettingsPage);
            else if (selected == ui->imageButton)
                ui->stackedWidget->setCurrentWidget(ui->imageSettingsPage);
            else if (selected == ui->textButton)
                ui->stackedWidget->setCurrentWidget(ui->textSettingsPage);
            else if (selected == ui->speedButton)
                ui->stackedWidget->setCurrentWidget(ui->rapidReviewSettingsPage);
        });
}

ExportSettingsDialog::~ExportSettingsDialog()
{
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
