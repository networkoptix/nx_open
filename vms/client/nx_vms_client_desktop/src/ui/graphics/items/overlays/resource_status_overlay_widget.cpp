// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_status_overlay_widget.h"

#include <cmath>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/style/style.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/math/color_transformations.h>

#include "background_generator.h"

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

QThreadPool QnStatusOverlayWidget::s_threadPool;

QImage QnStatusOverlayWidget::s_whiteGlowHorizontalImage;
QImage QnStatusOverlayWidget::s_whiteGlowVerticalImage;
QImage QnStatusOverlayWidget::s_redGlowHorizontalImage;
QImage QnStatusOverlayWidget::s_redGlowVerticalImage;

namespace {

constexpr int kBackgroundWidth = 910;
constexpr int kBackgroundHeight = 553;
constexpr int kBackgroundRectWidth = 625;
constexpr int kBackgroundRectHeight = 196;
constexpr int kMaskSize = 170;

void disableFocus(QGraphicsItem* item)
{
    if (item->isWidget())
    {
        auto widget = static_cast<QGraphicsWidget*>(item);
        widget->setFocusPolicy(Qt::NoFocus);
    }
}

void makeTransparentForMouse(QGraphicsItem* item)
{
    item->setAcceptedMouseButtons(Qt::NoButton);
    item->setAcceptHoverEvents(false);
    disableFocus(item);
}

QnMaskedProxyWidget* makeMaskedProxy(
    QWidget* source,
    QGraphicsItem* parentItem,
    bool transparent)
{
    const auto result = new QnMaskedProxyWidget(parentItem);
    result->setWidget(source);
    result->setAcceptDrops(false);
    result->setCacheMode(QGraphicsItem::NoCache);

    if (transparent)
        makeTransparentForMouse(result);

    disableFocus(result);

    return result;
}

void setupButton(QPushButton& button)
{
    button.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    static const auto kButtonName = "itemStateActionButton";
    button.setObjectName(kButtonName);

    static const auto kStyleSheetTemplateRaw = R"(
        QPushButton#%1
        {
            color: %2;
            background-color: %4;
            border-style: solid;
            border-radius: 4px;
            font: 500 13px;
            padding-left: 16px;
            padding-right: 16px;
            min-height: 28px;
            max-height: 28px;
        }
        QPushButton#%1:hover
        {
            color: %3;
            background-color: %5;
        }
        QPushButton#%1:pressed
        {
            color: %2;
            background-color: %6;
        }
    )";

    static const auto kBaseColor = core::colorTheme()->color("light1");

    static const auto kTextColor = core::colorTheme()->color("light4").name(QColor::HexArgb);
    static const auto kHoveredTextColor = kBaseColor.name(QColor::HexArgb);

    static const auto kBackground = toTransparent(kBaseColor, 0.1).name(QColor::HexArgb);
    static const auto kHovered = toTransparent(kBaseColor, 0.15).name(QColor::HexArgb);
    static const auto kPressed = toTransparent(kBaseColor, 0.05).name(QColor::HexArgb);

    static const QString kStyleSheetTemplate(QString::fromLatin1(kStyleSheetTemplateRaw));
    const auto kStyleSheet = kStyleSheetTemplate.arg(kButtonName,
        kTextColor, kHoveredTextColor, kBackground, kHovered, kPressed);

    button.setStyleSheet(kStyleSheet);
}

void setupCustomButton(QPushButton& button)
{
    button.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    static const auto kButtonName = "itemStateExtraActionButton";
    button.setObjectName(kButtonName);

    static const auto kStyleSheetTemplateRaw = R"(
        QPushButton#%1
        {
            color: %2;
            background-color: transparent;
            border-style: solid;
            border-radius: 4px;
            font: 500 13px;
            padding-left: 16px;
            padding-right: 16px;
            min-height: 32px;
            max-height: 32px;
        }
        QPushButton#%1:hover
        {
            color: %3;
        }
        QPushButton#%1:pressed
        {
            color: %2;
        }
        )";

    static const auto kBaseColor = core::colorTheme()->color("light14");

    static const auto kTextColor = toTransparent(kBaseColor, 0.6).name(QColor::HexArgb);
    static const auto kHoveredTextColor = kBaseColor.name(QColor::HexArgb);

    static const QString kStyleSheetTemplate(QString::fromLatin1(kStyleSheetTemplateRaw));
    const auto kStyleSheet = kStyleSheetTemplate.arg(kButtonName, kTextColor, kHoveredTextColor);

    button.setStyleSheet(kStyleSheet);
}

