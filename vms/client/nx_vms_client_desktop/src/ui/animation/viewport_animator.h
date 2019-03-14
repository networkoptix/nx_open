#pragma once

#include <QtCore/QObject>
#include <QtCore/QMargins>
#include <QtCore/QRect>
#include <client/client_globals.h>
#include "rect_animator.h"
#include "viewport_geometry_accessor.h"

class QGraphicsView;
class QMargins;

class ViewportAnimator: public RectAnimator {
    Q_OBJECT

    typedef RectAnimator base_type;

public:
    /**
     * Constructor.
     *
     * \param view                      View that this viewport animator will be assigned to.
     * \param parent                    Parent object.
     */
    ViewportAnimator(QObject* parent = nullptr);

    virtual ~ViewportAnimator() override;

    /**
     * \returns                         View that this viewport animator is assigned to.
     */
    QGraphicsView *view() const;

    void setView(QGraphicsView *view);

    QMargins viewportMargins(Qn::MarginTypes marginTypes = Qn::CombinedMargins) const;

    void setViewportMargins(const QMargins& margins, Qn::MarginType marginType);

    Qn::MarginFlags marginFlags() const;

    void setMarginFlags(Qn::MarginFlags marginFlags);

    /**
     * \param rect                      Target viewport rectangle.
     */
    void setTargetRect(const QRectF &rect);

    QRectF targetRect() const;

    /**
     * Starts animated move of a viewport to the given rect, taking margins and margin flags into account.
     * When animation finishes, viewport's bounding rect will include the given rect.
     *
     * Note that this function animates position and scale only. It does not
     * take rotation and more complex transformations into account.
     *
     * \param rect                      Rectangle to move adjusted viewport to,
     *                                  in scene coordinates.
     * \param animate                   Whether transition should be animated.
     */
    void moveTo(const QRectF &rect, bool animate);

    QRectF adjustedToReal(const QRectF &adjustedRect) const;

protected:
    virtual int estimatedDuration(const QVariant &from, const QVariant &to) const override;

    virtual void updateTargetValue(const QVariant &newTargetValue) override;

    QRectF realToAdjusted(const QGraphicsView *view, const QRectF &realRect) const;
    QRectF adjustedToReal(const QGraphicsView *view, const QRectF &adjustedRect) const;

private:
    /** Accessor for viewport rect. */
    ViewportGeometryAccessor *m_accessor;

    QMargins m_panelMargins;
    QMargins m_layoutMargins;

    Qn::MarginFlags m_marginFlags;

    QRectF m_adjustedTargetRect;
    bool m_adjustedTargetRectValid;
};
