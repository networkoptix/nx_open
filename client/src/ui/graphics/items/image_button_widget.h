#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <QGraphicsWidget>
#include <QPixmap>
#include <ui/processors/clickable.h>

class QnImageButtonWidget: public Clickable<QGraphicsWidget> {
    Q_OBJECT;

    typedef Clickable<QGraphicsWidget> base_type;

public:
    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    const QPixmap &pixmap() const { return m_pixmap; }
    void setPixmap(const QPixmap &pixmap);

    const QPixmap &checkedPixmap() const { return m_checkedPixmap; }
    void setCheckedPixmap(const QPixmap &pixmap);

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

protected:
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;

    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    QPixmap m_pixmap;
    QPixmap m_checkedPixmap;
    bool m_checkable;
    bool m_checked;
};


#endif // QN_IMAGE_BUTTON_WIDGET_H
