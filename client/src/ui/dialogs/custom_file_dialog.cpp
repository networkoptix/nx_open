#include "custom_file_dialog.h"

#include <QtGui/QGridLayout>

QnCustomFileDialog::QnCustomFileDialog(QWidget *parent,
                                       const QString &caption,
                                       const QString &directory,
                                       const QString &filter) :
    base_type(parent, caption, directory, filter)
{
    setOption(QFileDialog::DontUseNativeDialog);
}


QnCustomFileDialog::~QnCustomFileDialog() {
}

void QnCustomFileDialog::addWidget(QWidget *widget) {
    QGridLayout * gl = dynamic_cast<QGridLayout*>(layout());
    if (gl)
    {
        int r = gl->rowCount();
        gl->addWidget(widget, r, 0, 1, gl->columnCount());
        gl->setRowStretch(r, 0);
    }
}
