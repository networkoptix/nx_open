#include "resource_status_overlay_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/style/nx_style.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/common/palette.h>
#include <ui/widgets/word_wrapped_label.h>
#include <ui/widgets/common/busy_indicator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <utils/math/color_transformations.h>

namespace {

void makeTransparentForMouse(QGraphicsItem* item)
{
    item->setAcceptedMouseButtons(Qt::NoButton);
    item->setAcceptHoverEvents(false);
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

    return result;
}

void setupButton(QPushButton& button)
{
    static const auto kDefaultButtonHeight = 28.0;

    button.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button.setFixedHeight(kDefaultButtonHeight);

    static const auto kButtonName = lit("itemStateExtraActionButton");
    button.setObjectName(kButtonName);

    static const auto kStyleSheetTemplate = lit(
        "QPushButton#%1"
        "{"
        "    color: %2;"
        "    background-color: %4;"
        "    border-style: solid;"
        "    border-radius: 2px;"
        "    font: 500 13px;"
        "    padding-left: 16px;"
        "    padding-right: 16px;"
        "    min-height: 32px;"
        "    max-height: 32px;"
        "}"
        "QPushButton#%1:hover"
        "{"
        "    color: %3;"
        "    background-color: %5;"
        "}"
        "QPushButton#%1:pressed"
        "{"
        "    color: %2;"
        "    background-color: %6;"
        "}"
        );

    static const auto kBaseColor = QColor(qnNxStyle->mainColor(
        QnNxStyle::Colors::kContrast).darker(4));

    static const auto kTextColor = toTransparent(kBaseColor, 0.8).name(QColor::HexArgb);
    static const auto kHoveredTextColor = kBaseColor.name(QColor::HexArgb);

    static const auto kBackground = toTransparent(kBaseColor, 0.15).name(QColor::HexArgb);
    static const auto kHovered = toTransparent(kBaseColor, 0.2).name(QColor::HexArgb);
    static const auto kPressed = toTransparent(kBaseColor, 0.15).name(QColor::HexArgb);

    const auto kStyleSheet = kStyleSheetTemplate.arg(kButtonName,
        kTextColor, kHoveredTextColor, kBackground, kHovered, kPressed);

    button.setStyleSheet(kStyleSheet);
}

enum LabelStyleFlag
{
    kNormalStyle = 0x0,
    kErrorStyle = 0x1,
    kDescriptionStyle = 0x2
};

Q_DECLARE_FLAGS(LabelStyleFlags, LabelStyleFlag)

LabelStyleFlags getCaptionStyle(bool isError)
{
    return static_cast<LabelStyleFlags>(isError
        ? LabelStyleFlag::kErrorStyle
        : LabelStyleFlag::kNormalStyle);
}

LabelStyleFlags getDescriptionStyle(bool isError)
{
    const LabelStyleFlag errorFlag = static_cast<LabelStyleFlag>(isError
        ? LabelStyleFlag::kErrorStyle
        : LabelStyleFlag::kNormalStyle);

    return static_cast<LabelStyleFlags>(LabelStyleFlag::kDescriptionStyle | errorFlag);
}

void setupLabel(QLabel* label, LabelStyleFlags style)
{
    const bool isDescription = style.testFlag(kDescriptionStyle);
    const bool isError = style.testFlag(kErrorStyle);

    auto font = label->font();
    const int pixelSize = (isDescription ? 36 : (isError ? 88 : 80));
    const int areaWidth = (isError ? 960 : 800);

    font.setPixelSize(pixelSize);
    font.setWeight(isDescription ? QFont::Normal : QFont::Light);
    label->setFont(font);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(isDescription);

    label->setFixedWidth(isDescription
        ? areaWidth
        : qMax(areaWidth, label->minimumSizeHint().width()));

    const auto color = (style.testFlag(kErrorStyle)
        ? qnNxStyle->mainColor(QnNxStyle::Colors::kRed)
        : qnNxStyle->mainColor(QnNxStyle::Colors::kContrast));
    setPaletteColor(label, QPalette::WindowText, color);
}

} // namespace

