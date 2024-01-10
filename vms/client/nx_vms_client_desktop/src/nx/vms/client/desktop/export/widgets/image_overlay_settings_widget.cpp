// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_overlay_settings_widget.h"
#include "ui_image_overlay_settings_widget.h"

#include <limits>

#include <QtGui/QImageReader>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace {

static const QColor klight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{klight16Color, "light16"}}},
    {QIcon::Active, {{klight16Color, "light17"}}},
    {QIcon::Selected, {{klight16Color, "light15"}}},
};

} // namespace

namespace nx::vms::client::desktop {

ImageOverlaySettingsWidget::ImageOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ImageOverlaySettingsWidget()),
    m_lastImageDir(appContext()->localSettings()->backgroundsFolder())
{
    ui->setupUi(this);

    ui->sizeSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->opacitySlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->sizeSlider->setMaximum(std::numeric_limits<int>::max());

    auto aligner = new Aligner(this);
    aligner->addWidgets({ ui->sizeLabel, ui->opacityLabel });

    static const auto kDefaultMaximumWidth = 1000;
    static const auto kMimumWidth = 1;
    ui->sizeSlider->setRange(kMimumWidth, kDefaultMaximumWidth);
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

    ui->deleteButton->setIcon(
        qnSkin->icon("text_buttons/delete_20_deprecated.svg", kIconSubstitutions));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &ImageOverlaySettingsWidget::deleteClicked);
}

ImageOverlaySettingsWidget::~ImageOverlaySettingsWidget()
{
}

void ImageOverlaySettingsWidget::browseForFile()
{
    // TODO: #vkutin Implement image selection common dialog.
    // Currently there's code duplication with QnLayoutSettingsDialog

    QnSessionAwareFileDialog dialog(
        this,
        tr("Select file..."),
        m_lastImageDir,
        QnCustomFileDialog::createFilter(QnCustomFileDialog::picturesFilter()));

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

} // namespace nx::vms::client::desktop
