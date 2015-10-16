#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnBookmarksOverlayWidgetPrivate;
class QnBookmarksOverlayWidget : public GraphicsWidget {
public:
    QnBookmarksOverlayWidget(QGraphicsWidget *parent = nullptr);
    ~QnBookmarksOverlayWidget();

    QnCameraBookmarkList bookmarks() const;
    void setBookmarks(const QnCameraBookmarkList &bookmarks);

private:
    Q_DECLARE_PRIVATE(QnBookmarksOverlayWidget)
    QScopedPointer<QnBookmarksOverlayWidgetPrivate> d_ptr;
};
