#ifndef QN_RESOURCE_WIDGET_H
#define QN_RESOURCE_WIDGET_H

#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>

#include <core/resource/resource_fwd.h>

#include <ui/common/constrained_resizable.h>
#include <ui/common/geometry.h>
#include <ui/common/fixed_rotation.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/common/help_topic_queryable.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/shadow/shaded.h>

class QGraphicsLinearLayout;

class QnViewportBoundWidget;
class QnResourceVideoLayout;
class QnWorkbenchItem;
class QnStatusOverlayWidget;
class QnImageButtonWidget;
class QnImageButtonBar;

class GraphicsLabel;

class QnResourceWidget: public Shaded<Instrumented<GraphicsWidget> >, public QnWorkbenchContextAware, public ConstrainedResizable, public HelpTopicQueryable, protected QnGeometry {
    Q_OBJECT
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity)
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor NOTIFY frameColorChanged)
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement)
    Q_PROPERTY(QRectF enclosingGeometry READ enclosingGeometry WRITE setEnclosingGeometry)
    Q_PROPERTY(qreal enclosingAspectRatio READ enclosingAspectRatio WRITE setEnclosingAspectRatio)
    Q_PROPERTY(bool localActive READ isLocalActive WRITE setLocalActive)
    Q_FLAGS(Options Option)

    typedef Shaded<Instrumented<GraphicsWidget> > base_type;