enum LabelStyleFlag
{
    kNormalStyle = 0x0,
    kErrorStyle = 0x1,
};

Q_DECLARE_FLAGS(LabelStyleFlags, LabelStyleFlag)

LabelStyleFlags getCaptionStyle(bool isError)
{
    return static_cast<LabelStyleFlags>(isError
        ? LabelStyleFlag::kErrorStyle
        : LabelStyleFlag::kNormalStyle);
}

void setupLabel(QLabel* label, LabelStyleFlags style, QnStatusOverlayWidget::ErrorStyle errorStyle)
{
    const bool isError = style.testFlag(kErrorStyle);

    constexpr int kErrorFontPixelSize = 40;
    constexpr int kFontPixelSize = 35;

    auto font = label->font();
    const int pixelSize = isError ? kErrorFontPixelSize : kFontPixelSize;

    font.setPixelSize(pixelSize);
    label->setFont(font);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);

    constexpr int kTextRectWidth = 300;
    label->setFixedWidth(kTextRectWidth);

    QColor color =
        style.testFlag(kErrorStyle) && errorStyle == QnStatusOverlayWidget::ErrorStyle::red
            ? core::colorTheme()->color("red")
            : core::colorTheme()->color("light16");

    setPaletteColor(label, QPalette::WindowText, color);
}

} // namespace

QnStatusOverlayWidget::QnStatusOverlayWidget(QGraphicsWidget* parent):
    base_type(parent),
    m_visibleControls(Control::kNoControl),
    m_initialized(false),
    m_iconVisible(false),
    m_useErrorStyle(false),
    m_errorStyle(ErrorStyle::red),
    m_showGlow(false),
    m_preloaderHolder(new QnViewportBoundWidget(this)),
    m_centralHolder(new QnViewportBoundWidget(this)),
    m_extrasHolder(new QnViewportBoundWidget(this)),
    m_centralContainer(new QWidget()),
    m_extrasContainer(new QWidget()),
    m_preloader(new BusyIndicatorGraphicsWidget()),
    m_imageItem(new QGraphicsPixmapItem(this)),
    m_centralAreaImage(new QLabel()),
    m_caption(new QLabel()),
    m_button(new QPushButton()),
    m_customButton(new QPushButton())
{
    makeTransparentForMouse(this);

    m_centralContainer->installEventFilter(this);
    m_centralContainer->setAttribute(Qt::WA_Hover);

    connect(this, &GraphicsWidget::geometryChanged, this, &QnStatusOverlayWidget::updateAreaSizes);
    connect(this, &GraphicsWidget::scaleChanged, this, &QnStatusOverlayWidget::updateAreaSizes);
    connect(m_button, &QPushButton::clicked, this, &QnStatusOverlayWidget::actionButtonClicked);
    connect(m_customButton, &QPushButton::clicked, this, &QnStatusOverlayWidget::customButtonClicked);

    setupPreloader();
    setupCentralControls();
    setupExtrasControls();

    s_threadPool.waitForDone();
}

void QnStatusOverlayWidget::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    const PainterTransformScaleStripper scaleStripper(painter);
    const auto& destRect =
        scaleStripper.mapRect(rect()).adjusted(-0.5, -0.5, 0.5, 0.5);
    painter->fillRect(destRect, palette().window());
    if (m_showGlow)
    {
        const auto& scaledPixmap =
            getBackgroundPixmap().scaled(destRect.size().toSize(), Qt::KeepAspectRatio);
        auto height = destRect.height();
        auto x = destRect.x() + (destRect.width() - scaledPixmap.width()) / 2;
        auto y = destRect.y() + (height - scaledPixmap.height()) / 2;
        painter->drawPixmap(QPointF{x, y}, scaledPixmap);
    }
}

