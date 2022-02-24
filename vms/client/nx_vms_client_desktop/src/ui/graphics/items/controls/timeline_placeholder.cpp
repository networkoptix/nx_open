// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_placeholder.h"
#include "time_slider.h"

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include <nx/utils/log/assert.h>

using namespace nx::vms::client::desktop;

QnTimelinePlaceholder::QnTimelinePlaceholder(QGraphicsItem* parent, QnTimeSlider* slider) : base_type(parent)
{
    NX_ASSERT(slider);

    if (slider->lineCount() > 0)
        setText(slider->lineComment(0));

    connect(slider, &QnTimeSlider::lineCommentChanged, this,
        [this](int line, const QString& comment)
        {
            if (line == 0)
                setText(comment);
        });

    setPaletteColor(this, QPalette::WindowText, colorTheme()->color("dark16"));

    QFont font;
    font.setPixelSize(26);
    font.setWeight(QFont::Light);
    setFont(font);
    setAlignment(Qt::AlignCenter);
};
