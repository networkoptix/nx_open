#pragma once

#include <QtCore/QList>

#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QStackedLayout>

class QnGraphicsStackedWidget;
class QnGraphicsStackedWidgetPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(QnGraphicsStackedWidget);
    QnGraphicsStackedWidget* const q_ptr;

public:
    QnGraphicsStackedWidgetPrivate(QnGraphicsStackedWidget* main);
    virtual ~QnGraphicsStackedWidgetPrivate();

    int insertWidget(int index, QGraphicsWidget* widget);
    QGraphicsWidget* removeWidget(int index);
    int removeWidget(QGraphicsWidget* widget);

    int count() const;
    int indexOf(QGraphicsWidget* widget) const;
    QGraphicsWidget* widgetAt(int index) const;

    Qt::Alignment alignment(QGraphicsWidget* widget) const;
    void setAlignment(QGraphicsWidget* widget, Qt::Alignment alignment);

    int currentIndex() const;
    QGraphicsWidget* setCurrentIndex(int index);
    QGraphicsWidget* currentWidget() const;
    int setCurrentWidget(QGraphicsWidget* widget);

    QStackedLayout::StackingMode stackingMode() const;
    void setStackingMode(QStackedLayout::StackingMode mode);

    QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint) const;

private:
    void updateVisibility();
    void updateGeometries();

    void setWidgetGeometry(QGraphicsWidget* widget, const QRectF& geometry);

private:
    QList<QGraphicsWidget*> m_widgets;
    QHash<QGraphicsWidget*, Qt::Alignment> m_alignments;

    int m_currentIndex = -1;
    QStackedLayout::StackingMode m_stackingMode = QStackedLayout::StackOne;
};