void QnStatusOverlayWidget::setVisibleControls(Controls controls)
{
    if (m_visibleControls == controls)
        return;

    const bool preloaderVisible = controls.testFlag(Control::kPreloader);
    const bool imageOverlayVisible = controls.testFlag(Control::kImageOverlay);

    m_iconVisible = controls.testFlag(Control::kIcon);
    const bool captionVisible = controls.testFlag(Control::kCaption);
    const bool centralVisible = (m_iconVisible || captionVisible);

    const bool buttonVisible = controls.testFlag(Control::kButton);
    const bool customButtonVisible = controls.testFlag(Control::kCustomButton);
    m_visibleExtrasControlsCount = (buttonVisible ? 1 : 0) + (customButtonVisible ? 1 : 0);
    const bool extrasVisible = m_visibleExtrasControlsCount > 0;

    m_imageItem.setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    m_preloaderHolder->setVisible(preloaderVisible);
    m_imageItem.setVisible(!preloaderVisible && imageOverlayVisible);
    m_centralHolder->setVisible(!preloaderVisible && !imageOverlayVisible && centralVisible);
    m_extrasHolder->setVisible(!preloaderVisible && !imageOverlayVisible && extrasVisible);
    m_button->setVisible(buttonVisible);
    m_customButton->setVisible(customButtonVisible);
    m_centralAreaImage->setVisible(m_iconVisible);
    m_caption->setVisible(captionVisible);

    m_centralContainer->adjustSize();
    m_extrasContainer->adjustSize();

    m_visibleControls = controls;
    updateAreaSizes();
}

void QnStatusOverlayWidget::setIconOverlayPixmap(const QPixmap& pixmap)
{
    m_imageItem.setPixmap(pixmap);
    updateAreaSizes();
}

void QnStatusOverlayWidget::setIcon(const QPixmap& pixmap)
{
    const auto size = pixmap.isNull()
        ? QSize(0, 0)
        : pixmap.size() / pixmap.devicePixelRatio();
    m_centralAreaImage->setPixmap(pixmap);
    m_centralAreaImage->setFixedSize(size);
    m_centralAreaImage->setVisible(!pixmap.isNull());
}

void QnStatusOverlayWidget::setUseErrorStyle(bool useErrorStyle)
{
    if (m_useErrorStyle == useErrorStyle)
        return;

    m_useErrorStyle = useErrorStyle;

    setupLabel(m_caption, getCaptionStyle(useErrorStyle), m_errorStyle);
    updateAreaSizes();
}

void QnStatusOverlayWidget::setErrorStyle(ErrorStyle errorStyle)
{
    if (errorStyle == m_errorStyle)
        return;

    m_errorStyle = errorStyle;

    setupLabel(m_caption, getCaptionStyle(m_useErrorStyle), errorStyle);
    updateAreaSizes();
    getBackgroundPixmap();  //< preload required pixmap
}

void QnStatusOverlayWidget::setCaption(const QString& caption)
{
    m_caption->setText(caption);
    setupLabel(m_caption, getCaptionStyle(m_useErrorStyle), m_errorStyle);
    updateAreaSizes();
}

void QnStatusOverlayWidget::setButtonText(const QString& text)
{
    m_button->setText(text);
    updateAreaSizes();
}

void QnStatusOverlayWidget::setCustomButtonText(const QString& text)
{
    m_customButton->setText(text);
    updateAreaSizes();
}

void QnStatusOverlayWidget::setTooltip(const QString& tooltip)
{
    if (tooltip != m_tooltip)
    {
        m_tooltip = tooltip;
        if (m_centralAreaImage->underMouse() || m_caption->underMouse())
        {
            // Fake enter event to re-send tooltip changed message.
            QEnterEvent event({}, {}, {});
            eventFilter(m_caption->underMouse() ? m_caption : m_centralAreaImage, &event);
        }
    }
}

void QnStatusOverlayWidget::setShowGlow(bool showGlow)
{
    m_showGlow = showGlow;
}

QString QnStatusOverlayWidget::tooltip()
{
    return m_tooltip;
}

