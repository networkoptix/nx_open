#include "custom_file_dialog.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include <nx/utils/string.h>

// TODO: #GDM include and use <ui/workaround/cancel_drag.h>
QnCustomFileDialog::QnCustomFileDialog(QWidget *parent, const QString &caption, const QString &directory, const QString &filter):
    base_type(parent, caption, directory, filter),
    m_currentColumn(0)
{
    setOptions(QnCustomFileDialog::fileDialogOptions());
    connect(this, SIGNAL(accepted()), this, SLOT(at_accepted()));
}

QnCustomFileDialog::~QnCustomFileDialog() {
    return;
}

void QnCustomFileDialog::addCheckBox(const QString &text, bool *value, QnAbstractWidgetControlDelegate* delegate) {
    QCheckBox* checkbox = new QCheckBox(this);
    checkbox->setText(text);
    checkbox->setChecked(*value);
    m_checkBoxes.insert(checkbox, value);
    addWidget(QString(), checkbox, delegate);
}

void QnCustomFileDialog::addSpinBox(const QString &text, int minValue, int maxValue, int *value) {
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    int index = text.indexOf(valueSpacer());
    QString prefix = text.mid(0, index).trimmed();
    QString postfix = index >= 0 ? text.mid(index + valueSpacer().length()).trimmed() : QString();

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

    addWidget(QString(), widget, NULL);
}

void QnCustomFileDialog::addLineEdit(const QString &label, QString *value) {
    QLineEdit *lineEdit = new QLineEdit(this);
    lineEdit->setText(*value);
    m_lineEdits.insert(lineEdit, value);

    addWidget(label, lineEdit, NULL);
}

void QnCustomFileDialog::addWidget(const QString &label, QWidget *widget, QnAbstractWidgetControlDelegate *delegate) {
    QGridLayout *layout = customizedLayout();
    NX_ASSERT(layout);

    int row = layout->rowCount();
    if(label.isEmpty()) {
        layout->addWidget(widget, row, 0, 1, 2);
    } else {
        layout->addWidget(new QLabel(label, this), row, 0);
        layout->addWidget(widget, row, 1);
    }
    widget->raise();

    if (delegate) {
        delegate->addWidget(widget);
        delegate->disconnect();
        connect(this, &QnSystemBasedCustomDialog::filterSelected, delegate, &QnAbstractWidgetControlDelegate::updateWidget);
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

QString QnCustomFileDialog::selectedExtension() const {
    return nx::utils::extractFileExtension(selectedNameFilter());
}
