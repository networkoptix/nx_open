#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/settings/media_persistent.h>

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

    const settings::ExportTimestampOverlayPersistent& data() const;
    void setData(const settings::ExportTimestampOverlayPersistent& data);

signals:
    void dataChanged(const settings::ExportTimestampOverlayPersistent& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::TimestampOverlaySettingsWidget> ui;
    settings::ExportTimestampOverlayPersistent m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