void QnStatusOverlayWidget::generateBackgrounds()
{
    using namespace nx::vms::client::core;

    const QRect kHorizontalRect{(kBackgroundWidth - kBackgroundRectWidth) / 2,
        (kBackgroundHeight - kBackgroundRectHeight) / 2,
        kBackgroundRectWidth,
        kBackgroundRectHeight};
    const QRect kVerticalRect{(kBackgroundHeight - kBackgroundRectHeight) / 2,
        (kBackgroundWidth - kBackgroundRectWidth) / 2,
        kBackgroundRectHeight,
        kBackgroundRectWidth};

    s_threadPool.setThreadPriority(QThread::LowestPriority);
    s_threadPool.start(
        [kHorizontalRect]
        {
            s_whiteGlowHorizontalImage = generatePlaceholderBackground(kBackgroundWidth,
                kBackgroundHeight,
                kHorizontalRect,
                kMaskSize,
                colorTheme()->color("light16"),
                colorTheme()->color("dark4"));
        });
    s_threadPool.start(
        [kVerticalRect]
        {
            s_whiteGlowVerticalImage = generatePlaceholderBackground(kBackgroundHeight,
                kBackgroundWidth,
                kVerticalRect,
                kMaskSize,
                colorTheme()->color("light16"),
                colorTheme()->color("dark4"));
        });
    s_threadPool.start(
        [kHorizontalRect]
        {
            s_redGlowHorizontalImage = generatePlaceholderBackground(kBackgroundWidth,
                kBackgroundHeight,
                kHorizontalRect,
                kMaskSize,
                colorTheme()->color("red"),
                colorTheme()->color("dark4"));
        });
    s_threadPool.start(
        [kVerticalRect]
        {
            s_redGlowVerticalImage = generatePlaceholderBackground(kBackgroundHeight,
                kBackgroundWidth,
                kVerticalRect,
                kMaskSize,
                colorTheme()->color("red"),
                colorTheme()->color("dark4"));
        });
}

bool QnStatusOverlayWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_centralContainer)
    {
        switch (event->type())
        {
            case QEvent::HoverEnter:
            case QEvent::HoverMove:
            {
                QRect extendedImageArea;
                if (m_centralAreaImage->isVisible())
                {
                    extendedImageArea.setRect(m_centralAreaImage->x(),
                        m_centralAreaImage->y(),
                        m_centralAreaImage->width(),
                        m_caption->y() - m_centralAreaImage->y());
                }
                auto textWidth = m_caption->fontMetrics().horizontalAdvance(m_caption->text());
                auto textRect = QRect{m_caption->x() + (m_caption->width() - textWidth) / 2,
                    m_caption->y(),
                    textWidth,
                    m_caption->height()};

                auto hoverEvent = static_cast<QHoverEvent*>(event);
                if (extendedImageArea.contains(hoverEvent->position().toPoint())
                    || textRect.contains(hoverEvent->position().toPoint()))
                {
                    constexpr int kTooltipOffset = 4;
                    auto widget = m_centralAreaImage->isVisible() ? m_centralAreaImage : m_caption;
                    QPoint aboveTop{widget->width() / 2, -kTooltipOffset};
                    const auto& globalPos = widget->mapToGlobal(aboveTop);
                    emit tooltipUpdated(globalPos);
                    break;
                }
                [[fallthrough]];
            }
            case QEvent::HoverLeave:
            case QEvent::Leave:
                emit tooltipUpdated();
                break;
        }
    }
    return false;
}

void QnStatusOverlayWidget::setupPreloader()
{
    m_preloader->setIndicatorColor(core::colorTheme()->color("light16", 191));
    m_preloader->setBorderColor(core::colorTheme()->color("dark5"));
    m_preloader->dots()->setDotRadius(8);
    m_preloader->dots()->setDotSpacing(8);
    m_preloader->setMinimumSize(200, 200); //< For correct downscaling in small items.

    const auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->addItem(m_preloader);

    m_preloaderHolder->setLayout(layout);
    makeTransparentForMouse(m_preloaderHolder);
}

void QnStatusOverlayWidget::setupCentralControls()
{
    m_centralAreaImage->setVisible(false);

    setupLabel(m_caption, getCaptionStyle(m_useErrorStyle), m_errorStyle);
    m_caption->setVisible(false);

    m_centralContainer->setObjectName("centralContainer");
    m_centralContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setPaletteColor(m_centralContainer, QPalette::Window, Qt::transparent);

    constexpr int kIconTextSpacing = 16;
    const auto layout = new QVBoxLayout(m_centralContainer);
    layout->setSpacing(kIconTextSpacing);

    layout->addWidget(m_centralAreaImage, 0, Qt::AlignHCenter);
    layout->addWidget(m_caption, 0, Qt::AlignHCenter);

    const auto horizontalLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    horizontalLayout->addStretch(1);
    horizontalLayout->addItem(makeMaskedProxy(m_centralContainer, m_centralHolder, false));
    horizontalLayout->addStretch(1);

    const auto verticalLayout = new QGraphicsLinearLayout(Qt::Vertical, m_centralHolder);
    verticalLayout->setContentsMargins(16, 60, 16, 60);
    verticalLayout->addStretch(1);
    verticalLayout->addItem(horizontalLayout);
    verticalLayout->addStretch(1);

    makeTransparentForMouse(m_centralHolder);
}

