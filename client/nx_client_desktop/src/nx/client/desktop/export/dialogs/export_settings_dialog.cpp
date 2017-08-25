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

namespace {

static const QSize kPreviewSize(512, 288);

} // namespace

ExportSettingsDialog::ExportSettingsDialog(
    QnMediaResourceWidget* widget,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, parent)
{
    setMediaResource(widget->resource());
}

ExportSettingsDialog::ExportSettingsDialog(
    const QnMediaResourcePtr& media,
    const QnTimePeriod& timePeriod,
    QWidget* parent)
    :
    ExportSettingsDialog(timePeriod, parent)
{
    ui->layoutTab->setVisible(false);
    setMediaResource(media);
}

ExportSettingsDialog::ExportSettingsDialog(const QnTimePeriod& timePeriod, QWidget* parent)
    :
    base_type(parent),
    d(new Private(kPreviewSize)),
    ui(new Ui::ExportSettingsDialog)
{
    ui->setupUi(this);
    ui->mediaPreviewLayout->setAlignment(Qt::AlignCenter);
    ui->mediaPreviewLayout->setAlignment(Qt::AlignCenter);

    ui->mediaPreviewWidget->setMaximumSize(kPreviewSize);
    ui->layoutPreviewWidget->setMaximumSize(kPreviewSize);

    ui->mediaFrame->setFixedSize(kPreviewSize
        + QSize(ui->mediaFrame->frameWidth(), ui->mediaFrame->frameWidth()) * 2);
    ui->layoutFrame->setFixedSize(kPreviewSize
        + QSize(ui->mediaFrame->frameWidth(), ui->mediaFrame->frameWidth()) * 2);

    ui->mediaExportSettingsWidget->setMaximumHeight(ui->mediaFrame->maximumHeight());
    ui->layoutExportSettingsWidget->setMaximumHeight(ui->layoutFrame->maximumHeight());

    autoResizePagesToContents(ui->tabWidget,
        QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed), true);

    d->createOverlays(ui->mediaPreviewWidget);

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

    updateSettingsWidgets();

    connect(ui->filenamePanel, &FilenamePanel::filenameChanged, d, &Private::setFilename);

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::dataChanged,
        d, &Private::setTimestampOverlaySettings);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::dataChanged,
        d, &Private::setImageOverlaySettings);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::dataChanged,
        d, &Private::setTextOverlaySettings);

    connect(ui->timestampSettingsPage, &TimestampOverlaySettingsWidget::deleteClicked,
        ui->timestampButton, &SelectableTextButton::deactivate);

    connect(ui->imageSettingsPage, &ImageOverlaySettingsWidget::deleteClicked,
        ui->imageButton, &SelectableTextButton::deactivate);

    connect(ui->textSettingsPage, &TextOverlaySettingsWidget::deleteClicked,
        ui->textButton, &SelectableTextButton::deactivate);
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
    ui->timestampButton->setDeactivationToolTip(tr("Delete Timestamp"));
    ui->timestampButton->setText(tr("Timestamp"));
    ui->timestampButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/timestamp.png")));
    ui->timestampButton->setIcon(qnSkin->icon(
        lit("buttons/timestamp_hovered.png"),
        lit("buttons/timestamp_selected.png")));
    ui->timestampButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->timestampSettingsPage));
    ui->timestampButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(d->overlay(Private::OverlayType::timestamp)));
    connect(d->overlay(Private::OverlayType::timestamp), &ExportOverlayWidget::pressed,
        ui->timestampButton, &SelectableTextButton::click);

    ui->imageButton->setDeactivatable(true);
    ui->imageButton->setDeactivatedText(tr("Add Image"));
    ui->imageButton->setDeactivationToolTip(tr("Delete Image"));
    ui->imageButton->setText(tr("Image"));
    ui->imageButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/image.png")));
    ui->imageButton->setIcon(qnSkin->icon(
        lit("buttons/image_hovered.png"),
        lit("buttons/image_selected.png")));
    ui->imageButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->imageSettingsPage));
    ui->imageButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(d->overlay(Private::OverlayType::image)));
    connect(d->overlay(Private::OverlayType::image), &ExportOverlayWidget::pressed,
        ui->imageButton, &SelectableTextButton::click);

    ui->textButton->setDeactivatable(true);
    ui->textButton->setDeactivatedText(tr("Add Text"));
    ui->textButton->setDeactivationToolTip(tr("Delete Text"));
    ui->textButton->setText(tr("Text"));
    ui->textButton->setDeactivatedIcon(qnSkin->icon(lit("buttons/text.png")));
    ui->textButton->setIcon(qnSkin->icon(
        lit("buttons/text_hovered.png"),
        lit("buttons/text_selected.png")));
    ui->textButton->setProperty(kPagePropertyName,
        qVariantFromValue(ui->textSettingsPage));
    ui->textButton->setProperty(kOverlayPropertyName,
        qVariantFromValue(d->overlay(Private::OverlayType::text)));
    connect(d->overlay(Private::OverlayType::text), &ExportOverlayWidget::pressed,
        ui->textButton, &SelectableTextButton::click);

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
            ui->mediaExportSettingsWidget->setCurrentWidget(page);
        });

    connect(group, &SelectableTextButtonGroup::buttonStateChanged, this,
        [this](SelectableTextButton* button)
        {
            NX_EXPECT(button);
            const auto overlay = button->property(kOverlayPropertyName).value<ExportOverlayWidget*>();
            if (overlay)
            {
                const bool selected = button->state() == SelectableTextButton::State::selected;
                overlay->setHidden(button->state() == SelectableTextButton::State::deactivated);
                overlay->setBorderVisible(selected);
                if (selected)
                    overlay->raise();
            }
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

void ExportSettingsDialog::updateSettingsWidgets()
{
    ui->timestampSettingsPage->setData(d->exportMediaSettings().timestampOverlay);
    ui->imageSettingsPage->setData(d->exportMediaSettings().imageOverlay);
    ui->textSettingsPage->setData(d->exportMediaSettings().textOverlay);
}

void ExportSettingsDialog::setMediaResource(const QnMediaResourcePtr& media)
{
    d->setMediaResource(media);
    ui->mediaPreviewWidget->setImageProvider(d->mediaImageProvider());

    ui->imageSettingsPage->setMaxOverlayWidth(d->fullFrameSize().width());
    ui->textSettingsPage->setMaxOverlayWidth(d->fullFrameSize().width());

    updateSettingsWidgets();
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
