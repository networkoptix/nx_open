#ifndef QN_OVERLAYED_H
#define QN_OVERLAYED_H

#include <QtWidgets/QGraphicsItem>
#include <QtCore/QEvent>

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
        // TODO: #Elric Refactoring needed.
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
            InfoLayer,
            TopControlsLayer,
        };

        bool isOverlayVisible() const;
        void setOverlayVisible(bool visible, bool animate);

        void addOverlayWidget(QGraphicsWidget *widget
            , const OverlayParams &params);

        void removeOverlayWidget(QGraphicsWidget *widget);

        OverlayVisibility overlayWidgetVisibility(QGraphicsWidget *widget) const;
        void setOverlayWidgetVisibility(QGraphicsWidget *widget, OverlayVisibility visibility, bool animate);
        void updateOverlayWidgetsVisibility(bool animate);

        static void setOverlayWidgetVisible(QGraphicsWidget* widget, bool visible, bool animate);
        static bool isOverlayWidgetVisible(QGraphicsWidget* widget);
    private:
        template<class Base>
        friend class ::Overlayed;

        void initOverlayed(QGraphicsWidget *widget);

        int overlayWidgetIndex(QGraphicsWidget *widget) const;

        void updateOverlaysRotation();
        void updateOverlayWidgetsGeometry();

    private:
        struct OverlayWidget {
            OverlayVisibility visibility;
            QGraphicsWidget *widget;
            QGraphicsWidget *childWidget;
            QnViewportBoundWidget *boundWidget;
            QnFixedRotationTransform *rotationTransform;

            OverlayWidget();
        };

        QGraphicsWidget* m_widget;

        /** List of overlay widgets. */
        QList<OverlayWidget> m_overlayWidgets;

        bool m_overlayVisible;

        /** Fixed rotation angle in degrees. Used to rotate static text and images. */
        Qn::FixedRotation m_overlayRotation;
    };

    struct OverlayParams
    {
        OverlayedBase::OverlayVisibility visibility;
        bool autoRotate;
        bool bindToViewport;
        qreal z;
        QMarginsF margins;

        OverlayParams(OverlayedBase::OverlayVisibility visibility = OverlayedBase::UserVisible
            , bool autoRotate = false
            , bool bindToViewport = false
            , qreal z = OverlayedBase::BaseLayer
            , const QMarginsF &margins = QMarginsF());
    };

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