void QnStatusOverlayWidget::setupExtrasControls()
{
    setupButton(*m_button);
    setupCustomButton(*m_customButton);

    /* Even though there's only one button in the extras holder,
     * a container widget with a layout must be created, otherwise
     * graphics proxy doesn't handle size hint changes at all. */

    m_extrasContainer->setAttribute(Qt::WA_TranslucentBackground);
    m_extrasContainer->setObjectName("extrasContainer");

    const auto layout = new QVBoxLayout(m_extrasContainer);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_button);
    layout->setAlignment(m_button, Qt::AlignHCenter);

    const auto horizontalLayout = new QGraphicsLinearLayout(Qt::Horizontal, m_extrasHolder);
    horizontalLayout->addStretch(1);
    horizontalLayout->addItem(makeMaskedProxy(m_extrasContainer, m_extrasHolder, false));
    horizontalLayout->addStretch(1);

    makeTransparentForMouse(m_extrasHolder);
}

void QnStatusOverlayWidget::initializeHandlers()
{
    const auto instrumentManager = InstrumentManager::instance(scene());
    if (!instrumentManager)
        return;

    const auto transformInstrument =
        instrumentManager->instrument<TransformListenerInstrument>();
    if (!transformInstrument)
        return;

    connect(transformInstrument, &TransformListenerInstrument::transformChanged,
        this, &QnStatusOverlayWidget::updateAreaSizes);

    m_initialized = true;
}

void QnStatusOverlayWidget::updateAreaSizes()
{
    const auto currentScene = scene();
    if (!currentScene)
        return;

    if (!m_initialized)
        initializeHandlers();

    const auto view = (!scene()->views().isEmpty() ? scene()->views()[0] : nullptr);
    if (!view)
        return;

    const auto rect = geometry();
    const auto height = rect.height();

    QTransform sceneToViewport = view->viewportTransform();
    qreal scale = 1.0
        / std::sqrt(sceneToViewport.m11() * sceneToViewport.m11()
            + sceneToViewport.m12() * sceneToViewport.m12());

    bool showExtras = m_visibleExtrasControlsCount > 0;

    const qreal minHeight = 212 * scale;
    showExtras = showExtras
        && (rect.height()
            > minHeight * m_visibleExtrasControlsCount); // Do not show extras on too small items

    if (rect.height() <= minHeight)
        m_centralAreaImage->hide();
    else
        m_centralAreaImage->setVisible(m_iconVisible);

    m_preloaderHolder->setFixedSize(rect.size());
    m_centralHolder->setFixedSize(QSizeF(rect.width(), height));

    const bool showCentralArea = (!m_visibleControls.testFlag(Control::kPreloader)
        && !m_visibleControls.testFlag(Control::kImageOverlay));

    m_extrasHolder->setVisible(showCentralArea && showExtras);

    qreal extrasHeight = .0;
    if (m_extrasHolder->isVisible())
    {
        m_extrasContainer->adjustSize();
        const auto& containerGeometry = m_extrasContainer->geometry();
        extrasHeight = containerGeometry.height() * scale;
        m_extrasHolder->setFixedSize(QSizeF(rect.width(), extrasHeight));

        constexpr int kButtonOffsetWithText = 24;
        constexpr int kButtonOffsetWithoutText = 16;

        int buttonOffset = m_caption->isVisible() ? kButtonOffsetWithText : kButtonOffsetWithoutText;
        int textBottomY = m_caption->height() + buttonOffset;
        if (!scene()->views().empty())
        {
            auto view = scene()->views().first();
            const auto& globalPos = m_caption->mapToGlobal(QPoint{0, textBottomY});
            const auto& viewPos = view->mapFromGlobal(globalPos);
            const auto& scenePos = view->mapToScene(viewPos);
            const auto& localPos = mapFromScene(scenePos);
            m_extrasHolder->setPos(0, localPos.y());
        }
    }

    const QSizeF imageSize = m_imageItem.pixmap().isNull()
        ? QSizeF(0, 0)
        : QSizeF(m_imageItem.pixmap().size()) / m_imageItem.pixmap().devicePixelRatioF();
    auto imageSceneSize = imageSize * scale;
    const auto aspect = (imageSceneSize.isNull() || !imageSceneSize.height() || !imageSceneSize.width()
        ? 1
        : imageSceneSize.width() / imageSceneSize.height());

    const auto thirdWidth = rect.width() * 3 / 10;
    const auto thirdHeight = rect.height() * 3 / 10;

    if (imageSceneSize.width() > thirdWidth)
    {
        imageSceneSize.setWidth(thirdWidth);
        imageSceneSize.setHeight(thirdWidth / aspect);
    }

    if (imageSceneSize.height() > thirdHeight)
    {
        imageSceneSize.setHeight(thirdHeight);
        imageSceneSize.setWidth(thirdHeight * aspect);
    }

    const auto imageItemScale = qFuzzyIsNull(imageSize.width())
        ? 1.0
        : imageSceneSize.width() / imageSize.width();

    m_imageItem.setScale(imageItemScale);
    m_imageItem.setPos((rect.width() - imageSceneSize.width()) / 2,
        (rect.height() - imageSceneSize.height()) / 2);

    m_extrasHolder->updateScale();
    m_preloaderHolder->updateScale();
    m_centralHolder->updateScale();
}

