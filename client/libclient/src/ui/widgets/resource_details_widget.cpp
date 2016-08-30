#include "resource_details_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>

#include <ui/style/helper.h>
#include <ui/widgets/common/text_edit_label.h>
#include <ui/widgets/resource_preview_widget.h>

namespace {

    const int kNoDataFontPixelSize = 24;
    const int kNoDataFontWeight = QFont::Light;
    const int kCameraNameFontPixelSize = 13;
    const int kCameraNameFontWeight = QFont::DemiBold;
    const int kCameraPreviewWidthPixels = 160;
    const int kCameraPreviewHeightPixels = 240;

} // anonymous namespace


QnResourceDetailsWidget::QnResourceDetailsWidget(QWidget* parent) :
    base_type(parent),
    m_preview(new QnResourcePreviewWidget(this)),
    m_nameTextEdit(new QnTextEditLabel(this)),
    m_descriptionLabel(new QLabel(this))
{
    QFont font;
    font.setPixelSize(kNoDataFontPixelSize);
    font.setWeight(kNoDataFontWeight);
    m_preview->setFont(font);
    m_preview->setProperty(style::Properties::kDontPolishFontProperty, true);
    m_preview->setThumbnailSize(QSize(kCameraPreviewWidthPixels, 0));
    /* We cannot pass height to thumbnail manager as it will lose AR. */
    m_preview->setMaximumHeight(kCameraPreviewHeightPixels);
    m_preview->hide();

    font.setPixelSize(kCameraNameFontPixelSize);
    font.setWeight(kCameraNameFontWeight);
    m_nameTextEdit->setFont(font);
    m_nameTextEdit->hide();

    m_descriptionLabel->setWordWrap(true);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(m_preview);
    layout->addWidget(m_nameTextEdit);
    layout->addWidget(m_descriptionLabel);
    layout->addStretch();

    int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
    layout->setContentsMargins(margin, margin, margin, margin);

    int spacing = style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    layout->setSpacing(spacing);
}

QString QnResourceDetailsWidget::name() const
{
    return m_nameTextEdit->toPlainText();
}

void QnResourceDetailsWidget::setName(const QString& name)
{
    m_nameTextEdit->setPlainText(name);
    m_nameTextEdit->setHidden(name.isEmpty());
}

QString QnResourceDetailsWidget::description() const
{
    return m_descriptionLabel->text();
}

void QnResourceDetailsWidget::setDescription(const QString& description)
{
    m_descriptionLabel->setText(description);
    m_descriptionLabel->setHidden(description.isEmpty());
}

const QnResourcePtr& QnResourceDetailsWidget::targetResource() const
{
    return m_preview->targetResource();
}

void QnResourceDetailsWidget::setTargetResource(const QnResourcePtr& target)
{
    m_preview->setTargetResource(target);
    m_preview->setHidden(!m_preview->targetResource());
}
