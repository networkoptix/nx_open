#pragma once

#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>

#include <client/client_color_types.h>

#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include <ui/common/constrained_resizable.h>
#include <ui/common/geometry.h>
#include <ui/common/fixed_rotation.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/common/help_topic_queryable.h>
#include <ui/animation/animated.h>
#include <ui/graphics/items/overlays/overlayed.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QGraphicsLinearLayout;

class QnViewportBoundWidget;
class QnResourceVideoLayout;
class QnWorkbenchItem;
class QnStatusOverlayWidget;
class QnImageButtonWidget;
class QnImageButtonBar;
class QnProxyLabel;
class QnHtmlTextItem;

class GraphicsLabel;

class QnResourceWidget: public Overlayed<Animated<Instrumented<Connective<GraphicsWidget>>>>, public QnWorkbenchContextAware, public ConstrainedResizable, public HelpTopicQueryable, protected QnGeometry {
    Q_OBJECT
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity)
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth)
    Q_PROPERTY(QnResourceWidgetFrameColors frameColors READ frameColors WRITE setFrameColors)
    Q_PROPERTY(QColor frameDistinctionColor READ frameDistinctionColor WRITE setFrameDistinctionColor NOTIFY frameDistinctionColorChanged)
    Q_PROPERTY(bool localActive READ isLocalActive WRITE setLocalActive)
    Q_FLAGS(Options Option)

    typedef Overlayed<Animated<Instrumented<Connective<GraphicsWidget>>>> base_type;

public:
    enum Option {
        DisplayActivity             = 0x00001,   /**< Whether the paused overlay icon should be displayed. */
        DisplaySelection            = 0x00002,   /**< Whether selected / not selected state should be displayed. */
        DisplayMotion               = 0x00004,   /**< Whether motion is to be displayed. */                              // TODO: #Elric this flag also handles smart search, separate!
        //DisplayButtons              = 0x0008,   /**< Whether item buttons are to be displayed. */ supressed by InfoOverlaysForbidden
        DisplayMotionSensitivity    = 0x00010,   /**< Whether a grid with motion region sensitivity is to be displayed. */
        DisplayCrosshair            = 0x00020,   /**< Whether PTZ crosshair is to be displayed. */
        DisplayInfo                 = 0x00040,   /**< Whether info panel is to be displayed. */
        DisplayDewarped             = 0x00080,   /**< Whether the video is to be dewarped. */

        ControlPtz                  = 0x00100,   /**< Whether PTZ state can be controlled with mouse. */
        ControlZoomWindow           = 0x00200,   /**< Whether zoom windows can be created by dragging the mouse. */

        WindowRotationForbidden     = 0x01000,
        SyncPlayForbidden           = 0x02000,   /**< Whether SyncPlay is forbidden for this widget. */
        InfoOverlaysForbidden       = 0x04000,

        FullScreenMode              = 0x08000,
        ActivityPresence            = 0x10000
    };
    Q_DECLARE_FLAGS(Options, Option)

    enum Button {
        CloseButton                 = 0x1,
        InfoButton                  = 0x2,
        RotateButton                = 0x4
    };
    Q_DECLARE_FLAGS(Buttons, Button)

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

    void setBookmarksLabelText(const QString &text);

    /**
     * \returns                         Resource associated with this widget.
     */
    const QnResourcePtr &resource() const;

    /**
     * \returns                         Workbench item associated with this widget. Never returns NULL.
     */
    QnWorkbenchItem *item() const;

    /**
     * \returns                         Layout of channels in this widget. Never returns NULL.
     */
    QnConstResourceVideoLayoutPtr channelLayout() const {
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

    QColor frameDistinctionColor() const;
    void setFrameDistinctionColor(const QColor &frameColor);

    const QnResourceWidgetFrameColors &frameColors() const;
    void setFrameColors(const QnResourceWidgetFrameColors &frameColors);

    /**
     * \returns                         Aspect ratio of this widget.
     *                                  Negative value will be returned if this
     *                                  widget does not have aspect ratio.
     */
    float aspectRatio() const {
        return m_aspectRatio;
    }

    void setAspectRatio(float aspectRatio);

    /**
     * \returns                         Whether this widget has an aspect ratio.
     */
    bool hasAspectRatio() const {
        return m_aspectRatio > 0.0;
    }

    /**
     * \returns                         Aspect ratio of this widget taking its rotation into account.
     */
    virtual float visualAspectRatio() const;

    /**
     * \return                          Default visual aspect ratio for widgets of this type.
     *                                  Visual aspect ratio differs from aspect ratio in that it is always valid.
     */
    virtual float defaultVisualAspectRatio() const;

    /**
     * \return                          Aspect ratio of one channel.
     */
    float visualChannelAspectRatio() const;

    /**
     * \returns                         Geometry of the enclosing rectangle for this widget.
     */
    QRectF enclosingGeometry() const;

    /**
     * Every widget is considered to be inscribed into an enclosing rectangle.
     * Item will be inscribed even if it is rotated.
     * \param enclosingGeometry         Geometry of the enclosing rectangle for this widget.
     */
    void setEnclosingGeometry(const QRectF &enclosingGeometry, bool updateGeometry = true);

    /**
     * Calculate real item geometry according to the specified enclosing geometry.
     * \see setEnclosingGeometry
     */
    QRectF calculateGeometry(const QRectF &enclosingGeometry, qreal rotation) const;
    QRectF calculateGeometry(const QRectF &enclosingGeometry) const;

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

    bool isInfoVisible() const;
    Q_SLOT void setInfoVisible(bool visible, bool animate = true);

    Buttons checkedButtons() const;
    void setCheckedButtons(Buttons checkedButtons);

    Buttons visibleButtons() const;

    bool isLocalActive() const;
    void setLocalActive(bool localActive);

    using base_type::mapRectToScene;

signals:
    void painted();
    void aspectRatioChanged();
    void aboutToBeDestroyed();
    void optionsChanged();
    void zoomRectChanged();
    void zoomTargetWidgetChanged();
    void frameDistinctionColorChanged();
    void rotationStartRequested();
    void rotationStopRequested();
    void displayInfoChanged();

protected:
    virtual QCursor windowCursorAt(Qn::WindowFrameSection section) const override;
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual bool windowFrameEvent(QEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) = 0;
    virtual void paintChannelForeground(QPainter *painter, int channel, const QRectF &rect);

    void paintSelection(QPainter *painter, const QRectF &rect);

    virtual QSizeF constrainedSize(const QSizeF constraint, Qt::WindowFrameSection pinSection) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    const QSize &channelScreenSize() const;
    void setChannelScreenSize(const QSize &size);
    virtual void channelScreenSizeChangedNotify() {}

    virtual void updateHud(bool animate = true);

    virtual bool isHovered() const;

    Qn::ResourceStatusOverlay statusOverlay() const;
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay, bool animate = true);
    Qn::ResourceStatusOverlay calculateStatusOverlay(int resourceStatus, bool hasVideo) const;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const;
    Q_SLOT void updateStatusOverlay();

    virtual QString calculateTitleText() const;
    Q_SLOT void updateTitleText();

    virtual QString calculateDetailsText() const;
    void updateDetailsText();

    virtual QString calculatePositionText() const;
    void updatePositionText();

    void updateInfoText();

    virtual QCursor calculateCursor() const;
    Q_SLOT void updateCursor();

    QnImageButtonBar *buttonBar() const {
        return m_buttonBar;
    }

    QnImageButtonWidget *iconButton() const {
        return m_iconButton;
    }

    QnStatusOverlayWidget *statusOverlayWidget() const {
        return m_statusOverlayWidget;
    }

    virtual Buttons calculateButtonsVisibility() const;
    Q_SLOT void updateButtonsVisibility();

    void setChannelLayout(QnConstResourceVideoLayoutPtr channelLayout);
    virtual void channelLayoutChangedNotify() {}

    virtual void optionsChangedNotify(Options changedFlags);

    int channelCount() const;
    QRectF channelRect(int channel) const;
    QRectF exposedRect(int channel, bool accountForViewport = true, bool accountForVisibility = true, bool useRelativeCoordinates = false);

    void ensureAboutToBeDestroyedEmitted();

    Q_SLOT virtual void at_itemDataChanged(int role);

    float defaultAspectRatio() const;
