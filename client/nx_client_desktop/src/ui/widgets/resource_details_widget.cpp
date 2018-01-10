#include "resource_details_widget.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QStyle>

#include <nx/client/desktop/image_providers/camera_thumbnail_manager.h>

#include <core/resource/camera_resource.h>

#include <ui/style/helper.h>
#include <ui/widgets/common/text_edit_label.h>
#include <ui/widgets/resource_preview_widget.h>

namespace {

static const int kNoDataFontPixelSize = 24;
static const int kNoDataFontWeight = QFont::Light;
static const int kCameraNameFontPixelSize = 13;
static const int kCameraNameFontWeight = QFont::DemiBold;
static const QSize kCameraPreviewSize(160, 240);

} // namespace


QnResourceDetailsWidget::QnResourceDetailsWidget(QWidget* parent) :
    base_type(parent),
    m_thumbnailManager(new QnCameraThumbnailManager()),
    m_preview(new QnResourcePreviewWidget(this)),
    m_nameTextEdit(new QnTextEditLabel(this)),
    m_descriptionLabel(new QLabel(this))
{
    // We cannot pass height to thumbnail manager as it will lose AR.
    m_thumbnailManager->setThumbnailSize(QSize(kCameraPreviewSize.width(), 0));

    QFont font;
    font.setPixelSize(kNoDataFontPixelSize);
    font.setWeight(kNoDataFontWeight);
    m_preview->setFont(font);
    m_preview->setImageProvider(m_thumbnailManager.data());
    m_preview->setProperty(style::Properties::kDontPolishFontProperty, true);
    m_preview->setMaximumHeight(kCameraPreviewSize.height());
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

QnResourceDetailsWidget::~QnResourceDetailsWidget()
{
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

QnResourcePtr QnResourceDetailsWidget::resource() const
{
    return m_thumbnailManager->selectedCamera();
}

void QnResourceDetailsWidget::setResource(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    m_thumbnailManager->selectCamera(camera);
    m_preview->setHidden(!camera);
}
