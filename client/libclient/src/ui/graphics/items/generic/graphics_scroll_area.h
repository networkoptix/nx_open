#pragma once

#include <QtWidgets/QGraphicsWidget>

class QnGraphicsScrollAreaPrivate;
class QnGraphicsScrollArea : public QGraphicsWidget {
    Q_OBJECT

    typedef QGraphicsWidget base_type;

public:
    QnGraphicsScrollArea(QGraphicsItem *parent = nullptr);
    ~QnGraphicsScrollArea();

    QGraphicsWidget *contentWidget() const;
    void setContentWidget(QGraphicsWidget *widget);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

private:
    Q_DECLARE_PRIVATE(QnGraphicsScrollArea)
    QScopedPointer<QnGraphicsScrollAreaPrivate> d_ptr;
};
