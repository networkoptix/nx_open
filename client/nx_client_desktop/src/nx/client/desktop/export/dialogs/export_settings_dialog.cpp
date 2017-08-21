#include "export_settings_dialog.h"
#include "private/export_settings_dialog_p.h"
#include "ui_export_settings_dialog.h"

#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
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
    d->setMediaResource(widget->resource());
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnMediaResourcePtr& media,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, parent)
{
    ui->layoutTab->setVisible(false);
    d->setMediaResource(media);
}

ExportSettingsDialog::ExportSettingsDialog(const QnTimePeriod& timePeriod, QWidget* parent)
    :
    base_type(parent),
    d(new Private()),
    ui(new Ui::ExportSettingsDialog)
{
    ui->setupUi(this);
    d->createOverlays(ui->cameraPreviewWidget);

    setupSettingsButtons();

    auto exportButton = ui->buttonBox->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    setAccentStyle(exportButton);

    connect(d, &Private::statusChanged, this,
        [this, exportButton](Private::ErrorCode value)
        {
            exportButton->setEnabled(Private::isExportAllowed(value));
        });

    d->loadSettings();

    d->setTimePeriod(timePeriod);

    connect(ui->filenamePanel, &FilenamePanel::filenameChanged, d, &Private::setFilename);

    // FIXME!
    // TODO: #vkutin Update model, not overlay widget.
    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::dataChanged, this,
        [this]() { d->overlay(Private::OverlayType::text)->setText(ui->textSettingsPage->text()); });
}

void ExportSettingsDialog::setupSettingsButtons()
{
    static const auto kPagePropertyName = "_qn_ExportSettingsPage";
    static const auto kOverlayPropertyName = "_qn_ExportSettingsOverlay";

    ui->cameraExportSettingsButton->setText(tr("Export Settings"));
    ui->cameraExportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));
    ui->cameraExportSettingsButton->setState(SelectableTextButton::State::selected);
    ui->cameraExportSettingsButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->exportMediaSettingsPage));

    ui->layoutExportSettingsButton->setText(tr("Export Settings"));
    ui->layoutExportSettingsButton->setIcon(qnSkin->icon(
        lit("buttons/settings_hovered.png"),
        lit("buttons/settings_selected.png")));
    ui->layoutExportSettingsButton->setState(SelectableTextButton::State::selected);

    ui->timestampButton->setDeactivatable(true);
    ui->timestampButton->setDeactivatedText(tr("Add Timestamp"));
    ui->timestampButton->setText(tr("Timestamp"));
    ui->timestampButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/timestamp.png")));
    ui->timestampButton->setIcon(qnSkin->icon(
        lit("buttons/timestamp_hovered.png"),
        lit("buttons/timestamp_selected.png")));
    ui->timestampButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->timestampSettingsPage));
    ui->timestampButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(d->overlay(Private::OverlayType::timestamp)));

    ui->imageButton->setDeactivatable(true);
    ui->imageButton->setDeactivatedText(tr("Add Image"));
    ui->imageButton->setText(tr("Image"));
    ui->imageButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/image.png")));
    ui->imageButton->setIcon(qnSkin->icon(
        lit("buttons/image_hovered.png"),
        lit("buttons/image_selected.png")));
    ui->imageButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->imageSettingsPage));
    ui->imageButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(d->overlay(Private::OverlayType::image)));

    ui->textButton->setDeactivatable(true);
    ui->textButton->setDeactivatedText(tr("Add Text"));
    ui->textButton->setText(tr("Text"));
    ui->textButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/text.png")));
    ui->textButton->setIcon(qnSkin->icon(
        lit("buttons/text_hovered.png"),
        lit("buttons/text_selected.png")));
    ui->textButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->textSettingsPage));
    ui->textButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(d->overlay(Private::OverlayType::text)));

    ui->speedButton->setDeactivatable(true);
    ui->speedButton->setDeactivatedText(tr("Speed Up"));
    ui->speedButton->setText(tr("Speed"));
    ui->speedButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/rapid_review.png")));
    ui->speedButton->setIcon(qnSkin->icon(
        lit("buttons/rapid_review_hovered.png"),
        lit("buttons/rapid_review_selected.png")));
    ui->speedButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->rapidReviewSettingsPage));

    auto group = new SelectableTextButtonGroup(this);
    group->add(ui->cameraExportSettingsButton);
    group->add(ui->timestampButton);
    group->add(ui->imageButton);
    group->add(ui->textButton);
    group->add(ui->speedButton);
    group->setSelectionFallback(ui->cameraExportSettingsButton);

    connect(group, &SelectableTextButtonGroup::selectionChanged, this,
        [this](SelectableTextButton* /*old*/, SelectableTextButton* selected)
        {
            if (!selected)
                return;

            const auto page = selected->property(kPagePropertyName).value<QWidget*>();
            ui->stackedWidget->setCurrentWidget(page);
        });

    connect(group, &SelectableTextButtonGroup::buttonStateChanged, this,
        [this](SelectableTextButton* button)
        {
            NX_EXPECT(button);
            const auto overlay = button->property(kOverlayPropertyName).value<QWidget*>();
            if (overlay)
                overlay->setHidden(button->state() == SelectableTextButton::State::deactivated);
        });
}

ExportSettingsDialog::~ExportSettingsDialog()
{
}

ExportSettingsDialog::Mode ExportSettingsDialog::mode() const
{
    return d->mode();
}

const ExportMediaSettings& ExportSettingsDialog::exportMediaSettings() const
{
    return d->exportMediaSettings();
}

const ExportLayoutSettings& ExportSettingsDialog::exportLayoutSettings() const
{
    return d->exportLayoutSettings();
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
