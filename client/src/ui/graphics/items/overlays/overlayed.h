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
    class OverlayedBase {
    public:
        // TODO: #Elric Refactoring needed.
        enum OverlayVisibility {
            Invisible,
            Visible,
            AutoVisible,
            UserVisible,
        };

        bool isOverlayVisible() const;
        void setOverlayVisible(bool visible = true, bool animate = true);

        void addOverlayWidget(QGraphicsWidget *widget, 
            OverlayVisibility visibility = UserVisible,
            bool autoRotate = false, 
            bool bindToViewport = false, 
            bool placeOverControls = false,
            bool isControlsWidget = false);
        void removeOverlayWidget(QGraphicsWidget *widget);

        OverlayVisibility overlayWidgetVisibility(QGraphicsWidget *widget) const;
        void setOverlayWidgetVisibility(QGraphicsWidget *widget, OverlayVisibility visibility);
        void updateOverlayWidgetsVisibility(bool animate = true);
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
        };

        QGraphicsWidget* m_widget;

        /** List of overlay widgets. */
        QList<OverlayWidget> m_overlayWidgets;

        bool m_overlayVisible;

        /** Fixed rotation angle in degrees. Used to rotate static text and images. */
        Qn::FixedRotation m_overlayRotation;

        QGraphicsWidget* m_controlsWidget;
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

