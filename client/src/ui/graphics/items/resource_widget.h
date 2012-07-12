#ifndef QN_RESOURCE_WIDGET_H
#define QN_RESOURCE_WIDGET_H

#include <QtCore/QWeakPointer>
#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtGui/QStaticText>
#include <QtGui/QGraphicsWidget>

#include <camera/render_status.h>
#include <core/resource/motion_window.h>
#include <core/resource/resource_consumer.h>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */

#include <ui/common/constrained_resizable.h>
#include <ui/common/geometry.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/shadow/shaded.h>

class QGraphicsLinearLayout;

class QnViewportBoundWidget;
class QnResourceWidgetRenderer;
class QnVideoResourceLayout;
class QnWorkbenchItem;
class QnResourceDisplay;
class QnAbstractArchiveReader;

class QnLoadingProgressPainter;
class QnPausedPainter;
class QnImageButtonWidget;

class GraphicsLabel;
class Instrument;

/* Get rid of stupid win32 defines. */
#ifdef NO_DATA
#   undef NO_DATA
#endif

class QnResourceWidget: public Shaded<Instrumented<GraphicsWidget> >, public QnWorkbenchContextAware, public ConstrainedResizable, public FrameSectionQuearyable, protected QnGeometry {
    Q_OBJECT;
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity);
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth);
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement);
    Q_PROPERTY(QRectF enclosingGeometry READ enclosingGeometry WRITE setEnclosingGeometry);
    Q_PROPERTY(qreal enclosingAspectRatio READ enclosingAspectRatio WRITE setEnclosingAspectRatio);
    Q_FLAGS(DisplayFlags DisplayFlag);

    typedef Shaded<Instrumented<GraphicsWidget> > base_type;

public:
    enum DisplayFlag {
        DisplayActivityOverlay      = 0x1,  /**< Whether the paused overlay icon should be displayed. */
        DisplaySelectionOverlay     = 0x2,  /**< Whether selected / not selected state should be displayed. */
        DisplayMotion               = 0x4,  /**< Whether motion is to be displayed. */                              // TODO: this flag also handles smart search, separate!
        DisplayButtons              = 0x8,  /**< Whether item buttons are to be displayed. */
        DisplayMotionSensitivity    = 0x10, /**< Whether a grid with motion region sensitivity is to be displayed. */
    };
    Q_DECLARE_FLAGS(DisplayFlags, DisplayFlag)

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
     * \returns                         Display flags for this widget.
     */
    DisplayFlags displayFlags() const {
        return m_displayFlags;
    }

    /**
     * \param flag                      Affected flag.
     * \param value                     New value for the affected flag.
     */
    void setDisplayFlag(DisplayFlag flag, bool value = true) {
        setDisplayFlags(value ? m_displayFlags | flag : m_displayFlags & ~flag);
    }

    /**
     * \param flags                     New display flags for this widget.
     */
    void setDisplayFlags(DisplayFlags flags);

    /**
     * \returns                         Status of the last rendering operation.
     */
    Qn::RenderStatus currentRenderStatus() const {
        return m_renderStatus;
    }

    /**
     * \returns                         Text of this window's title.
     */
    QString titleText() const;

    /**
     * \param titleText                 New title text for this window.
     */
    void setTitleText(const QString &titleText);

    /**
     * \returns                         Information text that is displayed in this widget's footer.
     */
    QString infoText();

    /**
     * \param infoText                  New text to be displayed in this widget's footer. 
     *                                  If <tt>'\t'</tt> symbol is used in the text, 
     *                                  it will be split in two parts at the position of this symbol,
     *                                  and these parts will be aligned to the sides of the footer.
     */
    void setInfoText(const QString &infoText);

    using base_type::mapRectToScene;

public slots:
    void showActivityDecorations();
    void hideActivityDecorations();
    void fadeOutOverlay();
    void fadeInOverlay();
    void fadeOutInfo();
    void fadeInInfo();
    void fadeInfo(bool fadeIn);

