#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/settings/media_persistent.h>

namespace Ui { class BookmarkOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {

class BookmarkOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    BookmarkOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~BookmarkOverlaySettingsWidget() override;

    const settings::ExportBookmarkOverlayPersistent& data() const;
    void setData(const settings::ExportBookmarkOverlayPersistent& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

signals:
    void dataChanged(const settings::ExportBookmarkOverlayPersistent& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::BookmarkOverlaySettingsWidget> ui;
    settings::ExportBookmarkOverlayPersistent m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
