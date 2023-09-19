// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "progress_widget.h"

#include <QtWidgets/QBoxLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <ui/workaround/hidpi_workarounds.h>

QnProgressWidget::QnProgressWidget(QWidget *parent):
    QWidget(parent),
    m_img(new QLabel(this)),
    m_text(new QLabel(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);

    QMovie* movie = qnSkin->newMovie("legacy/loading.gif", this);
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
