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
    enum PixmapRole {
        BACKGROUND_ROLE,
        HOVERED_ROLE,
        CHECKED_ROLE,
        ROLE_COUNT
    };

    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    const QPixmap &pixmap(PixmapRole role) const;
    void setPixmap(PixmapRole role, const QPixmap &pixmap);

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
    void drawPixmap(QPainter *painter, PixmapRole role);
    bool hasPixmap(PixmapRole role);
    void setUnderMouse(bool underMouse);

private:
    QPixmap m_pixmaps[ROLE_COUNT];
    bool m_checkable;
    bool m_checked;
    bool m_underMouse;
};


#endif // QN_IMAGE_BUTTON_WIDGET_H
