#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <client/client_color_types.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnBookmarkItemPrivate;
class QnBookmarkItem : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY(QnBookmarkColors colors READ colors WRITE setColors)

    typedef QGraphicsWidget base_type;
public:
    QnBookmarkItem(QGraphicsItem *parent = nullptr);
    QnBookmarkItem(const QnCameraBookmark &bookmark, QGraphicsItem *parent = nullptr);
    ~QnBookmarkItem();

    const QnCameraBookmark &bookmark() const;
    void setBookmark(const QnCameraBookmark &bookmark);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const QnBookmarkColors &colors() const;
    void setColors(const QnBookmarkColors &colors);

private:
    Q_DECLARE_PRIVATE(QnBookmarkItem)
    QScopedPointer<QnBookmarkItemPrivate> d_ptr;
};
