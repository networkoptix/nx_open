#ifndef QN_BUSINESS_RULES_DIALOG_H
#define QN_BUSINESS_RULES_DIALOG_H

#include <QtCore/QScopedPointer>

#include <QtGui/QDialog>

namespace Ui {
    class BusinessRulesDialog;
}

class QnBusinessRulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnBusinessRulesDialog(QWidget *parent = 0);
    virtual ~QnBusinessRulesDialog();

private:
    QScopedPointer<Ui::BusinessRulesDialog> ui;
    QPushButton *m_copyButton;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
