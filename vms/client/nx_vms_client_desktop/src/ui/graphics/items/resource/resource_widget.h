// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QMetaType>
#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <qt_graphics_items/graphics_widget.h>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/animation/animated.h>
#include <ui/common/constrained_resizable.h>
#include <ui/common/fixed_rotation.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/common/help_topic_queryable.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/overlays/overlayed.h>

class QGraphicsLinearLayout;

class QnViewportBoundWidget;
class QnResourceVideoLayout;
class QnWorkbenchItem;
class QnStatusOverlayController;
class QnImageButtonWidget;
class QnImageButtonBar;
class QnProxyLabel;
class QnHtmlTextItem;
class QnResourceTitleItem;
class GraphicsLabel;
class QnStatusOverlayWidget;
class QnHudOverlayWidget;

class QnResourceWidget:
    public Overlayed<Animated<Instrumented<GraphicsWidget>>>,
    public nx::vms::client::desktop::SystemContextAware,
    public nx::vms::client::desktop::WindowContextAware,
    public ConstrainedResizable,
    public HelpTopicQueryable
{
    Q_OBJECT
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity)
    Q_PROPERTY(QColor frameDistinctionColor READ frameDistinctionColor WRITE setFrameDistinctionColor)
    Q_PROPERTY(bool localActive READ isLocalActive WRITE setLocalActive)
    Q_PROPERTY(QnUuid uuid READ uuid)
    Q_PROPERTY(QString uuidString READ uuidString)
    Q_PROPERTY(bool isZoomWindow READ isZoomWindow)
    Q_FLAGS(Options Option)

    using base_type = Overlayed<Animated<Instrumented<GraphicsWidget>>>;

public:
    enum Option
    {
        /** Whether the paused overlay icon should be displayed. */
        DisplayActivity = 1 << 0,

        /** Whether selected / not selected state should be displayed. */
        DisplaySelection = 1 << 1,

        // TODO: #sivanov This flag also handles smart search, separate.
        /** Whether motion is to be displayed. */
        DisplayMotion = 1 << 2,

        /** Whether info panel is to be displayed. */
        DisplayInfo = 1 << 3,

        /** Whether detected analytics objects are displayed. */
        DisplayAnalyticsObjects = 1 << 4,

        /** Whether regions of interest (ROI) are displayed. */
        DisplayRoi = 1 << 5,

        /** Whether the video is to be dewarped. */
        DisplayDewarped = 1 << 6,

        /** Whether PTZ state can be controlled. */
        ControlPtz = 1 << 7,

        /** Whether zoom windows can be created by dragging the mouse. */
        ControlZoomWindow = 1 << 8,

        WindowRotationForbidden = 1 << 9,
        WindowResizingForbidden = 1 << 10,

        FullScreenMode = 1 << 11,
        ActivityPresence = 1 << 12,

        AlwaysShowName = 1 << 13,
        InfoOverlaysForbidden = 1 << 14,
        AllowFocus = 1 << 15
    };

    Q_DECLARE_FLAGS(Options, Option)

    enum AspectRatioFlag
    {
        SingleChannel           = 0x0,
        WithRotation            = 0x01,
        WithChannelLayout       = 0x02
    };
    Q_DECLARE_FLAGS(AspectRatioFlags, AspectRatioFlag)

    enum class SelectionState
    {
        invalid,
        notSelected,
        inactiveFocused,
        focused,
        selected,
        focusedAndSelected,
    };

    /**
     * Constructor.
     *
     * \param context                   Context in which this resource widget operates.
     * \param item                      Workbench item that this widget represents.
     * \param parent                    Parent item.
     */
    QnResourceWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~QnResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    const QnResourcePtr &resource() const;

    /** Layout resource, owning this item. */
    nx::vms::client::desktop::LayoutResourcePtr layoutResource() const;

    /**
     * \returns                         Workbench item associated with this widget. Never returns nullptr.
     */
    QnWorkbenchItem *item() const;

    /**
     * \returns                         Layout of channels in this widget. Never returns nullptr.
     */
    QnConstResourceVideoLayoutPtr channelLayout() const {
        return m_channelsLayout;
    }

    const QRectF &zoomRect() const;
    void setZoomRect(const QRectF &zoomRect);
    QnResourceWidget *zoomTargetWidget() const;

    bool isZoomWindow() const;

    /**
     * \returns                         Frame opacity of this widget.
     */
    qreal frameOpacity() const;

    /**
     * \param frameOpacity              New frame opacity for this widget.
     */
    void setFrameOpacity(qreal frameOpacity);

    QColor frameDistinctionColor() const;
    void setFrameDistinctionColor(const QColor &frameColor);

    /**
     * \returns                         Aspect ratio of this widget.
     *                                  Negative value will be returned if this
     *                                  widget does not have aspect ratio.
     */
    float aspectRatio() const;

    void setAspectRatio(float aspectRatio);

    /**
     * \returns                         Whether this widget has an aspect ratio.
     */
    bool hasAspectRatio() const;

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
    Options options() const;

    /**
     * \param option                    Affected option.
     * \param value                     New value for the affected option.
     */
    void setOption(Option option, bool value = true);

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
    Q_SLOT void setInfoVisible(bool visible, bool animate);

    bool isLocalActive() const;
    void setLocalActive(bool localActive);

    QnResourceTitleItem* titleBar() const;

    void setCheckedButtons(int buttons);

    int checkedButtons() const;

    int visibleButtons() const;

    SelectionState selectionState() const;

    QPixmap placeholderPixmap() const;
    void setPlaceholderPixmap(const QPixmap& pixmap);

    /** Current action indicator text. When empty, action indication is hidden. */
    QString actionText() const;
    void setActionText(const QString& value);
    void clearActionText(std::chrono::milliseconds after);

    using base_type::mapRectToScene;

    /** Returns corresponding workbench item id. */
    QnUuid uuid() const;

    /** Returns corresponding workbench item id as a string. */
    QString uuidString() const;

    /** Debug string representation. */
    QString toString() const;