public:
    enum Option {
        DisplayActivity             = 0x0001,    /**< Whether the paused overlay icon should be displayed. */
        DisplaySelection            = 0x0002,    /**< Whether selected / not selected state should be displayed. */
        DisplayMotion               = 0x0004,    /**< Whether motion is to be displayed. */                              // TODO: #Elric this flag also handles smart search, separate!
        DisplayButtons              = 0x0008,    /**< Whether item buttons are to be displayed. */
        DisplayMotionSensitivity    = 0x0010,    /**< Whether a grid with motion region sensitivity is to be displayed. */
        DisplayCrosshair            = 0x0020,    /**< Whether PTZ crosshair is to be displayed. */
        DisplayInfo                 = 0x0040,    /** Whether info panel is to be displayed. */

        ControlPtz                  = 0x0100,    /**< Whether PTZ state can be controlled with mouse. */
        ControlZoomWindow           = 0x0200,    /**< Whether zoom windows can be created by dragging the mouse. */
        VirtualZoomWindow           = 0x0400,    /**< Whether zoom windows is fisheye shader. */

        WindowRotationForbidden     = 0x1000
    };
    Q_DECLARE_FLAGS(Options, Option)

    enum Button {
        CloseButton                 = 0x1,
        InfoButton                  = 0x2,
        RotateButton                = 0x4
    };
    Q_DECLARE_FLAGS(Buttons, Button)

    // TODO: #Elric Refactoring needed.
    enum OverlayVisibility {
        Invisible,
        Visible,
        AutoVisible,
        UserVisible,
    };

    /**
     * Constructor.
     *
     * \param context                   Context in which this resource widget operates.
     * \param item                      Workbench item that this widget represents.
     * \param parent                    Parent item.
     */
    QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    QnResourcePtr resource() const;

    /**
     * \returns                         Workbench item associated with this widget. Never returns NULL.
     */
    QnWorkbenchItem *item() const;

    /**
     * \returns                         Layout of channels in this widget. Never returns NULL.
     */
    const QnResourceVideoLayout *channelLayout() const {
        return m_channelsLayout;
    }
    
    const QRectF &zoomRect() const;
    void setZoomRect(const QRectF &zoomRect);
    QnResourceWidget *zoomTargetWidget() const;

    /**
     * \returns                         Frame opacity of this widget.
     */
    qreal frameOpacity() const {
        return m_frameOpacity;
    }

    /**
     * \param frameOpacity              New frame opacity for this widget.
     */
    void setFrameOpacity(qreal frameOpacity) {
        m_frameOpacity = frameOpacity;
    }

    /**
     * \returns                         Frame width of this widget.
     */
    qreal frameWidth() const {
        return m_frameWidth;
    }

    /**
     * \param frameWidth                New frame width for this widget.
     */
    void setFrameWidth(qreal frameWidth);

    /**
     * \returns                         Frame color of this widget.
     */
    QColor frameColor() const {
        return m_frameColor;
    }

    /**
     * \param frameColor                New frame color for this widget.
     */
    void setFrameColor(const QColor &frameColor);

    virtual void setGeometry(const QRectF &geometry) override;

    /**
     * \returns                         Aspect ratio of this widget.
     *                                  Negative value will be returned if this
     *                                  widget does not have aspect ratio.
     */
    qreal aspectRatio() const {
        return m_aspectRatio;
    }

    void setAspectRatio(qreal aspectRatio);

    /**
     * \returns                         Whether this widget has an aspect ratio.
     */
    bool hasAspectRatio() const {
        return m_aspectRatio > 0.0;
    }

    /**
     * Every widget is considered to be inscribed into an enclosing rectangle with a
     * fixed aspect ratio. When aspect ratio of the widget itself changes, it is
     * re-inscribed into its enclosed rectangle.
     *
     * \returns                         Aspect ratio of the enclosing rectangle for this widget.
     */
    qreal enclosingAspectRatio() const {
        return m_enclosingAspectRatio;
    }

    /**
     * \param enclosingAspectRatio      New enclosing aspect ratio.
     */
    void setEnclosingAspectRatio(qreal enclosingAspectRatio);

    /**
     * \returns                         Geometry of the enclosing rectangle for this widget.
     */
    QRectF enclosingGeometry() const;

    /**
     * \param enclosingGeometry         Geometry of the enclosing rectangle for this widget.
     */
    void setEnclosingGeometry(const QRectF &enclosingGeometry);

    /**
     * \returns                         Options for this widget.
     */
    Options options() const {
        return m_options;
    }

    /**
     * \param option                    Affected option.
     * \param value                     New value for the affected option.
     */
    void setOption(Option option, bool value = true) {
        setOptions(value ? m_options | option : m_options & ~option);
    }

    /**
     * \param options                   New options for this widget.
     */
    void setOptions(Options options);

    /**
     * \returns                         Status of the last rendering operation.
     */
    Qn::RenderStatus renderStatus() const;

    /**
     * \returns                         Text of this window's title.
     */
    QString titleText() const;

    /**
     * \returns                         Format string of this widget's title text.
     */
    QString titleTextFormat() const;

    /**
     * Sets format of the title text.
     * 
     * If <tt>'\t'</tt> symbol is used in the text, it will be split in two parts 
     * at the position of this symbol, and these parts will be aligned to the sides of the title bar.
     *
     * If <tt>"%1"</tt> placeholder is used, it will be replaced with this widget's
     * default autogenerated text. Note that <tt>"%1"</tt> is the default format.
     *
     * \param titleTextFormat           New title text for this window.
     */
    void setTitleTextFormat(const QString &titleTextFormat);

    /**
     * \returns                         Information text that is displayed in this widget's footer.
     */
    QString infoText();

    /**
     * \returns                         Format string of this widget's information text.
     */
    QString infoTextFormat() const;

    /**
     * \param infoTextFormat            New text to be displayed in this widget's footer. 
     * \see setTitleTextFormat(const QString &)
     */
    void setInfoTextFormat(const QString &infoTextFormat);

    bool isInfoVisible() const;
    Q_SLOT void setInfoVisible(bool visible, bool animate = true);

    Buttons checkedButtons() const;
    void setCheckedButtons(Buttons checkedButtons);

    Buttons visibleButtons() const;

    bool isLocalActive() const;
    void setLocalActive(bool localActive);

    bool isOverlayVisible() const;
    Q_SLOT void setOverlayVisible(bool visible = true, bool animate = true);

    void addOverlayWidget(QGraphicsWidget *widget, OverlayVisibility visibility = UserVisible, bool autoRotate = false, bool bindToViewport = false, bool placeOverControls = false);
    void removeOverlayWidget(QGraphicsWidget *widget);
    OverlayVisibility overlayWidgetVisibility(QGraphicsWidget *widget) const;
    void setOverlayWidgetVisibility(QGraphicsWidget *widget, OverlayVisibility visibility);

    using base_type::mapRectToScene;

signals:
    void painted();
    void aspectRatioChanged();
    void aboutToBeDestroyed();
    void optionsChanged();
    void zoomRectChanged();
    void zoomTargetWidgetChanged();
    void frameColorChanged();
    void rotationStartRequested();
    void rotationStopRequested();

