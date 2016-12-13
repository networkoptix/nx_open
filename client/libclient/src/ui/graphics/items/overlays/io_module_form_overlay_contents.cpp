#include "io_module_form_overlay_contents.h"

#include <array>

#include <ui/common/palette.h>
#include <ui/style/skin.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/standard/graphics_pixmap.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <utils/common/connective.h>

namespace {

constexpr int kInputFontPixelSize = 15;
constexpr int kOutputFontPixelSize = 12;
constexpr int kPortFontPixelSize = 12;
constexpr int kMaxPortNameLength = 20;
constexpr int kInputsOutputsSpacing = 48;


class IndicatorWidget: public GraphicsPixmap
{
    QPixmap m_onPixmap;
    QPixmap m_offPixmap;
    bool m_isOn;

public:
    IndicatorWidget(const QPixmap& onPixmap, const QPixmap& offPixmap, QGraphicsItem* parent = nullptr):
        GraphicsPixmap(parent),
        m_onPixmap(onPixmap),
        m_offPixmap(offPixmap),
        m_isOn(false)
    {
        setPixmap(m_offPixmap);
    }

    bool isOn() const
    {
        return m_isOn;
    }

    void setOn(bool on)
    {
        if (on == m_isOn)
            return;

        m_isOn = on;
        setPixmap(m_isOn ? m_onPixmap : m_offPixmap);
    }
};

} // namespace

class QnIoModuleFormOverlayContentsPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;

    Q_DISABLE_COPY(QnIoModuleFormOverlayContentsPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleFormOverlayContents)

public:
    QnIoModuleFormOverlayContents* const q_ptr;

    QGraphicsGridLayout* controlsLayout;
    QPixmap indicatorOnPixmap;
    QPixmap indicatorOffPixmap;

    struct PortControls
    {
        GraphicsLabel* id = nullptr;
        IndicatorWidget* indicator = nullptr;
        GraphicsLabel* label = nullptr;
        QPushButton* button = nullptr;
    };

    int inputsCount = 0;
    int outputsCount = 0;
    QHash<QString, PortControls> controlsById;

    enum Column
    {
        kInputName,
        kInputState,
        kInputCaption,
        kSpacer,
        kOutputName,
        kOutputState,
        kOutputCaption,

        kColumnCount
    };

    QnIoModuleFormOverlayContentsPrivate(QnIoModuleFormOverlayContents* main);

    QnIoModuleOverlayWidget* widget() const;

    void portsChanged(const QnIOPortDataList& ports);
    void stateChanged(const QnIOPortData& port, const QnIOStateData& state);

    PortControls createControls(const QnIOPortData& config);

    GraphicsLabel* createId(int row, int column);
    IndicatorWidget* createIndicator(int row, int column);
    GraphicsLabel* createLabel(int row, int column);
    QPushButton* createButton(int row, int column, const QString& port);
    void insertToLayout(QGraphicsWidget* item, int row, int column);
};

QnIoModuleFormOverlayContentsPrivate::QnIoModuleFormOverlayContentsPrivate(QnIoModuleFormOverlayContents* main):
    base_type(main),
    q_ptr(main),
    controlsLayout(new QGraphicsGridLayout()),
    indicatorOnPixmap(qnSkin->pixmap("legacy/io_indicator_on.png")),
    indicatorOffPixmap(qnSkin->pixmap("legacy/io_indicator_off.png"))
{
    enum
    {
        kOuterMargin = 24,
        kTopMargin = kOuterMargin * 2
    };

    controlsLayout->setContentsMargins(kOuterMargin, kTopMargin, kOuterMargin, kOuterMargin);

    static const std::array<Qt::AlignmentFlag, kColumnCount> kColumnAlignments
    {
        Qt::AlignRight,    // Input name
        Qt::AlignCenter,   // Input state
        Qt::AlignLeft,     // Input caption

        Qt::AlignCenter,   // Spacer

        Qt::AlignRight,    // Output name
        Qt::AlignCenter,   // Output state
        Qt::AlignLeft      // Output caption
    };

    for (int i = 0; i < kColumnCount; ++i)
        controlsLayout->setColumnAlignment(i, kColumnAlignments[i]);

    enum { kVeryBigStretch = 1000 };
    QGraphicsLinearLayout* const v = new QGraphicsLinearLayout(Qt::Vertical);
    v->addItem(controlsLayout);
    v->addStretch(kVeryBigStretch);

    QGraphicsLinearLayout* const h = new QGraphicsLinearLayout(Qt::Horizontal);
    h->addItem(v);
    h->addStretch(kVeryBigStretch);

    widget()->setLayout(h);
}

QnIoModuleOverlayWidget* QnIoModuleFormOverlayContentsPrivate::widget() const
{
    Q_Q(const QnIoModuleFormOverlayContents);
    return q->widget();
}

