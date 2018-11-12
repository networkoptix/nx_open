#pragma once

#include <QtCore/QScopedPointer>

#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QStackedLayout>

class QnGraphicsStackedWidgetPrivate;

/**
  * A container like QStackedWidget for graphics widgets.
  */
class QnGraphicsStackedWidget: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnGraphicsStackedWidget(QGraphicsItem* parent = nullptr,
        Qt::WindowFlags flags = Qt::WindowFlags());

    virtual ~QnGraphicsStackedWidget();

    /** Add graphics widget to the end of the stack.
      * Takes ownership of the widget.
      * Returns index of the widget in the stack or -1 if eigher
      *   nullptr or a widget already in the stack was passed. */
    int addWidget(QGraphicsWidget* widget);

    /** Insert graphics widget to the stack at specified position.
      * Takes ownership of the widget.
      * Index is first bounded to [0, count()].
      * Returns bounded index or -1 if eigher nullptr
      *   or a widget already in the stack was passed. */
    int insertWidget(int index, QGraphicsWidget* widget);

    /** Remove widget from the stack at specified position.
      * Ownership of the widget is passed to the caller.
      * Returns removed graphics widget or nullptr if index is out of range. */
    QGraphicsWidget* removeWidget(int index);

    /** Remove specified graphics widget from the stack.
      * Ownership of the widget is passed to the caller.
      * Returns index the removed widget had
      *   or -1 if specified widget was not in the stack. */
    int removeWidget(QGraphicsWidget* widget);

    /** Count of graphics widgets in the stack. */
    int count() const;

    /** Index of specified graphics widget in the stack.
      * Returns -1 if specified widget is not in the stack. */
    int indexOf(QGraphicsWidget* widget) const;

    /** Graphics widget at specified position in the stack.
      * Returns nullptr if specified index is out of range. */
    QGraphicsWidget* widgetAt(int index) const;

    /** Alignments of graphics widgets in the stack.
      * Alignments are associated with widgets, not indices,
      *   even though they can be set or queried by index. */
    Qt::Alignment alignment(QGraphicsWidget* widget) const;
    Qt::Alignment alignment(int index) const;
    void setAlignment(QGraphicsWidget* widget, Qt::Alignment alignment);
    void setAlignment(int index, Qt::Alignment alignment);

    /** Index of currently selected graphics widget.
      * Returns -1 if the stack is empty. */
    int currentIndex() const;

    /** Sets currently selected graphics widget by index.
      * Returns selected widget or nullptr if specified index is out of range. */
    QGraphicsWidget* setCurrentIndex(int index);

    /** Currently selected graphics widget.
      * Returns nullptr if the stack is empty. */
    QGraphicsWidget* currentWidget() const;

    /** Sets currently selected graphics widget.
      * Returns its index or -1 if specified widget is not in the stack. */
    int setCurrentWidget(QGraphicsWidget* widget);

    /** Stacking mode (see QStackedLayout documentation).
      *   QStackedLayout::StackOne: current widget is visible, other widgets are hidden.
      *   QStackedLayout::StackAll: all widgets are visible, current widget is on top of Z-order. */
    QStackedLayout::StackingMode stackingMode() const;
    void setStackingMode(QStackedLayout::StackingMode mode);

signals:
    /** Emitted after current widget has been changed. */
    void currentChanged(QGraphicsWidget* newCurrentWidget);

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const override;

private:
    const QScopedPointer<QnGraphicsStackedWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnGraphicsStackedWidget);
};
