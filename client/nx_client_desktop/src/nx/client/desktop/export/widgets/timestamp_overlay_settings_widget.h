#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/settings/export_media_persistent_settings.h>

namespace Ui { class TimestampOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {

class TimestampOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    TimestampOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~TimestampOverlaySettingsWidget() override;

    const ExportTimestampOverlayPersistentSettings& data() const;
    void setData(const ExportTimestampOverlayPersistentSettings& data);

    bool formatEnabled() const;
    void setFormatEnabled(bool value);

signals:
    void dataChanged(const ExportTimestampOverlayPersistentSettings& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::TimestampOverlaySettingsWidget> ui;
    ExportTimestampOverlayPersistentSettings m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
