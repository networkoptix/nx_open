#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/settings/export_media_persistent_settings.h>

namespace Ui { class TextOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {

class TextOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    TextOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~TextOverlaySettingsWidget() override;

    const ExportTextOverlayPersistentSettings& data() const;
    void setData(const ExportTextOverlayPersistentSettings& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

signals:
    void dataChanged(const ExportTextOverlayPersistentSettings& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::TextOverlaySettingsWidget> ui;
    ExportTextOverlayPersistentSettings m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
