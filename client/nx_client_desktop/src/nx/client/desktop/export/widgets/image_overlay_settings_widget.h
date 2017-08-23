#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/data/export_media_settings.h>

namespace Ui { class ImageOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ImageOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ImageOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~ImageOverlaySettingsWidget() override;

    const ExportImageOverlaySettings& data() const;
    void setData(const ExportImageOverlaySettings& data);

    int maximumWidth() const;
    void setMaximumWidth(int value);

signals:
    void dataChanged(const ExportImageOverlaySettings& data);
    void deleteClicked();

private:
    void updateControls();
    void browseForFile();

private:
    QScopedPointer<Ui::ImageOverlaySettingsWidget> ui;
    ExportImageOverlaySettings m_data;
    QString m_lastImageDir;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
