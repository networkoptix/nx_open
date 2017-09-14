#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/settings/media_persistent.h>

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

    const settings::ExportTextOverlayPersistent& data() const;
    void setData(const settings::ExportTextOverlayPersistent& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

signals:
    void dataChanged(const settings::ExportTextOverlayPersistent& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::TextOverlaySettingsWidget> ui;
    settings::ExportTextOverlayPersistent m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
