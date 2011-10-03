#ifndef ANIMATEDWIDGET_H
#define ANIMATEDWIDGET_H

#include <QtGui/QGraphicsWidget>

class AnimatedWidgetPrivate;
class AnimatedWidget : public QGraphicsWidget
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QPointF instantPos READ instantPos WRITE setInstantPos DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, QSizeF instantSize READ instantSize WRITE setInstantSize DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, qreal instantScale READ instantScale WRITE setInstantScale DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, qreal instantRotation READ instantRotation WRITE setInstantRotation DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, qreal instantOpacity READ instantOpacity WRITE setInstantOpacity DESIGNABLE false SCRIPTABLE false FINAL)

public:
    enum QGraphicsWidgetChange {
        ItemSizeChange = ItemPositionChange + 127,
        ItemSizeHasChanged
    };

    AnimatedWidget(QGraphicsItem *parent = 0);
    ~AnimatedWidget();

    bool isInteractive() const;
    void setInteractive(bool interactive);

/*
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
    QRectF boundingRect() const;
    QPainterPath shape() const;
*/
    QPainterPath opaqueArea() const;

    void setGeometry(const QRectF &rect);

Q_SIGNALS:
    void clicked();
    void doubleClicked();

protected:
/*    virtual void initStyleOption(QStyleOption *option) const;

    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;*/
    void updateGeometry();

    // Notification
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);
//    virtual QVariant propertyChange(const QString &propertyName, const QVariant &value);

    // Scene events
//    bool sceneEvent(QEvent *event);
    bool windowFrameEvent(QEvent *event);
//    Qt::WindowFrameSection windowFrameSectionAt(const QPointF& pos) const;
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);

/*    // Base event handlers
    bool event(QEvent *event);
    //virtual void actionEvent(QActionEvent *event);
    virtual void changeEvent(QEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void moveEvent(QGraphicsSceneMoveEvent *event);
    virtual void polishEvent();
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event);
    virtual void showEvent(QShowEvent *event);
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    virtual void grabMouseEvent(QEvent *event);
    virtual void ungrabMouseEvent(QEvent *event);
    virtual void grabKeyboardEvent(QEvent *event);
    virtual void ungrabKeyboardEvent(QEvent *event);
*/
private:
    Q_DISABLE_COPY(AnimatedWidget)
    AnimatedWidgetPrivate *const d;
};

#endif // ANIMATEDWIDGET_H
