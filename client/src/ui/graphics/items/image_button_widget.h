#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <QGraphicsWidget>
#include <QPixmap>
#include <ui/processors/clickable.h>

/**
 * A lightweight button widget that does not use style for painting. 
 */
class QnImageButtonWidget: public Clickable<QGraphicsWidget> {
    Q_OBJECT;

    typedef Clickable<QGraphicsWidget> base_type;

public:
    enum PixmapFlag {
        DEFAULT = 0,
        CHECKED = 0x1,
        HOVERED = 0x2,
        FLAGS_MAX = 0x3
    };
    Q_DECLARE_FLAGS(PixmapFlags, PixmapFlag);

    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    const QPixmap &pixmap(PixmapFlags flags) const;
    void setPixmap(PixmapFlags flags, const QPixmap &pixmap);

    bool isCheckable() const { return m_checkable; }

    bool isChecked() const { return m_checked; }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

public Q_SLOTS:
    void setCheckable(bool checkable);
    void setChecked(bool checked);
    inline void toggle() { setChecked(!isChecked()); }

Q_SIGNALS:
    void clicked();
    void toggled(bool checked);
    void hoverEntered();
    void hoverLeft();

protected:
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;

    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    bool drawPixmap(QPainter *painter, PixmapFlags flags);
    void setUnderMouse(bool underMouse);

private:
    QPixmap m_pixmaps[FLAGS_MAX + 1];
    bool m_checkable;
    bool m_checked;
    bool m_underMouse;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnImageButtonWidget::PixmapFlags);

#endif // QN_IMAGE_BUTTON_WIDGET_H
