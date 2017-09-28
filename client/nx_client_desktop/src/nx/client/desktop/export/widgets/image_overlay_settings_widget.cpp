#include "image_overlay_settings_widget.h"
#include "ui_image_overlay_settings_widget.h"

#include <QtGui/QImageReader>
#include <QtWidgets/QApplication>

#include <client/client_settings.h>
#include <ui/common/aligner.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>

namespace nx {
namespace client {
namespace desktop {

ImageOverlaySettingsWidget::ImageOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ImageOverlaySettingsWidget()),
    m_lastImageDir(qnSettings->backgroundsFolder())
{
    ui->setupUi(this);

    ui->sizeSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->opacitySlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ ui->sizeLabel, ui->opacityLabel });

    static const auto kDefaultMaximumWidth = 1000;
    const auto minimumWidth = qMax(QApplication::globalStrut().width(), 1);
    ui->sizeSlider->setRange(minimumWidth, kDefaultMaximumWidth);
    ui->opacitySlider->setRange(0, 100);
    updateControls();

    connect(ui->browseButton, &QPushButton::clicked,
        this, &ImageOverlaySettingsWidget::browseForFile);

    connect(ui->sizeSlider, &QSlider::valueChanged,
        [this](int value)
        {
            m_data.overlayWidth = value;
            emit dataChanged(m_data);
        });

    connect(ui->opacitySlider, &QSlider::valueChanged,
        [this](int value)
        {
            m_data.opacity = value / 100.0;
            emit dataChanged(m_data);
        });

    ui->deleteButton->setIcon(qnSkin->icon(lit("buttons/trash.png")));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &ImageOverlaySettingsWidget::deleteClicked);
}

ImageOverlaySettingsWidget::~ImageOverlaySettingsWidget()
{
}

void ImageOverlaySettingsWidget::browseForFile()
{
    // TODO: #vkutin #common Implement image selection common dialog.
    // Currently there's code duplication with QnLayoutSettingsDialog

    QByteArrayList filters;
    for (const auto& format: QImageReader::supportedImageFormats())
        filters << "*." + format;

    QnSessionAwareFileDialog dialog(this, tr("Select file..."), m_lastImageDir,
        tr("Pictures (%1)").arg(QString::fromLatin1(filters.join(" "))));

    dialog.setFileMode(QFileDialog::ExistingFile);

    if (!dialog.exec())
        return;

    QString fileName = dialog.selectedFile();
    if (fileName.isEmpty())
        return;

    QFileInfo fileInfo(fileName);
    m_lastImageDir = fileInfo.path();

    QImage image;
    if (!image.load(fileName))
    {
        QnMessageBox::warning(this, tr("Error"), tr("Image cannot be loaded."));
        return;
    }

    m_data.image = image;
    m_data.name = fileInfo.fileName();
    m_data.opacity = 1.0;
    m_data.overlayWidth = qMin(image.width(), maximumWidth());
    updateControls();

    emit dataChanged(m_data);
}

void ImageOverlaySettingsWidget::updateControls()
{
    ui->sizeSlider->setValue(m_data.overlayWidth);
    ui->opacitySlider->setValue(int(m_data.opacity * 100.0));
    ui->nameEdit->setText(m_data.name);
}

const ExportImageOverlayPersistentSettings& ImageOverlaySettingsWidget::data() const
{
    return m_data;
}

void ImageOverlaySettingsWidget::setData(const ExportImageOverlayPersistentSettings& data)
{
    m_data = data;
    updateControls();
    emit dataChanged(m_data);
}

int ImageOverlaySettingsWidget::maxOverlayWidth() const
{
    return ui->sizeSlider->maximum();
}

void ImageOverlaySettingsWidget::setMaxOverlayWidth(int value)
{
    return ui->sizeSlider->setMaximum(value);
}

} // namespace desktop
} // namespace client
} // namespace nx
