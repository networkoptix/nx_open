// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

class QAbstractItemView;
class QLabel;

namespace nx::vms::client::desktop {

/**
 * An utility widget that takes ownership of a specified item view,
 * makes it visible if it is not empty or hides it if it is empty
 * and shows a text message instead.
 */
class ItemViewAutoHider: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemView* view READ view WRITE setView)
    Q_PROPERTY(QString emptyViewMessage READ emptyViewMessage WRITE setEmptyViewMessage)
    using base_type = QWidget;

public:
    ItemViewAutoHider(QWidget* parent = nullptr);
    virtual ~ItemViewAutoHider();

    /** An item view controlled by this widget. */
    QAbstractItemView* view() const;

    /** Sets a new controlled view and takes its ownership.
     * If there was a controlled view already, its ownership is passed to the caller.
     * If a new model object is set to a controlled view via QAbstractItemView::setModel(),
     * ItemViewAutoHider::setView() must be called again with the same view.
     * Returns a pointer to the view which ownership is passed to the caller.
     */
    QAbstractItemView* setView(QAbstractItemView* view);

    /** A text message shown when the view is empty. */
    QString emptyViewMessage() const;
    void setEmptyViewMessage(const QString& message);

    /** A label widget displaying text message when the view is empty. */
    QLabel* emptyViewMessageLabel() const;

    /** Whether the view is currently hidden. */
    bool isViewHidden() const;

    /** Creates and configures a new auto-hider
     * for specified view with specified message.
     * Replaces the view with the auto-hider in the
     * parent layout, if one exists.
     */
    static ItemViewAutoHider* create(QAbstractItemView* view,
        const QString& message = QString());

signals:
    /** Emitted when the view visibility changes (in this widget's context). */
    void viewVisibilityChanged(bool hidden);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
