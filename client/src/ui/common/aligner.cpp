#include "aligner.h"

QnAligner::QnAligner(QObject* parent /*= nullptr*/):
    base_type(parent)
{

}

QnAligner::~QnAligner()
{

}

void QnAligner::addWidget(QWidget* widget)
{
    m_widgets << widget;
}

void QnAligner::start()
{
    align();
}

void QnAligner::align()
{
    if (m_widgets.isEmpty())
        return;

    auto widest = std::max_element(m_widgets.cbegin(), m_widgets.cend(), [](QWidget* left, QWidget* right)
    {
        return left->sizeHint().width() < right->sizeHint().width();
    });
    int width = (*widest)->sizeHint().width();

    for (QWidget* w : m_widgets)
        w->setFixedWidth(width);
}
