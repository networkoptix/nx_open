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

const auto kBigStretch = 1000;
const qreal kBaseOpacity = 0.75;

QGraphicsProxyWidget* makeMaskedProxy(QWidget* source)
{
    const auto result = new QnMaskedProxyWidget();
    result->setWidget(source);
    result->setAcceptDrops(false);
    return result;
}

void setupButton(QPushButton& button)
{
    static const auto kDefaultButtonHeight = 28.0;

    button.setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
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

    static const auto kTextColor = toTransparent(kBaseColor, 0.5 / kBaseOpacity).name(QColor::HexArgb);
    static const auto kHoveredTextColor = toTransparent(kBaseColor, 0.7 / kBaseOpacity).name(QColor::HexArgb);

    static const auto kBackground = toTransparent(kBaseColor, 0.1 / kBaseOpacity).name(QColor::HexArgb);
    static const auto kHovered = toTransparent(kBaseColor, 0.15 / kBaseOpacity).name(QColor::HexArgb);
    static const auto kPressed = toTransparent(kBaseColor, 0.1 / kBaseOpacity).name(QColor::HexArgb);

    const auto kStyleSheet = kStyleSheetTemplate.arg(kButtonName,
        kTextColor, kHoveredTextColor, kBackground, kHovered, kPressed);

    button.setStyleSheet(kStyleSheet);
}

void setupCaptionLabel(QnWordWrappedLabel& label, bool isErrorStyle)
{
    static const auto makeFont =
        [](int fontPixelSize)
        {
            QFont font;
            font.setPixelSize(fontPixelSize);
            font.setWeight(QFont::Light);
            return font;
        };

    static const auto kErrorFont = makeFont(88);
    static const auto kNormalFont = makeFont(80);
    static const auto kErrorLabelsize = 140;
    static const auto kNormalLabelSize = 64;
    static const auto kNormalColor = qnNxStyle->mainColor(QnNxStyle::Colors::kContrast);
    static const auto kErrorColor = qnNxStyle->mainColor(QnNxStyle::Colors::kRed);

    label.setFont(isErrorStyle ? kErrorFont : kNormalFont);
    label.setMinimumHeight(isErrorStyle ? kErrorLabelsize : kNormalLabelSize);
    label.label()->setAlignment(Qt::AlignHCenter);
    label.label()->setFixedWidth(960);
    label.label()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    label.label()->setWordWrap(true);
    label.setVisible(false);

    setPaletteColor(&label, QPalette::WindowText,
        (isErrorStyle ? kErrorColor : kNormalColor));
}

}

QnStatusOverlayWidget::QnStatusOverlayWidget(QGraphicsWidget *parent)
    :
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
    m_caption(new QnWordWrappedLabel()),

    m_button(new QPushButton()),
    m_description(new QnWordWrappedLabel())
{
    setOpacity(kBaseOpacity);
    setAcceptedMouseButtons(Qt::NoButton);

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

    setupCaptionLabel(*m_caption, isErrorStyle);
    updateAreasSizes();
}

void QnStatusOverlayWidget::setCaption(const QString& caption)
{
    m_caption->setText(caption);
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
    updateAreasSizes();
}

void QnStatusOverlayWidget::setupPreloader()
{
    m_preloader->setIndicatorColor(qnNxStyle->mainColor(QnNxStyle::Colors::kContrast).darker(6));
    m_preloader->setBorderColor(qnNxStyle->mainColor(QnNxStyle::Colors::kBase).darker(2));
    m_preloader->setDotRadius(8);
    m_preloader->setDotSpacing(8);

    const auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->addItem(m_preloader);

    m_preloaderHolder->setLayout(layout);
    m_preloaderHolder->setAcceptedMouseButtons(Qt::NoButton);
}

void QnStatusOverlayWidget::setupCentralControls()
{
    m_centralAreaImage->setAlignment(Qt::AlignHCenter);
    m_centralAreaImage->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_centralAreaImage->setScaledContents(true);
    m_centralAreaImage->setVisible(false);

    setupCaptionLabel(*m_caption, m_errorStyle);

    //

    const auto centralLayout = new QVBoxLayout();
    centralLayout->addStretch(kBigStretch);
    centralLayout->addSpacing(60);
    centralLayout->addWidget(m_centralAreaImage);
    centralLayout->addWidget(m_caption);
    centralLayout->addSpacing(60);
    centralLayout->addStretch(kBigStretch);

    centralLayout->setAlignment(m_centralAreaImage, Qt::AlignCenter);
    centralLayout->setAlignment(m_caption, Qt::AlignCenter);

    //

    const auto holder = new QWidget();
    holder->setLayout(centralLayout);
    const auto proxy = makeMaskedProxy(holder);

    const auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->addItem(proxy);
    layout->setAlignment(proxy, Qt::AlignCenter);

    m_centralHolder->setLayout(layout);
    m_centralHolder->setAcceptedMouseButtons(Qt::NoButton);
}

void QnStatusOverlayWidget::setupExtrasControls()
{
    setupButton(*m_button);
    m_button->setVisible(false);

    m_description->label()->setAlignment(Qt::AlignHCenter);
    m_description->setVisible(false);

    //
    const auto layout = new QGraphicsLinearLayout(Qt::Vertical);

    // Workaround for incorrect behavior of QGraphicsLinearLayout and QGraphicsProxyWidget
    const auto workaroundProxy = new QWidget();
    const auto workaroundLayout = new QVBoxLayout();
    workaroundProxy->setLayout(workaroundLayout);
    workaroundLayout->addWidget(m_button, 0, Qt::AlignHCenter | Qt::AlignTop);
    workaroundLayout->addStretch(kBigStretch);
    workaroundLayout->setContentsMargins(16, 0, 16, 16);

    const auto buttonProxy = makeMaskedProxy(workaroundProxy);
    layout->addItem(buttonProxy);
    layout->setAlignment(buttonProxy, Qt::AlignHCenter | Qt::AlignTop);

    const auto descriptionProxy = makeMaskedProxy(m_description);
    layout->addItem(descriptionProxy);
    layout->setAlignment(descriptionProxy, Qt::AlignHCenter | Qt::AlignTop);

    m_extrasHolder->setLayout(layout);
    m_extrasHolder->setAcceptedMouseButtons(Qt::NoButton);
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
    if (showExtras && showExtras)
    {
        const qreal maxExtrasHeight = scale * 80;    // TODO: #ynikitnekov Change for description
        const qreal minExtrasHeight = scale * (32 + 8);

        if (quater > maxExtrasHeight)
            extrasHeight = maxExtrasHeight;
        else if (quater < minExtrasHeight)
            extrasHeight = minExtrasHeight;
        else
            extrasHeight = quater;
    }

    m_preloaderHolder->setPos(0, 0);
    m_preloaderHolder->setFixedSize(rect.size());

    m_centralHolder->setPos(0, 0);
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
}
