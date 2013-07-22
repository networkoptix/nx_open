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

void QnCustomFileDialog::addCheckBox(const QString &text, bool *value, QnCheckboxControlAbstractDelegate* delegate) {
    QCheckBox* checkbox = new QCheckBox(this);
    checkbox->setText(text);
    checkbox->setChecked(*value);
    m_checkBoxes.insert(checkbox, value);
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

    int index = text.indexOf(QLatin1String("%n"));
    QString prefix = text.mid(0, index).trimmed();
    QString postfix = index >= 0 ? text.mid(index + 2).trimmed() : QString();

    QLabel* labelPrefix = new QLabel(widget);
    labelPrefix->setText(prefix);
    layout->addWidget(labelPrefix);

    QSpinBox* spinbox = new QSpinBox(widget);
    spinbox->setMinimum(minValue);
    spinbox->setMaximum(maxValue);
    spinbox->setValue(*value);
    m_spinBoxes.insert(spinbox, value);
    layout->addWidget(spinbox);

    if (!postfix.isEmpty()) {
        QLabel* labelPostfix = new QLabel(widget);
        labelPostfix->setText(postfix);
        layout->addWidget(labelPostfix);
    }

    layout->addStretch();

    widget->setLayout(layout);

    addWidget(widget);
}


void QnCustomFileDialog::addLineEdit(const QString &text, QString *value) {

    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* label = new QLabel(widget);
    label->setText(text);
    layout->addWidget(label);

    QLineEdit* edit = new QLineEdit(widget);
    edit->setText(*value);
    m_lineEdits.insert(edit, value);
    layout->addWidget(edit);

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
    foreach(QCheckBox* key, m_checkBoxes.keys())
        *m_checkBoxes[key] = key->isChecked();

    foreach(QSpinBox* key, m_spinBoxes.keys())
        *m_spinBoxes[key] = key->value();

    foreach(QLineEdit* key, m_lineEdits.keys())
        *m_lineEdits[key] = key->text();
}
