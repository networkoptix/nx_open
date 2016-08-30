#include "text_edit_label.h"

#include <ui/style/helper.h>

QnTextEditLabel::QnTextEditLabel(QWidget* parent) :
    base_type(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setReadOnly(true);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFocusPolicy(Qt::NoFocus);
    viewport()->unsetCursor();
    document()->setDocumentMargin(0.0);

    setProperty(style::Properties::kDontPolishFontProperty, true);

    /* QTextEdit doesn't have precise vertical size hint, therefore we have to adjust maximum vertical size. */
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
        [this](const QSizeF& size)
        {
            setMaximumHeight(size.height());
        });
}