protected:
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override;
    virtual QCursor windowCursorAt(Qn::WindowFrameSection section) const override;
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual bool windowFrameEvent(QEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) = 0;
    virtual void paintChannelForeground(QPainter *painter, int channel, const QRectF &rect);
    
    void paintSelection(QPainter *painter, const QRectF &rect);

    virtual QSizeF constrainedSize(const QSizeF constraint) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    const QSize &channelScreenSize() const;
    void setChannelScreenSize(const QSize &size);
    virtual void channelScreenSizeChangedNotify() {}

    Qn::ResourceStatusOverlay statusOverlay() const;
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay);
    Qn::ResourceStatusOverlay calculateStatusOverlay(int resourceStatus) const;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const;
    Q_SLOT void updateStatusOverlay();

    virtual QString calculateTitleText() const;
    Q_SLOT void updateTitleText();

    virtual QString calculateInfoText() const;
    Q_SLOT void updateInfoText();

    virtual QCursor calculateCursor() const;
    Q_SLOT void updateCursor();

    int overlayWidgetIndex(QGraphicsWidget *widget) const;
    void updateOverlayWidgetsGeometry();
    void updateOverlayWidgetsVisibility(bool animate = true);

    QnImageButtonBar *buttonBar() const {
        return m_buttonBar;
    }

    QnImageButtonWidget *iconButton() const {
        return m_iconButton;
    }

    QnViewportBoundWidget* headerOverlayWidget() const {
        return m_headerOverlayWidget;
    }

    QnStatusOverlayWidget *statusOverlayWidget() const {
        return m_statusOverlayWidget;
    }

    virtual Buttons calculateButtonsVisibility() const;
    Q_SLOT void updateButtonsVisibility();

    void setChannelLayout(const QnResourceVideoLayout *channelLayout);
    virtual void channelLayoutChangedNotify() {}
    
    virtual void optionsChangedNotify(Options changedFlags);

    int channelCount() const;
    QRectF channelRect(int channel) const;
    QRectF exposedRect(int channel, bool accountForViewport = true, bool accountForVisibility = true, bool useRelativeCoordinates = false);

    void ensureAboutToBeDestroyedEmitted();

    QColor activeFrameColor() const;
    QColor selectedFrameColor() const;

private:
    void setTitleTextInternal(const QString &titleText);
    void setInfoTextInternal(const QString &infoText);

    Q_SLOT void at_iconButton_visibleChanged();
    Q_SLOT void at_infoButton_toggled(bool toggled);

    struct OverlayWidget {
        OverlayVisibility visibility;
        QGraphicsWidget *widget;
        QGraphicsWidget *childWidget;
        QnViewportBoundWidget *boundWidget;
        QnFixedRotationTransform *rotationTransform;
    };

private:
    friend class QnWorkbenchDisplay;

    /** Layout item. */
    QPointer<QnWorkbenchItem> m_item;

    /** Resource associated with this widget. */
    QnResourcePtr m_resource;

    /** Options that control display & behavior. */
    Options m_options;

    /** Whether this item is 'locally active'. This affects the color of item's border. */
    bool m_localActive;

    /** Layout of this widget's channels. */
    const QnResourceVideoLayout *m_channelsLayout;

    /** Aspect ratio. Negative value means that aspect ratio is not enforced. */
    qreal m_aspectRatio;

    /** Aspect ratio of the virtual enclosing rectangle. */
    qreal m_enclosingAspectRatio;

    /** Cached size of a single media channel, in screen coordinates. */
    QSize m_channelScreenSize;

    /** Frame opacity. */
    qreal m_frameOpacity;

    /** Frame width. */
    qreal m_frameWidth;

    /** Base frame color */
    QColor m_frameColor;

    QString m_titleTextFormat, m_infoTextFormat;
    bool m_titleTextFormatHasPlaceholder, m_infoTextFormatHasPlaceholder;

    /** List of overlay widgets. */
    QList<OverlayWidget> m_overlayWidgets;
    bool m_overlayVisible;

    /* Widgets for overlaid stuff. */
    QnViewportBoundWidget *m_headerOverlayWidget;
    QGraphicsLinearLayout *m_headerLayout;
    GraphicsWidget *m_headerWidget;
    GraphicsLabel *m_headerLeftLabel;
    GraphicsLabel *m_headerRightLabel;
    QnImageButtonBar *m_buttonBar;
    QnImageButtonWidget *m_iconButton;

    QnViewportBoundWidget *m_footerOverlayWidget;
    GraphicsWidget *m_footerWidget;
    GraphicsLabel *m_footerLeftLabel;
    GraphicsLabel *m_footerRightLabel;

    QnStatusOverlayWidget *m_statusOverlayWidget;


    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Whether mouse cursor is in widget. Usable to show/hide decorations. */
    bool m_mouseInWidget;

    /** Fixed rotation angle in degrees. Used to rotate static text and images. */
    Qn::FixedRotation m_overlayRotation;

    QRectF m_zoomRect;

    /** Current overlay. */
    Qn::ResourceStatusOverlay m_statusOverlay;

    Qn::RenderStatus m_renderStatus;

    qint64 m_lastNewFrameTimeMSec;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Options)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Buttons)
Q_DECLARE_METATYPE(QnResourceWidget *)

#endif // QN_RESOURCE_WIDGET_H
