#include "bookmarks_overlay_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <core/resource/camera_bookmark.h>

#include <ui/graphics/items/controls/bookmark_item.h>
#include <ui/graphics/items/generic/graphics_scroll_area.h>

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
    QnGraphicsScrollArea *scrollArea;
    QGraphicsLinearLayout *mainLayout;
    QList<QnBookmarkItem*> bookmarkItems;

    void setBookmarks(const QnCameraBookmarkList &bookmarks);
};


QnBookmarksOverlayWidget::QnBookmarksOverlayWidget(QGraphicsWidget *parent)
    : GraphicsWidget(parent)
    , d_ptr(new QnBookmarksOverlayWidgetPrivate(this))
{
    Q_D(QnBookmarksOverlayWidget);

    setFlag(QGraphicsItem::ItemClipsChildrenToShape);
    setAcceptedMouseButtons(0);

    d->contentWidget = new QGraphicsWidget(this);

    d->scrollArea = new QnGraphicsScrollArea(this);
    d->scrollArea->setContentWidget(d->contentWidget);
    d->scrollArea->setMinimumWidth(maximumBookmarkWidth);
    d->scrollArea->setMaximumWidth(maximumBookmarkWidth);
    d->scrollArea->setAlignment(Qt::AlignBottom | Qt::AlignRight);

    d->mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    d->mainLayout->addStretch();
    d->mainLayout->addItem(d->scrollArea);
    setLayout(d->mainLayout);

    setContentsMargins(4, 20, 4, 20);
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
    , scrollArea(nullptr)
    , mainLayout(nullptr)
{
}

void QnBookmarksOverlayWidgetPrivate::setBookmarks(const QnCameraBookmarkList &bookmarks) {
    if (bookmarksList == bookmarks)
        return;

    qDeleteAll(bookmarkItems);
    bookmarkItems.clear();

    bookmarksList = bookmarks;

    int height = 0;
    for (const QnCameraBookmark &bookmark: bookmarks) {
        QnBookmarkItem *bookmarkItem = new QnBookmarkItem(bookmark, contentWidget);
        bookmarkItem->setPos(0, height);
        height += bookmarkItem->size().height() + layoutSpacing;
        bookmarkItems.append(bookmarkItem);
    }

    contentWidget->resize(maximumBookmarkWidth, std::max(0, height - layoutSpacing));

    Q_Q(QnBookmarksOverlayWidget);
    q->update();
}
