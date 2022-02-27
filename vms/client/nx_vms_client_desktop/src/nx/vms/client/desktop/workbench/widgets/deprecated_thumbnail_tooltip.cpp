// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_thumbnail_tooltip.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kRoundingRadius = 2;
static constexpr BaseTooltip::TailGeometry kTailGeometry = {8, 16, 0};
static constexpr int kNoDataFontPixelSize = 32;
static constexpr int kNoDataFontWeight = QFont::Light;
static constexpr int kMarginSize = 8;

} // namespace

DeprecatedThumbnailTooltip::DeprecatedThumbnailTooltip(QWidget* parent):
    DeprecatedThumbnailTooltip(Qt::TopEdge, parent)
{}

DeprecatedThumbnailTooltip::DeprecatedThumbnailTooltip(Qt::Edge tailBorder, QWidget* parent):
    BaseTooltip(tailBorder, /*filterEvents*/ true, parent),
    m_textLabel(new TextEditLabel(this)),
    m_previewWidget(new nx::vms::client::desktop::AsyncImageWidget(this))
{
    setRoundingRadius(kRoundingRadius);
    setTailGeometry(kTailGeometry);

    auto dots = m_previewWidget->busyIndicator()->dots();
    dots->setDotRadius(style::Metrics::kStandardPadding / 2.0);
    dots->setDotSpacing(static_cast<unsigned int>(style::Metrics::kStandardPadding));

    auto layout = new QVBoxLayout();
    layout->setContentsMargins(
        kMarginSize + (tailBorder == Qt::LeftEdge ? tailGeometry().height : 0),
        kMarginSize + (tailBorder == Qt::TopEdge ? tailGeometry().height : 0),
        kMarginSize + (tailBorder == Qt::RightEdge ? tailGeometry().height : 0),
        kMarginSize + (tailBorder == Qt::BottomEdge ? tailGeometry().height : 0));
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(m_previewWidget);
    layout->addWidget(m_textLabel);
    setLayout(layout);

    QFont font;
    font.setPixelSize(kNoDataFontPixelSize);
    font.setWeight(kNoDataFontWeight);
    m_previewWidget->setFont(font);
    m_previewWidget->setCropMode(nx::vms::client::desktop::AsyncImageWidget::CropMode::notHovered);
    m_previewWidget->setAutoScaleUp(true);

    setThumbnailVisible(true);
}

QString DeprecatedThumbnailTooltip::text() const
{
    return m_textLabel->toPlainText();
}

void DeprecatedThumbnailTooltip::setText(const QString& text)
{
    m_textLabel->setText(text);
}

void DeprecatedThumbnailTooltip::setThumbnailVisible(bool visible)
{
    if (m_previewWidget->isHidden() != visible)
        return;

    m_textLabel->setSizePolicy(
        visible ? QSizePolicy::Ignored : QSizePolicy::Preferred,
        QSizePolicy::Preferred);

    m_textLabel->setWordWrapMode(visible
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::ManualWrap);

    m_previewWidget->setVisible(visible);
}

void DeprecatedThumbnailTooltip::setImageProvider(ImageProvider* provider)
{
    if (m_previewWidget->imageProvider() == provider)
        return;

    m_previewWidget->setImageProvider(provider);
}

void DeprecatedThumbnailTooltip::setMaxThumbnailSize(const QSize& value)
{
    if (m_maxThumbnailSize == value)
        return;

    m_maxThumbnailSize = value;

    // Specify maximum width and height for the widget
    m_previewWidget->setMaximumSize(value);
}

AsyncImageWidget::CropMode DeprecatedThumbnailTooltip::cropMode() const
{
    return m_previewWidget->cropMode();
}

void DeprecatedThumbnailTooltip::setCropMode(AsyncImageWidget::CropMode value)
{
    m_previewWidget->setCropMode(value);
}

} // namespace nx::vms::client::desktop
