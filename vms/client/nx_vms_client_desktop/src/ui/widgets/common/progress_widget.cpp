#include "progress_widget.h"

#include <QtWidgets/QBoxLayout>

#include <ui/style/skin.h>
#include <ui/workaround/hidpi_workarounds.h>

QnProgressWidget::QnProgressWidget(QWidget *parent):
    QWidget(parent),
    m_img(new QLabel(this)),
    m_text(new QLabel(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);

    const char* gifName = devicePixelRatio() == 1 ? "legacy/loading.gif" : "legacy/loading@2x.gif";
    QMovie* movie = qnSkin->newMovie(gifName, this);
    QnHiDpiWorkarounds::setMovieToLabel(m_img, movie);

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