signals:
    void aspectRatioChanged(qreal oldAspectRatio, qreal newAspectRatio);
    void aboutToBeDestroyed();
    void displayFlagsChanged();
    void rotationStartRequested();
    void rotationStopRequested();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannel(QPainter *painter, int channel, const QRectF &rect) = 0;

    void assertPainters(); 

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override;
    
    virtual bool windowFrameEvent(QEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    virtual QSizeF constrainedSize(const QSizeF constraint) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    void ensureAboutToBeDestroyedEmitted();

    const QSize &channelScreenSize() const;
    virtual void updateChannelScreenSize(const QSize &channelScreenSize);


signals:
    void updateOverlayTextLater();

protected slots:
    virtual void updateOverlayText();
    void updateButtonsVisibility();

    void at_sourceSizeChanged(const QSize &size);
    void at_resource_resourceChanged();
    void at_resource_nameChanged();
    void at_searchButton_toggled(bool checked);

protected:
    /**
     * \param channel                   Channel number.
     * \returns                         Rectangle in local coordinates where given channel is to be drawn.
     */
    QRectF channelRect(int channel) const;

    enum OverlayIcon {
        NO_ICON,
        PAUSED,
        LOADING,
        NO_DATA,
        OFFLINE,
        UNAUTHORIZED
    };

    struct ChannelState {
        ChannelState(): icon(NO_ICON), iconChangeTimeMSec(0), iconFadeInNeeded(false), lastNewFrameTimeMSec(0) {}

        /** Current overlay icon. */
        OverlayIcon icon;

        /** Time when the last icon change has occurred, in milliseconds since epoch. */
        qint64 iconChangeTimeMSec;

        /** Whether the icon should fade in on change. */
        bool iconFadeInNeeded;

        /** Last time when new frame was rendered, in milliseconds since epoch. */
        qint64 lastNewFrameTimeMSec;
    };

    void setOverlayIcon(int channel, OverlayIcon icon);

    void drawSelection(const QRectF &rect);

    void drawOverlayIcon(int channel, const QRectF &rect);

    void drawMotionGrid(QPainter *painter, const QRectF &rect, const QnMetaDataV1Ptr &motion, int channel);

    void drawMotionMask(QPainter *painter, const QRectF& rect, int channel);

    void drawFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor& color, const QColor& penColor);

    void drawFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset = QPointF());

protected:
    /** Paused painter. */
    QSharedPointer<QnPausedPainter> m_pausedPainter;

    /** Loading progress painter. */
    QSharedPointer<QnLoadingProgressPainter> m_loadingProgressPainter;

    /** Layout item. */
    QWeakPointer<QnWorkbenchItem> m_item;

    /** Resource associated with this widget. */
    QnResourcePtr m_resource;

    /* Display flags. */
    DisplayFlags m_displayFlags;

    /** Layout of this widget's contents. */
    const QnVideoResourceLayout *m_contentLayout;

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

    /* Widgets for overlaid stuff. */
    QnViewportBoundWidget *m_headerOverlayWidget;
    QGraphicsLinearLayout *m_headerLayout;
    GraphicsLabel *m_headerLabel;

    QnImageButtonWidget *m_infoButton;
    QnImageButtonWidget *m_closeButton;
    QnImageButtonWidget *m_rotateButton;
    QnImageButtonWidget *m_searchButton;

    QnViewportBoundWidget *m_footerOverlayWidget;
    QGraphicsWidget *m_footerWidget;
    GraphicsLabel *m_footerLeftLabel;
    GraphicsLabel *m_footerRightLabel;

    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Additional per-channel state. */
    QVector<ChannelState> m_channelState;

    /** Status of the last painting operation. */
    Qn::RenderStatus m_renderStatus;

    QStaticText m_noDataStaticText;
    QStaticText m_offlineStaticText;
    QStaticText m_unauthorizedStaticText;
    QStaticText m_unauthorizedStaticText2;
    QStaticText m_sensStaticText[10];
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::DisplayFlags);
Q_DECLARE_METATYPE(QnResourceWidget *)

#endif // QN_RESOURCE_WIDGET_H
