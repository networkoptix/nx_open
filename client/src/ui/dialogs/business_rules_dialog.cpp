#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtCore/QEvent>

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QApplication>

QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent): 
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog())
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}





