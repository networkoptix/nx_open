#include "dialog_button_box.h"

#include <QtGui/QMovie>

#include <QtWidgets/QLayout>
#include <QtWidgets/QHBoxLayout>

#include <ui/style/skin.h>

QnProgressWidget::QnProgressWidget(QWidget *parent):
    QWidget(parent),
    m_img(new QLabel(this)),
    m_text(new QLabel(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    QMovie* movie = qnSkin->loadMovie("loading.gif", this);
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


QnDialogButtonBox::QnDialogButtonBox(QWidget *parent):
    base_type(parent) {}

QnDialogButtonBox::QnDialogButtonBox(Qt::Orientation orientation, QWidget *parent):
    base_type(orientation, parent) {}

QnDialogButtonBox::QnDialogButtonBox(StandardButtons buttons, QWidget *parent):
    base_type(buttons, parent) {}

QnDialogButtonBox::QnDialogButtonBox(StandardButtons buttons, Qt::Orientation orientation, QWidget *parent):
    base_type(buttons, orientation, parent) {}

QnDialogButtonBox::~QnDialogButtonBox() {}

void QnDialogButtonBox::showProgress(const QString &text) {
    QnProgressWidget* widget = progressWidget(true);
    widget->setText(text);
    widget->show();
}

void QnDialogButtonBox::hideProgress() {
    QnProgressWidget* widget = progressWidget(false);
    if (widget)
        widget->hide();
}

QnProgressWidget* QnDialogButtonBox::progressWidget(bool canCreate) {
    QBoxLayout* layout = dynamic_cast<QBoxLayout*>(this->layout());

    if (layout->count() == 0) {
        if (!canCreate)
            return NULL;
        QnProgressWidget* result = new QnProgressWidget(this);
        layout->insertWidget(0, result);
        return result;
    }

    QnProgressWidget* result = dynamic_cast<QnProgressWidget*>(layout->itemAt(0)->widget());
    if (!result && canCreate) {
        result = new QnProgressWidget(this);
        layout->insertWidget(0, result);
    }
    return result;
}
