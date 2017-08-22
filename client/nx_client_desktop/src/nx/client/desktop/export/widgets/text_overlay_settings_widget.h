#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/data/export_media_settings.h>

namespace Ui { class TextOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class TextOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    TextOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~TextOverlaySettingsWidget() override;

    const ExportTextOverlaySettings& data() const;
    void setData(const ExportTextOverlaySettings& data);

    int maximumWidth() const;
    void setMaximumWidth(int value);

signals:
    void dataChanged(const ExportTextOverlaySettings& data);

private:
    void updateControls();

private:
    QScopedPointer<Ui::TextOverlaySettingsWidget> ui;
    ExportTextOverlaySettings m_data;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
