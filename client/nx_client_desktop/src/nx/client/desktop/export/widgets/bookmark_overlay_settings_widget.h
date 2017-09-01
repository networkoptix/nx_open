#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/data/export_media_settings.h>

namespace Ui { class BookmarkOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class BookmarkOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    BookmarkOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~BookmarkOverlaySettingsWidget() override;

    struct Data: public ExportTextOverlaySettings
    {
        bool includeDescription = true;
        Data();
    };

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
    QScopedPointer<Ui::BookmarkOverlaySettingsWidget> ui;
    Data m_data;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
