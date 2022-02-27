// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_details_widget.h"

#include "ui_resource_details_widget.h"

#include <core/resource/camera_resource.h>

#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>

namespace {

static constexpr int kThumbnailPlaceholderFontPixelSize = 24;
static constexpr int kThumbnailPlaceholderFontWeight = QFont::Light;

static constexpr int kCaptionFontPixelSize = 13;
static constexpr int kCaptionFontWeight = QFont::DemiBold;

static constexpr int kCameraThumbnailWidth = 160;

QFont thumbnailPlaceholderFont()
{
    QFont font;
    font.setPixelSize(kThumbnailPlaceholderFontPixelSize);
    font.setWeight(kThumbnailPlaceholderFontWeight);
    return font;
}

QFont captionFont()
{
    QFont font;
    font.setPixelSize(kCaptionFontPixelSize);
    font.setWeight(kCaptionFontWeight);
    return font;
}

} // namespace

namespace nx::vms::client::desktop {

ResourceDetailsWidget::ResourceDetailsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ResourceDetailsWidget),
    m_cameraThumbnailManager(new CameraThumbnailManager)
{
    ui->setupUi(this);
    setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);

    // Thumbnail height hasn't determined as aspect ratio may vary.
    m_cameraThumbnailManager->setThumbnailSize(QSize(kCameraThumbnailWidth, 0));

    ui->cameraPreviewWidget->setFont(thumbnailPlaceholderFont());
    ui->cameraPreviewWidget->setImageProvider(m_cameraThumbnailManager.get());
    ui->cameraPreviewWidget->setProperty(nx::style::Properties::kDontPolishFontProperty, true);

    ui->captionTextLabel->setFont(captionFont());
    ui->warningCaptionLabel->setFont(captionFont());

    const auto warningCaptionColor = colorTheme()->color("yellow_core");
    const auto warningDescriptionColor = colorTheme()->color("yellow_d2");
    setPaletteColor(ui->warningCaptionLabel, QPalette::WindowText, warningCaptionColor);
    setPaletteColor(ui->warningExplanationLabel, QPalette::WindowText, warningDescriptionColor);

    const auto warningIcon = qnSkin->icon("tree/buggy.png");
    ui->warningIconLabel->setPixmap(warningIcon.pixmap(
        QSize(nx::style::Metrics::kDefaultIconSize, nx::style::Metrics::kDefaultIconSize)));

    ui->cameraPreviewWidget->hide();
    ui->captionTextLabel->hide();
    ui->descriptionLabel->hide();
    ui->warningCaptionContainer->hide();
    ui->warningExplanationLabel->hide();
}

ResourceDetailsWidget::~ResourceDetailsWidget()
{
}

void ResourceDetailsWidget::clear()
{
    setUpdatesEnabled(false);

    setThumbnailCameraResource({});
    setCaptionText({});
    setDescriptionText({});
    setWarningCaptionText({});
    setWarningExplanationText({});

    setUpdatesEnabled(true);
}

QnResourcePtr ResourceDetailsWidget::thumbnailCameraResource() const
{
    return m_cameraThumbnailManager->selectedCamera();
}

void ResourceDetailsWidget::setThumbnailCameraResource(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    m_cameraThumbnailManager->selectCamera(camera);
    ui->cameraPreviewWidget->setHidden(camera.isNull());
}

QString ResourceDetailsWidget::captionText() const
{
    return ui->captionTextLabel->toPlainText();
}

void ResourceDetailsWidget::setCaptionText(const QString& name)
{
    ui->captionTextLabel->setPlainText(name.trimmed());
    ui->captionTextLabel->setHidden(captionText().isEmpty());
}

QString ResourceDetailsWidget::descriptionText() const
{
    return ui->descriptionLabel->text();
}

void ResourceDetailsWidget::setDescriptionText(const QString& description)
{
    ui->descriptionLabel->setText(description.trimmed());
    ui->descriptionLabel->setHidden(descriptionText().isEmpty());
}

QString ResourceDetailsWidget::warningCaptionText() const
{
    return ui->warningCaptionLabel->text();
}

void ResourceDetailsWidget::setWarningCaptionText(const QString& text)
{
    ui->warningCaptionLabel->setText(text.trimmed());
    ui->warningCaptionContainer->setHidden(warningCaptionText().isEmpty());
}

QString ResourceDetailsWidget::warningExplanationText() const
{
    return ui->warningExplanationLabel->text();
}

void ResourceDetailsWidget::setWarningExplanationText(const QString& text)
{
    ui->warningExplanationLabel->setText(text.trimmed());
    ui->warningExplanationLabel->setHidden(warningExplanationText().isEmpty());
}

} // namespace nx::vms::client::desktop
