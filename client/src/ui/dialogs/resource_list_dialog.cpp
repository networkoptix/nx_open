#include "resource_list_dialog.h"

#include <QtGui/QLabel>
#include <QtGui/QTableWidget>
#include <QtGui/QVBoxLayout>

QnResourceListDialog::QnResourceListDialog(QWidget *parent):
    QnButtonBoxDialog(parent)
{
    m_label = new QLabel(this);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(2);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    setButtonBox(m_buttonBox);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    layout->addWidget(m_tableWidget);
    layout->addWidget(m_buttonBox);
}

QnResourceListDialog::~QnResourceListDialog() {
    return;
}

void QnResourceListDialog::setText(const QString &text) {
    m_label->setText(text);
}

QString QnResourceListDialog::text() const {
    return m_label->text();
}

void QnResourceListDialog::setStandardButtons(QDialogButtonBox::StandardButtons standardButtons) {
    m_buttonBox->setStandardButtons(standardButtons);
}

QDialogButtonBox::StandardButtons QnResourceListDialog::standardButtons() const {
    return m_buttonBox->standardButtons();
}

void QnResourceListDialog::setResources(const QnResourceList &resources) {
    
}

const QnResourceList &QnResourceListDialog::resources() const {
    return m_resources;
}