private:
    void createButtons();
    void createHeaderOverlay();
    void createFooterOverlay();

    void setTitleTextInternal(const QString &titleText);

    void addInfoOverlay();
    void addMainOverlay();

    void setupIconButton(QGraphicsLinearLayout *layout
        , QnImageButtonWidget *button);

    void insertIconButtonCopy(QGraphicsLinearLayout *layout);

    Q_SLOT void updateCheckedButtons();

    Q_SLOT void at_infoButton_toggled(bool toggled);

    Q_SLOT void at_buttonBar_checkedButtonsChanged();
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
    QnConstResourceVideoLayoutPtr m_channelsLayout;

    /** Aspect ratio. Negative value means that aspect ratio is not enforced. */
    float m_aspectRatio;

    /** Virtual enclosing rectangle. */
    QRectF m_enclosingGeometry;

    /** Cached size of a single media channel, in screen coordinates. */
    QSize m_channelScreenSize;

    /** Frame opacity. */
    qreal m_frameOpacity;

    /** Frame width. */
    qreal m_frameWidth;

    /** Base frame color */
    QColor m_frameDistinctionColor;

    QnResourceWidgetFrameColors m_frameColors;

    QString m_titleTextFormat;
    bool m_titleTextFormatHasPlaceholder;

    /* Widgets for overlaid stuff. */

    QnImageButtonBar *m_buttonBar;
    QnImageButtonWidget *m_iconButton;


    QnStatusOverlayWidget *m_statusOverlayWidget;

    struct OverlayWidgets {
        GraphicsWidget *cameraNameOnlyOverlay;
        GraphicsLabel  *cameraNameOnlyLabel;

        GraphicsWidget *cameraNameWithButtonsOverlay;
        GraphicsLabel  *mainNameLabel;
        GraphicsLabel  *mainExtrasLabel;

        GraphicsWidget *detailsOverlay;     /**< Overlay containing info item. */
        QnHtmlTextItem *detailsItem;        /**< Detailed camera info (resolution, stream, etc). */

        GraphicsWidget *positionOverlay;    /**< Overlay containing position item. */
        QnHtmlTextItem *positionItem;       /**< Current camera position. */

        OverlayWidgets();
    };

    OverlayWidgets m_overlayWidgets;

    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Whether mouse cursor is in widget. Usable to show/hide decorations. */
    bool m_mouseInWidget;

    QRectF m_zoomRect;

    /** Current overlay. */
    Qn::ResourceStatusOverlay m_statusOverlay;

    Qn::RenderStatus m_renderStatus;

    qint64 m_lastNewFrameTimeMSec;
};

typedef QList<QnResourceWidget *> QnResourceWidgetList;

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Options)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Buttons)
Q_DECLARE_METATYPE(QnResourceWidget::Options)
Q_DECLARE_METATYPE(QnResourceWidget *)
Q_DECLARE_METATYPE(QnResourceWidgetList);