QnStatusOverlayWidget::QnStatusOverlayWidget(QGraphicsWidget* parent):
    base_type(parent),

    m_visibleControls(Control::kNoControl),
    m_initialized(false),
    m_errorStyle(false),

    m_preloaderHolder(new QnViewportBoundWidget(this)),
    m_centralHolder(new QnViewportBoundWidget(this)),
    m_extrasHolder(new QnViewportBoundWidget(this)),

    m_preloader(new QnBusyIndicatorGraphicsWidget()),

    m_imageItem(new QGraphicsPixmapItem(this)),

    m_centralAreaImage(new QLabel()),
    m_caption(new QLabel()),
    m_description(new QLabel()),

    m_button(new QPushButton())
{
    setAutoFillBackground(true);

    makeTransparentForMouse(this);

    connect(this, &GraphicsWidget::geometryChanged, this, &QnStatusOverlayWidget::updateAreasSizes);
    connect(this, &GraphicsWidget::scaleChanged, this, &QnStatusOverlayWidget::updateAreasSizes);
    connect(m_button, &QPushButton::clicked, this, &QnStatusOverlayWidget::actionButtonClicked);

    setupPreloader();
    setupCentralControls();
    setupExtrasControls();
}

void QnStatusOverlayWidget::setVisibleControls(Controls controls)
{
    if (m_visibleControls == controls)
        return;

    const bool preloaderVisible = controls.testFlag(Control::kPreloader);
    const bool imageOverlayVisible = controls.testFlag(Control::kImageOverlay);

    const bool iconVisible = controls.testFlag(Control::kIcon);
    const bool captionVisible = controls.testFlag(Control::kCaption);
    const bool descriptionVisible = controls.testFlag(Control::kDescription);
    const bool centralVisible = (iconVisible || captionVisible || descriptionVisible);

    const bool extrasVisible = controls.testFlag(Control::kButton);

    m_imageItem.setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    m_preloaderHolder->setVisible(preloaderVisible);
    m_imageItem.setVisible(!preloaderVisible && imageOverlayVisible);
    m_centralHolder->setVisible(!preloaderVisible && !imageOverlayVisible && centralVisible);
    m_extrasHolder->setVisible(!preloaderVisible && !imageOverlayVisible && extrasVisible);

    m_centralAreaImage->setVisible(iconVisible);
    m_caption->setVisible(captionVisible);

    m_description->setVisible(descriptionVisible);

    m_visibleControls = controls;
    updateAreasSizes();
}

void QnStatusOverlayWidget::setIconOverlayPixmap(const QPixmap& pixmap)
{
    m_imageItem.setPixmap(pixmap);
    updateAreasSizes();
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

void QnStatusOverlayWidget::setErrorStyle(bool isErrorStyle)
{
    if (m_errorStyle == isErrorStyle)
        return;

    m_errorStyle = isErrorStyle;

    setupLabel(m_caption, getCaptionStyle(isErrorStyle));
    setupLabel(m_description, getDescriptionStyle(isErrorStyle));
    updateAreasSizes();
}

void QnStatusOverlayWidget::setCaption(const QString& caption)
{
    m_caption->setText(caption);
    setupLabel(m_caption, getCaptionStyle(m_errorStyle));
    updateAreasSizes();
}

void QnStatusOverlayWidget::setButtonText(const QString& text)
{
    m_button->setText(text);
    updateAreasSizes();
}

void QnStatusOverlayWidget::setDescription(const QString& description)
{
    m_description->setText(description);
    setupLabel(m_description, getDescriptionStyle(m_errorStyle));
    updateAreasSizes();
}

void QnStatusOverlayWidget::setupPreloader()
{
    m_preloader->setIndicatorColor(qnNxStyle->mainColor(QnNxStyle::Colors::kContrast).darker(6));
    m_preloader->setBorderColor(qnNxStyle->mainColor(QnNxStyle::Colors::kBase).darker(2));
    m_preloader->dots()->setDotRadius(8);
    m_preloader->dots()->setDotSpacing(8);

    const auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->addItem(m_preloader);

    m_preloaderHolder->setLayout(layout);
    makeTransparentForMouse(m_preloaderHolder);
}

void QnStatusOverlayWidget::setupCentralControls()
{
    m_centralAreaImage->setVisible(false);

    setupLabel(m_caption, getCaptionStyle(m_errorStyle));
    m_caption->setVisible(false);

    setupLabel(m_description, getDescriptionStyle(m_errorStyle));
    m_description->setVisible(false);

    const auto container = new QWidget();
    container->setObjectName(lit("centralContainer"));
    container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setPaletteColor(container, QPalette::Window, Qt::transparent);

    const auto layout = new QVBoxLayout(container);
    layout->setSpacing(0);

    layout->addWidget(m_centralAreaImage, 0, Qt::AlignHCenter);
    layout->addWidget(m_caption, 0, Qt::AlignHCenter);
    layout->addWidget(m_description, 0, Qt::AlignHCenter);

    const auto horizontalLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    horizontalLayout->addStretch(1);
    horizontalLayout->addItem(makeMaskedProxy(container, m_centralHolder, true));
    horizontalLayout->addStretch(1);

    const auto verticalLayout = new QGraphicsLinearLayout(Qt::Vertical, m_centralHolder);
    verticalLayout->setContentsMargins(16, 60, 16, 60);
    verticalLayout->addStretch(1);
    verticalLayout->addItem(horizontalLayout);
    verticalLayout->addStretch(1);

    m_centralHolder->setOpacity(0.7);
    makeTransparentForMouse(m_centralHolder);
}

void QnStatusOverlayWidget::setupExtrasControls()
{
    setupButton(*m_button);

    /* Even though there's only one button in the extras holder,
     * a container widget with a layout must be created, otherwise
     * graphics proxy doesn't handle size hint changes at all. */

    const auto container = new QWidget();
    container->setAttribute(Qt::WA_TranslucentBackground);
    container->setObjectName(lit("extrasContainer"));

    const auto layout = new QHBoxLayout(container);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_button);

    const auto horizontalLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    horizontalLayout->addStretch(1);
    horizontalLayout->addItem(makeMaskedProxy(container, m_extrasHolder, false));
    horizontalLayout->addStretch(1);

    const auto verticalLayout = new QGraphicsLinearLayout(Qt::Vertical, m_extrasHolder);
    verticalLayout->setContentsMargins(16, 0, 16, 16);
    verticalLayout->addStretch(1);
    verticalLayout->addItem(horizontalLayout);
    verticalLayout->addStretch(1);

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
        this, &QnStatusOverlayWidget::updateAreasSizes);

    m_initialized = true;
}

