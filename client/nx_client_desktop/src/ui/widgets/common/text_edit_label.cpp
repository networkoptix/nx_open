#include "text_edit_label.h"

#include <ui/style/helper.h>

QnTextEditLabel::QnTextEditLabel(QWidget* parent) :
    base_type(parent),
    m_documentSize(0, 0)
{
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    setReadOnly(true);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFocusPolicy(Qt::NoFocus);
    viewport()->unsetCursor();
    document()->setDocumentMargin(0.0);

    setProperty(style::Properties::kDontPolishFontProperty, true);

    connect(document(), &QTextDocument::contentsChanged, document(),
        [this]()
        {
            if (!isAutoWrapped())
                document()->adjustSize();
        });

    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
        [this](const QSizeF& size)
        {
            auto newSize = size.toSize();
            if (m_documentSize == newSize)
                return;

            m_documentSize = newSize;
            updateGeometry();
        });
}

bool QnTextEditLabel::isAutoWrapped() const
{
    switch (wordWrapMode())
    {
        case QTextOption::WordWrap:
        case QTextOption::WrapAnywhere:
        case QTextOption::WrapAtWordBoundaryOrAnywhere:
            return true;

        default:
            return false;
    }
}

QSize QnTextEditLabel::sizeHint() const
{
    return m_documentSize;
}

QSize QnTextEditLabel::minimumSizeHint() const
{
    return QSize(0, m_documentSize.height());
}
