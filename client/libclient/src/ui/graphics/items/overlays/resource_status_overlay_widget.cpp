#include "resource_status_overlay_widget.h"

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

void makeTransparentForMouse(QGraphicsWidget* item)
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

void setupCaptionLabel(QLabel* label, bool isErrorStyle)
{
    auto font = label->font();
    font.setPixelSize(isErrorStyle ? 88 : 80);
    font.setWeight(QFont::Light);
    label->setFont(font);

    label->setAlignment(Qt::AlignCenter);
    label->setFixedWidth(960);
    label->setMinimumHeight(qMax(120, label->heightForWidth(label->width())));
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label->setWordWrap(true);
    label->setVisible(false);

    const auto color = isErrorStyle
        ? qnNxStyle->mainColor(QnNxStyle::Colors::kRed)
        : qnNxStyle->mainColor(QnNxStyle::Colors::kContrast);
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

    m_button(new QPushButton()),
    m_description(new QnWordWrappedLabel())
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
    const bool centralVisible = (iconVisible || captionVisible);

    const bool buttonVisible = controls.testFlag(Control::kButton);
    const bool descriptionVisible = controls.testFlag(Control::kDescription);
    const bool extrasVisible = (buttonVisible || descriptionVisible);

    m_imageItem.setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    m_preloaderHolder->setVisible(preloaderVisible);
    m_imageItem.setVisible(!preloaderVisible && imageOverlayVisible);
    m_centralHolder->setVisible(!preloaderVisible && !imageOverlayVisible && centralVisible);
    m_extrasHolder->setVisible(!preloaderVisible && !imageOverlayVisible && extrasVisible);

    m_centralAreaImage->setVisible(iconVisible);
    m_caption->setVisible(captionVisible);

    m_button->setVisible(buttonVisible);
    m_description->setVisible(descriptionVisible && !buttonVisible);

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
    const auto size = pixmap.size();
    m_centralAreaImage->setPixmap(pixmap);
    m_centralAreaImage->setFixedSize(size);
    m_centralAreaImage->setVisible(!pixmap.isNull());
}

void QnStatusOverlayWidget::setErrorStyle(bool isErrorStyle)
{
    if (m_errorStyle == isErrorStyle)
        return;

    m_errorStyle = isErrorStyle;

    setupCaptionLabel(m_caption, isErrorStyle);
    updateAreasSizes();
}

void QnStatusOverlayWidget::setCaption(const QString& caption)
{
    m_caption->setText(caption);
    setupCaptionLabel(m_caption, m_errorStyle);
    updateAreasSizes();
}

void QnStatusOverlayWidget::setButtonText(const QString& text)
{
    m_button->setText(text);
    m_button->adjustSize();
    updateAreasSizes();
}

void QnStatusOverlayWidget::setDescription(const QString& description)
{
    m_description->setText(description);
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
    setupCaptionLabel(m_caption, m_errorStyle);

    const auto container = new QWidget();
    setPaletteColor(container, QPalette::Window, Qt::transparent);

    const auto layout = new QVBoxLayout(container);
    layout->setSpacing(0);

    layout->addWidget(m_centralAreaImage, 0, Qt::AlignHCenter);
    layout->addWidget(m_caption, 0, Qt::AlignHCenter);

    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    const auto holderLayout = new QGraphicsLinearLayout(Qt::Vertical, m_centralHolder);
    holderLayout->setContentsMargins(16, 60, 16, 60);
    holderLayout->addStretch(1);
    holderLayout->addItem(makeMaskedProxy(container, m_centralHolder, true));
    holderLayout->addStretch(1);

    m_centralHolder->setOpacity(0.7);
    makeTransparentForMouse(m_centralHolder);
}

void QnStatusOverlayWidget::setupExtrasControls()
{
    setupButton(*m_button);
    m_button->setVisible(false);

    m_description->label()->setAlignment(Qt::AlignHCenter);
    m_description->setVisible(false);

    const auto layout = new QGraphicsLinearLayout(Qt::Vertical, m_extrasHolder);
    layout->setContentsMargins(16, 0, 16, 16);

    const auto buttonProxy = makeMaskedProxy(m_button, m_extrasHolder, false);
    layout->addItem(buttonProxy);
    layout->setAlignment(buttonProxy, Qt::AlignHCenter);

    const auto descriptionProxy = makeMaskedProxy(m_description, m_extrasHolder, true);
    layout->addItem(descriptionProxy);
    layout->setAlignment(descriptionProxy, Qt::AlignHCenter);

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

    bool showExtras = (m_visibleControls.testFlag(Control::kButton)
        || m_visibleControls.testFlag(Control::kDescription));

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

    const QSizeF imageSize = m_imageItem.pixmap().size();
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