QPixmap QnStatusOverlayWidget::getBackgroundPixmap()
{
    NX_ASSERT(qApp && qApp->thread() == QThread::currentThread()
        && s_threadPool.activeThreadCount() == 0 && !s_whiteGlowHorizontalImage.isNull()
        && !s_whiteGlowVerticalImage.isNull() && !s_redGlowHorizontalImage.isNull()
        && !s_redGlowVerticalImage.isNull());

    const auto whiteGlowHorizontal = std::make_pair(
        std::cref(s_whiteGlowHorizontalImage), std::ref(m_whiteGlowHorizontalPixmap));
    const auto whiteGlowVertical =
        std::make_pair(std::cref(s_whiteGlowVerticalImage), std::ref(m_whiteGlowVerticalPixmap));
    const auto redGlowHorizontal =
        std::make_pair(std::cref(s_redGlowHorizontalImage), std::ref(m_redGlowHorizontalPixmap));
    const auto redGlowVertical =
        std::make_pair(std::cref(s_redGlowVerticalImage), std::ref(m_redGlowVerticalPixmap));

    const auto isError = (m_errorStyle == ErrorStyle::red);

    const auto& background = (rect().width() >= rect().height())
        ? (isError ? redGlowHorizontal : whiteGlowHorizontal)
        : (isError ? redGlowVertical : whiteGlowVertical);

    if (background.second.isNull())
        background.second = QPixmap::fromImage(background.first);

    return background.second;
}

QPixmap QnStatusOverlayWidget::whiteGlowHorizontal()
{
    NX_ASSERT(qApp && qApp->thread() == QThread::currentThread()
        && s_threadPool.activeThreadCount() == 0 && !s_whiteGlowHorizontalImage.isNull());

    if (m_whiteGlowHorizontalPixmap.isNull())
        m_whiteGlowHorizontalPixmap = QPixmap::fromImage(s_whiteGlowHorizontalImage);

    return m_whiteGlowHorizontalPixmap;
}

QPixmap QnStatusOverlayWidget::whiteGlowVertical()
{
    NX_ASSERT(qApp && qApp->thread() == QThread::currentThread()
        && s_threadPool.activeThreadCount() == 0 && !s_whiteGlowVerticalImage.isNull());

    if (m_whiteGlowVerticalPixmap.isNull())
        m_whiteGlowVerticalPixmap = QPixmap::fromImage(s_whiteGlowVerticalImage);

    return m_whiteGlowVerticalPixmap;
}

QPixmap QnStatusOverlayWidget::redGlowHorizontal()
{
    NX_ASSERT(qApp && qApp->thread() == QThread::currentThread()
        && s_threadPool.activeThreadCount() == 0 && !s_redGlowHorizontalImage.isNull());

    if (m_redGlowHorizontalPixmap.isNull())
        m_redGlowHorizontalPixmap = QPixmap::fromImage(s_redGlowHorizontalImage);

    return m_redGlowHorizontalPixmap;
}

QPixmap QnStatusOverlayWidget::redGlowVertical()
{
    NX_ASSERT(qApp && qApp->thread() == QThread::currentThread()
        && s_threadPool.activeThreadCount() == 0 && !s_redGlowVerticalImage.isNull());

    if (m_redGlowVerticalPixmap.isNull())
        m_redGlowVerticalPixmap = QPixmap::fromImage(s_redGlowVerticalImage);

    return m_redGlowVerticalPixmap;
}
