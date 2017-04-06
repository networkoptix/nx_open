#pragma once

#include <QtCore/QScopedPointer>

#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QStackedLayout>

class QnGraphicsStackedWidgetPrivate;
class QnGraphicsStackedWidget: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnGraphicsStackedWidget(QGraphicsItem* parent = nullptr,
        Qt::WindowFlags flags = Qt::WindowFlags());

    virtual ~QnGraphicsStackedWidget();

    int addWidget(QGraphicsWidget* widget);
    int insertWidget(int index, QGraphicsWidget* widget);

    QGraphicsWidget* removeWidget(int index);
    int removeWidget(QGraphicsWidget* widget);

    int count() const;
    int indexOf(QGraphicsWidget* widget) const;
    QGraphicsWidget* widgetAt(int index) const;

    Qt::Alignment alignment(QGraphicsWidget* widget) const;
    Qt::Alignment alignment(int index) const;
    void setAlignment(QGraphicsWidget* widget, Qt::Alignment alignment);
    void setAlignment(int index, Qt::Alignment alignment);

    int currentIndex() const;
    QGraphicsWidget* setCurrentIndex(int index);
    QGraphicsWidget* currentWidget() const;
    int setCurrentWidget(QGraphicsWidget* widget);

    QStackedLayout::StackingMode stackingMode() const;
    void setStackingMode(QStackedLayout::StackingMode mode);

signals:
    void currentChanged(QGraphicsWidget* newCurrentWidget);

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const override;

private:
    const QScopedPointer<QnGraphicsStackedWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnGraphicsStackedWidget);
};