void QnStatusOverlayWidget::updateAreasSizes()
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
    const auto quater = (height * 0.25);

    QTransform sceneToViewport = view->viewportTransform();
    qreal scale = 1.0 / std::sqrt(sceneToViewport.m11() * sceneToViewport.m11() + sceneToViewport.m12() * sceneToViewport.m12());

    //TODO: #vkutin #ynikitenkov Localize visibility matters in ONE place!
    bool showExtras = m_visibleControls.testFlag(Control::kButton);

    const qreal minHeight = 95 * scale; // TODO: #ynikitenkov Change for description
    showExtras = showExtras && (rect.height() > minHeight); // Do not show extras on too small items

    qreal extrasHeight = 0.0;
    if (showExtras)
    {
        const qreal maxExtrasHeight = scale * 80;    // TODO: #ynikitnekov Change for description
        const qreal minExtrasHeight = scale * 24;

        if (quater > maxExtrasHeight)
            extrasHeight = maxExtrasHeight;
        else if (quater < minExtrasHeight)
            extrasHeight = minExtrasHeight;
        else
            extrasHeight = quater;
    }

    m_preloaderHolder->setFixedSize(rect.size());

    m_centralHolder->setFixedSize(QSizeF(rect.width(), height - extrasHeight));

    const bool showCentralArea = (!m_visibleControls.testFlag(Control::kPreloader)
        && !m_visibleControls.testFlag(Control::kImageOverlay));

    m_extrasHolder->setVisible(showCentralArea && showExtras);
    m_extrasHolder->setPos(0, height - extrasHeight);
    m_extrasHolder->setFixedSize(QSizeF(rect.width(), extrasHeight));

    const QSizeF imageSize = m_imageItem.pixmap().isNull()
        ? QSizeF(0, 0)
        : QSizeF(m_imageItem.pixmap().size()) / m_imageItem.pixmap().devicePixelRatioF();
    auto imageSceneSize = imageSize * scale;
    const auto aspect = (imageSceneSize.isNull() || !imageSceneSize.height() || !imageSceneSize.width()
        ? 1
        : imageSceneSize.width() / imageSceneSize.height());

    const auto thirdWidth = rect.width() / 3;
    const auto thirdHeight = rect.height() / 3;

    if (imageSceneSize.width() > thirdWidth)
    {
        imageSceneSize.setWidth(thirdWidth);
        imageSceneSize.setHeight(thirdWidth / aspect);
    }

    if (imageSceneSize.height() >thirdHeight)
    {
        imageSceneSize.setHeight(thirdHeight);
        imageSceneSize.setWidth(thirdHeight * aspect);
    }
    m_imageItem.setScale(imageSceneSize.width() / imageSize.width());
    m_imageItem.setPos((rect.width() - imageSceneSize.width()) / 2,
        (rect.height() - imageSceneSize.height()) / 2);

    m_extrasHolder->updateScale();
    m_preloaderHolder->updateScale();
    m_centralHolder->updateScale();
}
