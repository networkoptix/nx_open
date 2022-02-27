// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

class QAbstractScrollArea;
class QAbstractItemView;
class QPropertyAnimation;

namespace nx::vms::client::desktop {

/**
 * Provides functionality similar to the QAbstractItemView::autoScroll() does: automatically
 * scrolls the contents of the view if the user drags near the viewport edge for the case when
 * an item view is placed into another scroll area, either alone or as part of widgets composition.
 * Stock implementation don't work in that case.
 * @note In theory, should work for plain item view too, provided pointer to the scroll area may
 *     be the same as pointer to the item view, but it not tested yet. It makes sense since stock
 *     implementation is quite clumsy.
 * @todo Currently it assists with vertical scrolling only, it would be nice if component could
 *     cope with horizontal scrolling as well. Will be implemented bit later.
 */
class ItemViewDragAndDropScrollAssist: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:

    /**
     * Sets up the scroll assistance during drag and drop interaction for given scroll area and
     * item view.
     * @param scrollArea Valid pointer to the scroll area. Nothing will be changed if scroll
     *     assistance was already set up for that scroll area before.
     * @param itemView Valid pointer to the item view, it should be child of provided scroll area,
     *     not necessarily direct. Or it may be the same pointer as scrollArea, thus autoScroll
     *     functionality of item view will be overridden.
     */
    static void setupScrollAssist(QAbstractScrollArea* scrollArea, QAbstractItemView* itemView);

    /**
     * Removes scroll assistance from the given scroll area if such was set up earlier,
     * does nothing if not.
     */
    static void removeScrollAssist(QAbstractScrollArea* scrollArea);

    virtual ~ItemViewDragAndDropScrollAssist() override;
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ItemViewDragAndDropScrollAssist(QAbstractScrollArea* scrollArea, QAbstractItemView* itemView);
    bool isScrolling() const;
    void stopScrolling() const;

    void updateScrollingStateAndParameters();
    void initializeScrolling(bool isForwardScroll, double edgeProximity);
    void updateScrollingVelocity(double edgeProximity) const;

private:
    QAbstractScrollArea* m_scrollArea;
    QPointer<QPropertyAnimation> m_animation;
};

/**
 * A helper to calculate auto-scroll velocity based on edge proximity.
 */
class ProximityScrollHelper: public QObject
{
    Q_OBJECT
public:
    /** Signed velocity in pixels per millisecond. */
    Q_INVOKABLE qreal velocity(const QRectF& geometry, const QPointF& position) const;
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