signals:
    void painted();
    void aspectRatioChanged();
    void optionsChanged(Options changedOptions, QPrivateSignal);
    void zoomRectChanged();
    void zoomTargetWidgetChanged();
    void rotationStartRequested();
    void rotationStopRequested();
    void displayInfoChanged();
    void selectionStateChanged(SelectionState state, QPrivateSignal);
    void placeholderPixmapChanged();

protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;
    virtual Qn::RenderStatus paintChannelBackground(QPainter* painter, int channel,
        const QRectF& channelRect, const QRectF& paintRect);
    virtual void paintChannelForeground(QPainter *painter, int channel, const QRectF &rect);
    virtual void paintEffects(QPainter* painter);

    virtual QSizeF constrainedSize(const QSizeF constraint, Qt::WindowFrameSection pinSection) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    const QSize &channelScreenSize() const;
    void setChannelScreenSize(const QSize &size);
    virtual void channelScreenSizeChangedNotify() {}

    virtual bool forceShowPosition() const;
    virtual void updateHud(bool animate);

    virtual bool isHovered() const;

    Qn::ResourceStatusOverlay calculateStatusOverlay(nx::vms::api::ResourceStatus resourceStatus, bool hasVideo) const;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const;
    void updateStatusOverlay(bool animate);

    virtual Qn::ResourceOverlayButton calculateOverlayButton(
        Qn::ResourceStatusOverlay statusOverlay) const;
    virtual QString overlayCustomButtonText(
        Qn::ResourceStatusOverlay statusOverlay) const;

    void updateOverlayButton();
    void updateCustomOverlayButton();

    virtual QString calculateTitleText() const;
    Q_SLOT void updateTitleText();

    virtual QString calculateDetailsText() const;
    virtual QPixmap calculateDetailsIcon() const;
    void updateDetailsText();

    virtual QString calculatePositionText() const;
    void updatePositionText();

    void updateInfoText();

    QnStatusOverlayController *statusOverlayController() const;

    virtual int calculateButtonsVisibility() const;
    Q_SLOT void updateButtonsVisibility();

    void setChannelLayout(QnConstResourceVideoLayoutPtr channelLayout);
    virtual void channelLayoutChangedNotify() {}

    virtual void optionsChangedNotify(Options changedFlags);

    int channelCount() const;
    QRectF channelRect(int channel) const;
    QRectF exposedRect(int channel, bool accountForViewport = true, bool accountForVisibility = true, bool useRelativeCoordinates = false);

    void registerButtonStatisticsAlias(QnImageButtonWidget* button, const QString& alias);
    QnImageButtonWidget* createStatisticAwareButton(const QString& alias);

    Q_SLOT virtual void at_itemDataChanged(int role);

    float defaultAspectRatio() const;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    QString tooltipText(const QString& toolTip, const QString& hotkey) const;
    QString tooltipText(const QString& toolTip, const QKeySequence& hotkey) const;

    bool isVideoWallLicenseValid() const;

    QIcon loadSvgIcon(const QString& name) const;

private:
    void setupOverlayButtonsHandlers();

    void setupHud();
    void setupSelectionOverlay();
    void createButtons();

    void setTitleTextInternal(const QString &titleText);

    /*
    void setupIconButton(QGraphicsLinearLayout *layout
        , QnImageButtonWidget *button);
        */
    Q_SLOT void updateCheckedButtons();

    Q_SLOT void at_infoButton_toggled(bool toggled);

    Q_SLOT void at_buttonBar_checkedButtonsChanged();

private:
    void updateSelectedState();
    void updateFullscreenButton();

protected:
    QnHudOverlayWidget* m_hudOverlay;
    QnStatusOverlayWidget* m_statusOverlay;

private:
    friend class QnWorkbenchDisplay;

    struct Private;
    nx::utils::ImplPtr<Private> d;

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

    QString m_titleTextFormat;
    bool m_titleTextFormatHasPlaceholder;

    /* Widgets for overlaid stuff. */

    QnStatusOverlayController* m_statusController;

    /** Whether mouse cursor is in widget. Usable to show/hide decorations. */
    bool m_mouseInWidget;

    QRectF m_zoomRect;

    /** Current overlay. */
    Qn::RenderStatus m_renderStatus;

    qint64 m_lastNewFrameTimeMSec;

    SelectionState m_selectionState;

    QPixmap m_placeholderPixmap;
};

typedef QList<QnResourceWidget *> QnResourceWidgetList;

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::Options)
Q_DECLARE_METATYPE(QnResourceWidget::Options)
Q_DECLARE_METATYPE(QnResourceWidget *)
Q_DECLARE_METATYPE(QnResourceWidgetList);
