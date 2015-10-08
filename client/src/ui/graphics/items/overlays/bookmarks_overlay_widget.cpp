#include "bookmarks_overlay_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <core/resource/camera_bookmark.h>

#include <ui/graphics/items/controls/bookmark_item.h>

namespace {
    const int maximumBookmarkWidth = 250;
    const int layoutSpacing = 1;
}

class QnBookmarksOverlayWidgetPrivate {
    Q_DECLARE_PUBLIC(QnBookmarksOverlayWidget)
    QnBookmarksOverlayWidget *q_ptr;

public:
    QnBookmarksOverlayWidgetPrivate(QnBookmarksOverlayWidget *parent);

    QnCameraBookmarkList bookmarksList;

    QGraphicsWidget *contentWidget;
    QGraphicsLinearLayout *mainLayout;
    QGraphicsLinearLayout *bookmarksLayout;

    void clearLayout();
    void setBookmarks(const QnCameraBookmarkList &bookmarks);
};


QnBookmarksOverlayWidget::QnBookmarksOverlayWidget(QGraphicsWidget *parent)
    : GraphicsWidget(parent)
    , d_ptr(new QnBookmarksOverlayWidgetPrivate(this))
{
    Q_D(QnBookmarksOverlayWidget);

    setFlag(QGraphicsItem::ItemClipsChildrenToShape);

    d->contentWidget = new QGraphicsWidget(this);
    d->contentWidget->setMaximumWidth(maximumBookmarkWidth);
    d->bookmarksLayout = new QGraphicsLinearLayout(Qt::Vertical);
    d->bookmarksLayout->setSpacing(layoutSpacing);
    d->contentWidget->setLayout(d->bookmarksLayout);

    d->mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    d->mainLayout->addStretch();
    d->mainLayout->addItem(d->contentWidget);
    d->mainLayout->setContentsMargins(4, 20, 4, 20);
    setLayout(d->mainLayout);
}

QnBookmarksOverlayWidget::~QnBookmarksOverlayWidget() {
}

QnCameraBookmarkList QnBookmarksOverlayWidget::bookmarks() const {
    Q_D(const QnBookmarksOverlayWidget);
    return d->bookmarksList;
}

void QnBookmarksOverlayWidget::setBookmarks(const QnCameraBookmarkList &bookmarks) {
    Q_D(QnBookmarksOverlayWidget);
    d->setBookmarks(bookmarks);
}


QnBookmarksOverlayWidgetPrivate::QnBookmarksOverlayWidgetPrivate(QnBookmarksOverlayWidget *parent)
    : q_ptr(parent)
    , contentWidget(nullptr)
    , mainLayout(nullptr)
    , bookmarksLayout(nullptr)
{
}

void QnBookmarksOverlayWidgetPrivate::clearLayout() {
    while (bookmarksLayout->count() > 0) {
        QGraphicsLayoutItem *item = bookmarksLayout->itemAt(0);
        bookmarksLayout->removeAt(0);
        delete item;
    }
}

void QnBookmarksOverlayWidgetPrivate::setBookmarks(const QnCameraBookmarkList &bookmarks) {
    if (bookmarksList == bookmarks)
        return;

    bookmarksList = bookmarks;

    clearLayout();

    bookmarksLayout->addStretch();

    for (const QnCameraBookmark &bookmark: bookmarks) {
        QnBookmarkItem *bookmarkItem = new QnBookmarkItem(bookmark);
        bookmarksLayout->addItem(bookmarkItem);
    }

    Q_Q(QnBookmarksOverlayWidget);
    q->update();
}
