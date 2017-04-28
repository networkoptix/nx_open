#pragma once

#include <QtWidgets/QGraphicsWidget>

class QnGraphicsScrollAreaPrivate;
class QnGraphicsScrollArea: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnGraphicsScrollArea(QGraphicsItem* parent = nullptr);
    virtual ~QnGraphicsScrollArea();

    QGraphicsWidget* contentWidget() const;
    void setContentWidget(QGraphicsWidget* widget);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnGraphicsScrollArea)
    const QScopedPointer<QnGraphicsScrollAreaPrivate> d_ptr;
};
