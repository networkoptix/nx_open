#include "timeline_placeholder.h"
#include "time_slider.h"

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

    QFont font;
    font.setPixelSize(26);
    font.setWeight(QFont::Light);
    setFont(font);
    setAlignment(Qt::AlignCenter);
};
