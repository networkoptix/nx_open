#ifndef QN_RESOURCE_WIDGET_H
#define QN_RESOURCE_WIDGET_H

#include <QtCore/QWeakPointer>
#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtGui/QStaticText>

#include <core/resource/resource_fwd.h>

#include <camera/render_status.h>

#include <ui/common/constrained_resizable.h>
#include <ui/common/geometry.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/shadow/shaded.h>

class QGraphicsLinearLayout;

class QnViewportBoundWidget;
class QnResourceVideoLayout;
class QnWorkbenchItem;

class QnLoadingProgressPainter;
class QnPausedPainter;
class QnImageButtonWidget;
class QnImageButtonBar;

class GraphicsLabel;

class QnResourceWidget: public Shaded<Instrumented<GraphicsWidget> >, public QnWorkbenchContextAware, public ConstrainedResizable, public FrameSectionQuearyable, protected QnGeometry {
    Q_OBJECT
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity)
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth)
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement)
    Q_PROPERTY(QRectF enclosingGeometry READ enclosingGeometry WRITE setEnclosingGeometry)
    Q_PROPERTY(qreal enclosingAspectRatio READ enclosingAspectRatio WRITE setEnclosingAspectRatio)
    Q_FLAGS(Options Option)

    typedef Shaded<Instrumented<GraphicsWidget> > base_type;

public:
    enum Option {
        DisplayActivityOverlay      = 0x1,  /**< Whether the paused overlay icon should be displayed. */
        DisplaySelectionOverlay     = 0x2,  /**< Whether selected / not selected state should be displayed. */
        DisplayMotion               = 0x4,  /**< Whether motion is to be displayed. */                              // TODO: this flag also handles smart search, separate!
        DisplayButtons              = 0x8,  /**< Whether item buttons are to be displayed. */
        DisplayMotionSensitivity    = 0x10, /**< Whether a grid with motion region sensitivity is to be displayed. */
        DisplayCrosshair            = 0x20, // TODO
        ControlPtz                  = 0x40, // TODO
        DisplayInfo                 = 0x80  /** Whether info widget is to be displayed. */
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

    /**
     * \returns                         Resource associated with this widget.
     */
    QnResourcePtr resource() const;

    /**
     * \returns                         Workbench item associated with this widget. Never returns NULL.
     */
    QnWorkbenchItem *item() const {
        return m_item.data();
    }

    const QnResourceVideoLayout *channelLayout() const {
        return m_channelsLayout;
    }
    
    /**
     * \returns                         Frame opacity of this widget.
     */
    qreal frameOpacity() const {
        return m_frameOpacity;
    }

    /**
     * \param frameColor                New frame opacity for this widget.
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
    Qn::RenderStatus currentRenderStatus() const {
        return m_channelState[0].renderStatus;
    }

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

    // TODO: #gdm move to private interface, update on rotation change.
    /**
     * Updates overlay widget's rotation.
     *
     * \param rotation - target rotation angle in degrees.
     */
    void updateOverlayRotation(qreal rotation);

    bool isDecorationsVisible() const;
    Q_SLOT void setDecorationsVisible(bool visible = true, bool animate = true);

    bool isInfoVisible() const;
    Q_SLOT void setInfoVisible(bool visible, bool animate = true);

    bool isInfoButtonVisible() const;

    using base_type::mapRectToScene;

signals:
    void aspectRatioChanged();
    void aboutToBeDestroyed();
    void optionsChanged();
    void rotationStartRequested();
    void rotationStopRequested();

protected:
    enum Overlay {
        EmptyOverlay,
        PausedOverlay,
        LoadingOverlay,
        NoDataOverlay,
        OfflineOverlay,
        UnauthorizedOverlay
    };

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override;

    virtual bool windowFrameEvent(QEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &rect) = 0;
    virtual void paintChannelForeground(QPainter *painter, int channel, const QRectF &rect);
    virtual void paintOverlay(QPainter *painter, const QRectF &rect, Overlay overlay);
    
    void paintSelection(QPainter *painter, const QRectF &rect);
    void paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset = QPointF());

    virtual QSizeF constrainedSize(const QSizeF constraint) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    const QSize &channelScreenSize() const;
    void setChannelScreenSize(const QSize &size);
    virtual void channelScreenSizeChangedNotify() {}

    Overlay channelOverlay(int channel) const;
    void setChannelOverlay(int channel, Overlay overlay);
    Overlay calculateChannelOverlay(int channel, int resourceStatus) const;
    virtual Overlay calculateChannelOverlay(int channel) const;
    Q_SLOT void updateChannelOverlay(int channel);

    virtual QString calculateTitleText() const;
    Q_SLOT void updateTitleText();

    virtual QString calculateInfoText() const;
    Q_SLOT void updateInfoText();

    QnImageButtonBar *buttonBar() const {
        return m_buttonBar;
    }

    QnImageButtonWidget *iconButton() const {
        return m_iconButton;
    }

    virtual Buttons calculateButtonsVisibility() const;
    Q_SLOT void updateButtonsVisibility();

    void setChannelLayout(const QnResourceVideoLayout *channelLayout);
    virtual void channelLayoutChangedNotify() {}
    
    virtual void optionsChangedNotify(Options changedFlags);

    int channelCount() const;
    QRectF channelRect(int channel) const;
    Qn::RenderStatus channelRenderStatus(int channel) const;

    void ensureAboutToBeDestroyedEmitted();

private:
    void setTitleTextInternal(const QString &titleText);
    void setInfoTextInternal(const QString &infoText);

    Q_SLOT void at_iconButton_visibleChanged();

    struct ChannelState {
        ChannelState(): overlay(EmptyOverlay), changeTimeMSec(0), fadeInNeeded(false), lastNewFrameTimeMSec(0), renderStatus(Qn::NothingRendered) {}

        /** Current overlay. */
        Overlay overlay;

        /** Time when the last icon change has occurred, in milliseconds since epoch. */
        qint64 changeTimeMSec;

        /** Whether the icon should fade in on change. */
        bool fadeInNeeded;

        /** Last time when new frame was rendered, in milliseconds since epoch. */
        qint64 lastNewFrameTimeMSec;

        /** Last render status. */
        Qn::RenderStatus renderStatus;
    };

private:
    /** Paused painter. */
    QSharedPointer<QnPausedPainter> m_pausedPainter;

    /** Loading progress painter. */
    QSharedPointer<QnLoadingProgressPainter> m_loadingProgressPainter;

    /** Layout item. */
    QWeakPointer<QnWorkbenchItem> m_item;

    /** Resource associated with this widget. */
    QnResourcePtr m_resource;

    /* Display flags. */
    Options m_options;

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

    QString m_titleTextFormat, m_infoTextFormat;
    bool m_titleTextFormatHasPlaceholder, m_infoTextFormatHasPlaceholder;

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

    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Additional per-channel state. */
    QVector<ChannelState> m_channelState;

    QStaticText m_noDataStaticText;
    QStaticText m_offlineStaticText;
    QStaticText m_unauthorizedStaticText;
    QStaticText m_unauthorizedStaticText2;
    QStaticText m_loadingStaticText;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Options)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Buttons)
Q_DECLARE_METATYPE(QnResourceWidget *)

#endif // QN_RESOURCE_WIDGET_H
