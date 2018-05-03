#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/export/settings/export_media_persistent_settings.h>

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

    const ExportBookmarkOverlayPersistentSettings& data() const;
    void setData(const ExportBookmarkOverlayPersistentSettings& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

signals:
    void dataChanged(const ExportBookmarkOverlayPersistentSettings& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::BookmarkOverlaySettingsWidget> ui;
    ExportBookmarkOverlayPersistentSettings m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
