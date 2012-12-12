#ifndef QN_BUSINESS_RULES_DIALOG_H
#define QN_BUSINESS_RULES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QDialog>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>

namespace Ui {
    class BusinessRulesDialog;
}

class QnBusinessRulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent = 0);
    virtual ~QnBusinessRulesDialog();

private slots:
    void at_newRuleButton_clicked();
    void at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle);

private:
    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QnAppServerConnectionPtr m_connection;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
