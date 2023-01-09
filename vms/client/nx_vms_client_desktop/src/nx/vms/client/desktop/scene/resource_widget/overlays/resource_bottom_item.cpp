// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_bottom_item.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <qt_graphics_items/graphics_label.h>

#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

static constexpr QColor kPauseBackgroundColor = QColor(194, 38, 38, 100);

ResourceBottomItem::ResourceBottomItem(QGraphicsItem* parent):
    base_type(parent),
    m_pauseButton(new QnHtmlTextItem()),
    m_positionAndRecording(new QnHtmlTextItem())
{
    setAcceptedMouseButtons(Qt::NoButton);

    QnHtmlTextItemOptions pauseOptions;
    pauseOptions.backgroundColor = kPauseBackgroundColor;
    pauseOptions.borderRadius = 2;
    pauseOptions.autosize = true;
    pauseOptions.vertPadding = 6;
    pauseOptions.horPadding = 7;

    auto pauseIcon = qnSkin->icon("item/pause_button.svg");
    m_pauseButton->setParent(this);
    m_pauseButton->setVisible(false);
    m_pauseButton->setAcceptedMouseButtons(Qt::NoButton);
    m_pauseButton->setObjectName("PauseButton");
    m_pauseButton->setToolTip(tr("video is paused"));
    m_pauseButton->setOptions(pauseOptions);
    m_pauseButton->setIcon(pauseIcon.pixmap(QSize(8, 8)));

    QnHtmlTextItemOptions positionOptions;
    // Default color will be replaced with proper color from QPalette::Window by another handler
    positionOptions.backgroundColor = QColor();
    positionOptions.horSpacing = 4;
    positionOptions.horPadding = 6;
    positionOptions.vertPadding = 4;
    positionOptions.borderRadius = 2;
    positionOptions.autosize = true;

    m_positionAndRecording->setVisible(true);
    m_positionAndRecording->setParent(this);
    m_positionAndRecording->setObjectName("IconButton");
    m_positionAndRecording->setOptions(positionOptions);

    auto mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(4.0, 4.0, 4.0, 10.0);
    mainLayout->addItem(m_pauseButton);
    mainLayout->addItem(m_positionAndRecording);

    setLayout(mainLayout);
}

ResourceBottomItem::~ResourceBottomItem()
{
}

QnHtmlTextItem* ResourceBottomItem::positionAndRecordingStatus() const
{
    return m_positionAndRecording;
}

QnHtmlTextItem* ResourceBottomItem::pauseButton() const
{
    return m_pauseButton;
}

void ResourceBottomItem::setVisibleButtons(int buttons)
{
    if (buttons & Qn::PauseButton)
    {
        m_pauseButton->setVisible(true);
        m_positionAndRecording->setIcon({});
        auto options = m_positionAndRecording->options();
        options.backgroundColor = kPauseBackgroundColor;
        m_positionAndRecording->setOptions(options);
    }
    else
    {
        m_pauseButton->setVisible(false);
        auto options = m_positionAndRecording->options();
        options.backgroundColor = QColor();
        m_positionAndRecording->setOptions(options);
    }

    if (!(buttons & Qn::RecordingStatusIconButton))
        m_positionAndRecording->setIcon({});
}

int ResourceBottomItem::visibleButtons()
{
    int buttons = isVisible() ? Qn::RecordingStatusIconButton : 0;
    if (m_pauseButton->isVisible())
        buttons = buttons & Qn::PauseButton;
    return buttons;
}

} // namespace nx::vms::client::desktop
