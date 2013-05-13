#include "custom_file_dialog.h"

#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>

QnCustomFileDialog::QnCustomFileDialog(QWidget *parent,
                                       const QString &caption,
                                       const QString &directory,
                                       const QString &filter) :
    base_type(parent, caption, directory, filter)
{
    setOption(QFileDialog::DontUseNativeDialog);
    connect(this, SIGNAL(accepted()), this, SLOT(at_accepted()));

}


QnCustomFileDialog::~QnCustomFileDialog() {
}

void QnCustomFileDialog::addCheckbox(const QString &text, bool *value, QnCheckboxControlAbstractDelegate* delegate) {
    QCheckBox* checkbox = new QCheckBox(this);
    checkbox->setText(text);
    checkbox->setChecked(*value);
    m_checkboxes.insert(checkbox, value);
    addWidget(checkbox);

    if (delegate) {
        delegate->setCheckbox(checkbox);
        connect(this, SIGNAL(filterSelected(QString)), delegate, SLOT(at_filterSelected(QString)));
    }
}

void QnCustomFileDialog::addSpinBox(const QString &text, int minValue, int maxValue, int *value) {

    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* label = new QLabel(widget);
    label->setText(text);
    layout->addWidget(label);

    QSpinBox* spinbox = new QSpinBox(widget);
    spinbox->setMinimum(minValue);
    spinbox->setMaximum(maxValue);
    spinbox->setValue(*value);
    m_spinboxes.insert(spinbox, value);
    layout->addWidget(spinbox);
    layout->addStretch();

    widget->setLayout(layout);

    addWidget(widget);
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

void QnCustomFileDialog::at_accepted() {
    foreach(QCheckBox* key, m_checkboxes.keys()) {
        *m_checkboxes[key] = key->isChecked();
    }

    foreach(QSpinBox* key, m_spinboxes.keys()) {
        *m_spinboxes[key] = key->value();
    }
}
