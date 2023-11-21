// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_details_widget.h"
#include "ui_resource_details_widget.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kThumbnailPlaceholderFontPixelSize = 24;
static constexpr auto kThumbnailPlaceholderFontWeight = QFont::Light;

static constexpr auto kCaptionFontWeight = QFont::DemiBold;

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
    font.setPixelSize(fontConfig()->normal().pixelSize());
    font.setWeight(kCaptionFontWeight);
    return font;
}

QWidget* createWarningWidget(const QString& caption, const QString& message)
{
    if (!NX_ASSERT(!caption.isEmpty() || !message.isEmpty()))
        return nullptr;

    const auto iconSize = QSize(
        nx::style::Metrics::kDefaultIconSize,
        nx::style::Metrics::kDefaultIconSize);
    const auto warningPixmap = qnSkin->icon("tree/buggy.png").pixmap(iconSize);
    const auto warningCaptionColor = nx::vms::client::core::colorTheme()->color("yellow_core");
    const auto warningMessageColor = nx::vms::client::core::colorTheme()->color("yellow_d2");

    auto warningWidget = new QWidget();

    auto warningWidgetLayout = new QVBoxLayout(warningWidget);
    warningWidgetLayout->setSpacing(2);
    warningWidgetLayout->setContentsMargins({});

    auto warningCaptionLayout = new QHBoxLayout();
    warningCaptionLayout->setSpacing(4);
    warningCaptionLayout->setContentsMargins({});

    auto warningIconLabel = new QLabel(warningWidget);
    warningIconLabel->setPixmap(warningPixmap);

    auto warningCaptionLabel = new QLabel(warningWidget);
    warningCaptionLabel->setText(caption);
    warningCaptionLabel->setFont(captionFont());
    setPaletteColor(warningCaptionLabel, QPalette::WindowText, warningCaptionColor);

    warningCaptionLayout->addWidget(warningIconLabel);
    warningCaptionLayout->addWidget(warningCaptionLabel);
    warningCaptionLayout->addStretch();

    auto warningMessageLabel = new QLabel(warningWidget);
    warningMessageLabel->setText(message);
    warningMessageLabel->setWordWrap(true);
    setPaletteColor(warningMessageLabel, QPalette::WindowText, warningMessageColor);

    warningWidgetLayout->addLayout(warningCaptionLayout);
    warningWidgetLayout->addWidget(warningMessageLabel);

    return warningWidget;
}

} // namespace

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

    ui->captionLabel->setFont(captionFont());

    ui->cameraPreviewWidget->hide();
    ui->captionLabel->hide();
    ui->messageLabel->hide();
}

ResourceDetailsWidget::~ResourceDetailsWidget()
{
}

void ResourceDetailsWidget::clear()
{
    setUpdatesEnabled(false);

    setThumbnailCameraResource({});
    setCaption({});
    clearMessage();
    clearWarningMessages();

    setUpdatesEnabled(true);
}

void ResourceDetailsWidget::setThumbnailCameraResource(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    m_cameraThumbnailManager->selectCamera(camera);
    ui->cameraPreviewWidget->setHidden(camera.isNull());
}

void ResourceDetailsWidget::setCaption(const QString& caption)
{
    const auto trimmedCaption = caption.trimmed();
    ui->captionLabel->setPlainText(trimmedCaption);
    ui->captionLabel->setHidden(trimmedCaption.isEmpty());
}

void ResourceDetailsWidget::setMessage(
    const QString& message,
    const QColor& color)
{
    setPaletteColor(ui->messageLabel, QPalette::WindowText, color);

    const auto trimmedMessage = message.trimmed();
    ui->messageLabel->setText(trimmedMessage);
    ui->messageLabel->setHidden(trimmedMessage.isEmpty());
}

void ResourceDetailsWidget::clearMessage()
{
    setMessage({});
}

void ResourceDetailsWidget::addWarningMessage(
    const QString& warningCaption,
    const QString& warningMessage)
{
    if (const auto warningWidget = createWarningWidget(warningCaption, warningMessage))
        ui->warningContainer->layout()->addWidget(warningWidget);
}

void ResourceDetailsWidget::clearWarningMessages()
{
    while (auto warningItem = ui->warningContainer->layout()->takeAt(0))
    {
        delete warningItem->widget();
        delete warningItem;
    }
}

} // namespace nx::vms::client::desktop
