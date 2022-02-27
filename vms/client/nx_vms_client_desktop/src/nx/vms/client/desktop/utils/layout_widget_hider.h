// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/Qt>

class QGridLayout;
class QWidget;

namespace nx::vms::client::desktop {

/**
 * Allows to avoid undesired spacing in QLayout after selective widget hiding.
 * Hiding widgets presented in QLayout does not remove them from QLayout effectively, so if
 * QLayout::spacing() is not 0, undesired spacing will "appear" at sides of hidden widget. That
 * is especially bad when widgets must be aligned tight to left or right of parent widget and
 * leftmost or rightmost one was hidden later.
 * Upon hiding and showing, the class distributes visible widgets among available layout columns
 * all at left or right depending on alignment specified.
 * Currently, the class supports horizontal alignment and QGridLayout only. The functionality can
 * be expanded on demand by defining more constructors and improving setVisible().
 */
class LayoutWidgetHider
{
public:
    /**
     * Defines group of successive aligned widgets which can be hidden later.
     * @param gridLayout Should contain proper QSpacerItem to provide the same alignment as
     *     corresponding argument specifies. Removing and adding items after this constructor
     *     call at the same row as widgets have would introduce undefined behavior.
     * @param widgets Must already be added into gridLayout at the same row, at successive
     *     columns, without spanning. The order in the list does not matter.
     * @param alignment Qt::AlignLeft and Qt::AlignRight are allowed only.
     */
    LayoutWidgetHider(
        QGridLayout* gridLayout, QList<QWidget*> widgets, Qt::AlignmentFlag alignment);

    /**
     * Calling QWidget::setVisible() for widgets assigned to this class besides this method is
     *     not recommended, though any non-redundant call for LayoutWidgetHider::setVisible()
     *     after that will re-synchronize widget columns correctly.
     */
    void setVisible(QWidget* widget, bool visible);

    void hide(QWidget* widget);
    void show(QWidget* widget);

private:
    QGridLayout* m_gridLayout = nullptr;
    QMap<int, QWidget*> m_widgets; //< Key is initial widget column number in layout.
    Qt::AlignmentFlag m_alignment;
    bool m_valid = false;
    int m_row = -1;
};

} // namespace nx::vms::client::desktop
