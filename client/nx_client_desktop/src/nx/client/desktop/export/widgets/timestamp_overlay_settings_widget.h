#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/data/export_media_settings.h>

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

    using Data = ExportTimestampOverlaySettings;

    const Data& data() const;
    void setData(const Data& data);

signals:
    void dataChanged(const Data& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::TimestampOverlaySettingsWidget> ui;
    Data m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
