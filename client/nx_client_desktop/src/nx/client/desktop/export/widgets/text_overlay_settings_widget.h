#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/data/export_media_settings.h>

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

    using Data = ExportTextOverlaySettings;

    const Data& data() const;
    void setData(const Data& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

signals:
    void dataChanged(const Data& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::TextOverlaySettingsWidget> ui;
    Data m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
