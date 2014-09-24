#include "dialog_button_box.h"

#include <QtGui/QMovie>

#include <QtWidgets/QLayout>
#include <QtWidgets/QHBoxLayout>

#include <ui/widgets/progress_widget.h>

#include <ui/style/skin.h>

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
