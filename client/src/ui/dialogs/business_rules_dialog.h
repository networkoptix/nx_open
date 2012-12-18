#ifndef QN_BUSINESS_RULES_DIALOG_H
#define QN_BUSINESS_RULES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QDialog>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <events/business_event_rule.h>
#include <ui/widgets/business/business_rule_widget.h>
#include <utils/common/request_param.h>

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
    void at_resources_deleted(const QnHTTPRawResponse& response, int handle);

    void newRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);
    void saveRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);
    void deleteRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);
private:
    void addRuleToList(QnBusinessEventRulePtr rule);

    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QnAppServerConnectionPtr m_connection;
    QMap<int, QnBusinessRuleWidget*> m_savingWidgets;
    QMap<int, QnBusinessRuleWidget*> m_deletingWidgets;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
