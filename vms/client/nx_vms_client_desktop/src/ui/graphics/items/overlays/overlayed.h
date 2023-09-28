// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_OVERLAYED_H
#define QN_OVERLAYED_H

#include <QtCore/QEvent>
#include <QtWidgets/QGraphicsItem>

#include <ui/common/fixed_rotation.h>
#include <utils/common/forward.h>

template<class Base>
class Overlayed;

class QnViewportBoundWidget;

namespace detail {

struct OverlayParams;

class OverlayedBase
{
public:
    // TODO: #sivanov Refactoring needed.
    enum OverlayVisibility
    {
        Invisible,
        Visible,
        AutoVisible,
        UserVisible,
    };

    enum OverlayLayer
    {
        BaseLayer = 0,
        StatusLayer,
        SelectionLayer,
        RewindLayer,
        InfoLayer,
        TopControlsLayer,
    };

    enum class OverlayFlag
    {
        none = 0,
        autoRotate = 0x01,
        bindToViewport = 0x02
    };
    Q_DECLARE_FLAGS(OverlayFlags, OverlayFlag)

    bool isOverlayVisible() const;
    void setOverlayVisible(bool visible, bool animate);

    void addOverlayWidget(QGraphicsWidget* widget, const OverlayParams& params);

    void removeOverlayWidget(QGraphicsWidget* widget);

    OverlayVisibility overlayWidgetVisibility(QGraphicsWidget* widget) const;
    void setOverlayWidgetVisibility(
        QGraphicsWidget* widget,
        OverlayVisibility visibility,
        bool animate);
    void updateOverlayWidgetsVisibility(bool animate);

    void updateOverlayWidgetMargins(const QGraphicsWidget* widget, const QMargins& margins);

    static void setOverlayWidgetVisible(QGraphicsWidget* widget, bool visible, bool animate);
    static bool isOverlayWidgetVisible(QGraphicsWidget* widget);

private:
    template<class Base>
    friend class ::Overlayed;

    void initOverlayed(QGraphicsWidget* widget);

    int overlayWidgetIndex(QGraphicsWidget* widget) const;

    void updateOverlaysRotation();
    void updateOverlayWidgetsGeometry();

private:
    struct OverlayWidget
    {
        OverlayVisibility visibility;
        QGraphicsWidget* widget;
        QGraphicsWidget* childWidget;
        QnViewportBoundWidget* boundWidget;
        QnFixedRotationTransform* rotationTransform;

        OverlayWidget();
    };

    QGraphicsWidget* m_widget;

    /** List of overlay widgets. */
    QList<OverlayWidget> m_overlayWidgets;

    bool m_overlayVisible;

    /** Fixed rotation angle in degrees. Used to rotate static text and images. */
    nx::vms::client::core::Rotation m_overlayRotation;
};

struct OverlayParams
{
    OverlayedBase::OverlayVisibility visibility;
    OverlayedBase::OverlayFlags flags;
    qreal z;
    QMarginsF margins;

    OverlayParams(
        OverlayedBase::OverlayVisibility visibility = OverlayedBase::UserVisible,
        OverlayedBase::OverlayFlags = OverlayedBase::OverlayFlag::none,
        qreal z = OverlayedBase::BaseLayer,
        const QMarginsF &margins = QMarginsF());
};

Q_DECLARE_OPERATORS_FOR_FLAGS(OverlayedBase::OverlayFlags);

} // namespace detail


template<class Base>
class Overlayed: public Base, public detail::OverlayedBase {
    typedef Base base_type;

public:
    QN_FORWARD_CONSTRUCTOR(Overlayed, Base, { initOverlayed(this); });

    virtual ~Overlayed() { }
protected:
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        if (change == QGraphicsItem::ItemRotationHasChanged) {
            updateOverlaysRotation();
        }
        return base_type::itemChange(change, value);
    }

    virtual void setGeometry(const QRectF &geometry) override {
        base_type::setGeometry(geometry);
        updateOverlayWidgetsGeometry();
    }
};


#endif // QN_OVERLAYED_H
