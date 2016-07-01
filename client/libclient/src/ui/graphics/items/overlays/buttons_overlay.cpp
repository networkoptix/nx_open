
#include "buttons_overlay.h"

#include <ui/common/palette.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_bar.h>

namespace
{
    GraphicsLabel *createGraphicsLabel()
    {
        auto label = new GraphicsLabel();
        label->setAcceptedMouseButtons(0);
        label->setPerformanceHint(GraphicsLabel::PixmapCaching);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        QFont f = label->font();
        f.setBold(true);
        label->setFont(f);
        return label;
    }

    GraphicsWidget *createGraphicsWidget(QGraphicsLayout* layout)
    {
        auto widget = new GraphicsWidget();
        widget->setLayout(layout);
        widget->setAcceptedMouseButtons(0);
        widget->setAutoFillBackground(true);

        static const QColor overlayBackgroundColor = QColor(0, 0, 0, 96); // TODO: #gdm #customization

        setPaletteColor(widget, QPalette::Window, overlayBackgroundColor);
        return widget;
    }


    QGraphicsLinearLayout *createGraphicsLayout(Qt::Orientation orientation)
    {
        auto layout = new QGraphicsLinearLayout(orientation);
        layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
        return layout;
    }
}

QnButtonsOverlay::QnButtonsOverlay(QGraphicsItem *parent)
    : QnViewportBoundWidget(parent)
    , m_leftButtonsPanel(new QnImageButtonBar(this))
    , m_rightButtonsPanel(new QnImageButtonBar(this))
    , m_nameLabel(createGraphicsLabel())
    , m_extraInfoLabel(createGraphicsLabel())
{
    setAcceptedMouseButtons(Qt::NoButton);

    static const QSizeF kButtonsSize(24, 24);
    m_leftButtonsPanel->setUniformButtonSize(kButtonsSize);
    m_rightButtonsPanel->setUniformButtonSize(kButtonsSize);

    enum { kLayoutSpacing = 2};

    auto mainLayout = createGraphicsLayout(Qt::Horizontal);
    mainLayout->setSpacing(kLayoutSpacing);
    mainLayout->addItem(leftButtonsBar());
    mainLayout->addItem(titleLabel());
    mainLayout->addItem(extraInfoLabel());
    mainLayout->addStretch();
    mainLayout->addItem(rightButtonsBar());

    auto headerWidget = createGraphicsWidget(mainLayout);

    auto spacerLayout = createGraphicsLayout(Qt::Vertical);
    spacerLayout->addItem(headerWidget);
    spacerLayout->addStretch();

    setLayout(spacerLayout);
}

QnButtonsOverlay::~QnButtonsOverlay()
{

}

void QnButtonsOverlay::setSimpleMode(bool isSimpleMode)
{
    const bool extraVisible = !isSimpleMode;

    m_extraInfoLabel->setVisible(extraVisible);
    m_rightButtonsPanel->setVisible(extraVisible);
}

QnImageButtonBar *QnButtonsOverlay::leftButtonsBar()
{
    return m_leftButtonsPanel;
}

QnImageButtonBar *QnButtonsOverlay::rightButtonsBar()
{
    return m_rightButtonsPanel;
}

GraphicsLabel *QnButtonsOverlay::titleLabel()
{
    return m_nameLabel;
}

GraphicsLabel *QnButtonsOverlay::extraInfoLabel()
{
    return m_extraInfoLabel;
}


