#pragma once

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

#include <ui/common/indents.h>

class QnWidgetTableDelegate: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QnWidgetTableDelegate)
    using base_type = QObject;

public:
    QnWidgetTableDelegate(QObject* parent = nullptr);

    /** Create a widget for specified model index. */
    virtual QWidget* createWidget(QAbstractItemModel* model,
        const QModelIndex& index, QWidget* parent) const;

    /** Attempt to update widget with data from specified model index.
      * Return true if successful or false if a new widget must be created. */
    virtual bool updateWidget(QWidget* widget, const QModelIndex& index) const;

    /** Margins for specified widget or model index. */
    virtual QnIndents itemIndents(QWidget* widget, const QModelIndex& index) const;

    /** Size hint for specified widget or model index. */
    virtual QSize sizeHint(QWidget* widget, const QModelIndex& index) const;

    /** Model index for specified widget. */
    static QModelIndex indexForWidget(QWidget* widget);
};
