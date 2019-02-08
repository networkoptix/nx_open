#pragma once

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

#include <ui/common/indents.h>

namespace nx::vms::client::desktop {

class WidgetTableDelegate: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(WidgetTableDelegate)
    using base_type = QObject;

public:
    WidgetTableDelegate(QObject* parent = nullptr);

    /** Creates a widget for specified model index. */
    virtual QWidget* createWidget(QAbstractItemModel* model,
        const QModelIndex& index, QWidget* parent) const;

    /** Attempts to update widget with data from specified model index.
     * Returns true if successful or false if a new widget must be created.
     */
    virtual bool updateWidget(QWidget* widget, const QModelIndex& index) const;

    /** Margins for specified widget or model index. */
    virtual QnIndents itemIndents(QWidget* widget, const QModelIndex& index) const;

    /** Size hint for specified widget or model index. */
    virtual QSize sizeHint(QWidget* widget, const QModelIndex& index) const;

    /** Model index for specified widget. */
    static QModelIndex indexForWidget(QWidget* widget);
};

} // namespace nx::vms::client::desktop
