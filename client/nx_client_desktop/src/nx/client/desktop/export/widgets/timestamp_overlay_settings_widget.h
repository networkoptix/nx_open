#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/data/export_media_settings.h>

namespace Ui { class TimestampOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class TimestampOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    TimestampOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~TimestampOverlaySettingsWidget() override;

    const ExportTimestampOverlaySettings& data() const;
    void setData(const ExportTimestampOverlaySettings& data);

signals:
    void dataChanged(const ExportTimestampOverlaySettings& data);

private:
    void updateControls();

private:
    QScopedPointer<Ui::TimestampOverlaySettingsWidget> ui;
    ExportTimestampOverlaySettings m_data;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