QnIoModuleFormOverlayContentsPrivate::PortControls
    QnIoModuleFormOverlayContentsPrivate::createControls(const QnIOPortData& config)
{
    PortControls result;
    bool isOutput = config.portType == Qn::PT_Output;
    if (!isOutput && config.portType != Qn::PT_Input)
        return result;

    /* Create new controls: */
    if (isOutput)
    {
        result.id = createId(outputsCount, kOutputName);
        result.indicator = createIndicator(outputsCount, kOutputState);
        result.button = createButton(outputsCount, kOutputCaption, config.id);
        ++outputsCount;
    }
    else
    {
        result.id = createId(inputsCount, kInputName);
        result.indicator = createIndicator(inputsCount, kInputState);
        result.label = createLabel(inputsCount, kInputCaption);
        ++inputsCount;
    }

    result.id->setText(config.id);
    return result;
}

GraphicsLabel* QnIoModuleFormOverlayContentsPrivate::createId(int row, int column)
{
    QFont font;
    font.setPixelSize(kPortFontPixelSize);

    auto portIdLabel = new GraphicsLabel(widget());
    portIdLabel->setFont(font);
    portIdLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    portIdLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    setPaletteColor(portIdLabel, QPalette::WindowText, widget()->colors().idLabel);

    insertToLayout(portIdLabel, row, column);
    return portIdLabel;
}

IndicatorWidget* QnIoModuleFormOverlayContentsPrivate::createIndicator(int row, int column)
{
    auto indicator = new IndicatorWidget(indicatorOnPixmap, indicatorOffPixmap, widget());
    insertToLayout(indicator, row, column);
    return indicator;
}

GraphicsLabel* QnIoModuleFormOverlayContentsPrivate::createLabel(int row, int column)
{
    QFont font;
    font.setPixelSize(kInputFontPixelSize);

    auto descriptionLabel = new GraphicsLabel(widget());
    descriptionLabel->setFont(font);
    descriptionLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    descriptionLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    insertToLayout(descriptionLabel, row, column);
    return descriptionLabel;
}

QPushButton* QnIoModuleFormOverlayContentsPrivate::createButton(int row, int column, const QString& port)
{
    QFont font;
    font.setPixelSize(kOutputFontPixelSize);

    QPushButton* button = new QPushButton();
    button->setFont(font);

    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    button->setAutoDefault(false);

    setPaletteColor(button, QPalette::Window, widget()->colors().buttonBackground);

    connect(button, &QPushButton::clicked, this,
        [this, port]()
        {
            Q_Q(QnIoModuleFormOverlayContents);
            q->userClicked(port);
        });

    auto buttonProxy = new QnMaskedProxyWidget(widget());
    buttonProxy->setWidget(button);

    insertToLayout(buttonProxy, row, column);
    return button;
}

void QnIoModuleFormOverlayContentsPrivate::insertToLayout(QGraphicsWidget* item, int row, int column)
{
    controlsLayout->addItem(item, row, column);
}

void QnIoModuleFormOverlayContentsPrivate::portsChanged(const QnIOPortDataList& ports)
{
    /* Reset internal data: */
    inputsCount = outputsCount = 0;
    controlsById.clear();

    /* Clear layout: */
    while (controlsLayout->count())
    {
        QScopedPointer<QGraphicsLayoutItem> item(controlsLayout->itemAt(0));
        controlsLayout->removeAt(0);
    }

    /* Create new controls: */
    for (const auto& port : ports)
    {
        auto controls = createControls(port);
        controlsById[port.id] = controls;

        if (!controls.id)
            continue;

        if (controls.label)
        {
            QFontMetrics metrics(controls.label->font());
            int maxWidth = metrics.width(QString(kMaxPortNameLength, L'w'));
            controls.label->setText(metrics.elidedText(port.inputName, Qt::ElideRight, maxWidth));
        }

        if (controls.button)
        {
            const auto& metrics = controls.button->fontMetrics();
            int maxWidth = metrics.width(QString(kMaxPortNameLength, L'w'));
            controls.button->setText(metrics.elidedText(port.outputName, Qt::ElideRight, maxWidth));
            controls.button->setEnabled(widget()->userInputEnabled());
        }
    }

    /* Configure 2- or 1-column layout: */
    controlsLayout->setColumnFixedWidth(kSpacer, inputsCount > 0 ? kInputsOutputsSpacing : 0);
}

void QnIoModuleFormOverlayContentsPrivate::stateChanged(const QnIOPortData& port, const QnIOStateData& state)
{
    Q_UNUSED(port); //< unused at the moment

    auto controls = controlsById[state.id];

    if (controls.indicator)
        controls.indicator->setOn(state.isActive);
}

QnIoModuleFormOverlayContents::QnIoModuleFormOverlayContents(QnIoModuleOverlayWidget* widget):
    base_type(widget),
    d_ptr(new QnIoModuleFormOverlayContentsPrivate(this))
{
}

QnIoModuleFormOverlayContents::~QnIoModuleFormOverlayContents()
{
}

void QnIoModuleFormOverlayContents::portsChanged(const QnIOPortDataList& ports)
{
    Q_D(QnIoModuleFormOverlayContents);
    d->portsChanged(ports);
}

void QnIoModuleFormOverlayContents::stateChanged(const QnIOPortData& port, const QnIOStateData& state)
{
    Q_D(QnIoModuleFormOverlayContents);
    d->stateChanged(port, state);
}
