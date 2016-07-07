#include "progress_widget.h"

#include <ui/style/skin.h>

QnProgressWidget::QnProgressWidget(QWidget *parent):
    QWidget(parent),
    m_img(new QLabel(this)),
    m_text(new QLabel(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);

    QMovie* movie = qnSkin->newMovie("loading.gif", this);
    m_img->setMovie(movie);
    if (movie->loopCount() >= 0)
        connect(movie, &QMovie::finished, movie, &QMovie::start);
    movie->start();

    layout->addWidget(m_img);
    layout->addWidget(m_text);
    m_text->setVisible(false);
    setLayout(layout);
}

void QnProgressWidget::setText(const QString &text) {
    m_text->setText(text);
    m_text->setVisible(!text.isEmpty());
}


