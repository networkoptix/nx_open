#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtGui/QMessageBox>

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>

#include <ui/style/resource_icon_cache.h>

#include <utils/settings.h>

namespace {
//TODO: tr
    static QLatin1String prolongedEvent("While %1");
    static QLatin1String instantEvent("On %1 %2");

    QString toggleStateToString(ToggleState::Value state) {
        switch (state) {
        case ToggleState::On: return QObject::tr("start");
        case ToggleState::Off: return QObject::tr("stop");
        default: return QObject::tr("start/stop");
        }
        return QString();
    }

    QString eventTypeString(const QnBusinessEventRulePtr &rule){
        QString typeStr = BusinessEventType::toString(rule->eventType());
        if (BusinessActionType::hasToggleState(rule->actionType()))
            return QString(prolongedEvent).arg(typeStr);
        else
            return QString(instantEvent).arg(typeStr)
                    .arg(toggleStateToString(BusinessEventParameters::getToggleState(rule->eventParams())));
    }

    QString extractHost(const QString &url) {
        /* Try it as a host address first. */
        QHostAddress hostAddress(url);
        if(!hostAddress.isNull())
            return hostAddress.toString();

        /* Then go default QUrl route. */
        return QUrl(url).host();
    }

    QString getResourceName(const QnResourcePtr& resource) {
        if (!resource)
            return QObject::tr("<select target>");

        QnResource::Flags flags = resource->flags();
        if (qnSettings->isIpShownInTree()) {
            if((flags & QnResource::network) || (flags & QnResource::server && flags & QnResource::remote)) {
                QString host = extractHost(resource->getUrl());
                if(!host.isEmpty())
                    return QString(QLatin1String("%1 (%2)")).arg(resource->getName()).arg(host);
            }
        }
        return resource->getName();
    }

    QnBusinessEventRulePtr ruleById(QnBusinessEventRules rules, QString uniqId) {
        foreach(const QnBusinessEventRulePtr& rule, rules)
            if (rule->getUniqueId() == uniqId)
                return rule;
        return QnBusinessEventRulePtr();
    }

    static int IdRole       = Qt::UserRole + 1;
    static int WidgetRole   = Qt::UserRole + 2;
}

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent):
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog()),
    m_listModel(new QStandardItemModel(this)),
    m_currentDetailsWidget(NULL),
    m_connection(connection)
{
    ui->setupUi(this);

    m_listModel->setColumnCount(6);
    m_listModel->setHeaderData(0, Qt::Horizontal, tr("#"));
    m_listModel->setHeaderData(1, Qt::Horizontal, tr("Event"));
    m_listModel->setHeaderData(2, Qt::Horizontal, tr("Source"));
    m_listModel->setHeaderData(3, Qt::Horizontal, QString());
    m_listModel->setHeaderData(4, Qt::Horizontal, tr("Action"));
    m_listModel->setHeaderData(5, Qt::Horizontal, tr("Target"));

    connection->getBusinessRules(m_rules); // TODO: replace synchronous call
    foreach (QnBusinessEventRulePtr rule, m_rules) {
        addRuleToList(rule);
    }

    ui->tableView->setModel(m_listModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::Interactive);

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    ui->tableView->resizeColumnsToContents();
    ui->tableView->clearSelection();

    //TODO: show description label if no rules are loaded
   // ui->newRuleWidget->setExpanded(true);

    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(reject()));
    connect(ui->newRuleButton, SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
//    connect(ui->newRuleWidget, SIGNAL(apply(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(saveRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
//    connect(ui->newRuleWidget, SIGNAL(deleteConfirmed(QnBusinessRuleWidget*,QnBusinessEventRulePtr)), this, SLOT(deleteRule(QnBusinessRuleWidget*,QnBusinessEventRulePtr)));
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    QnBusinessEventRulePtr rule = QnBusinessEventRulePtr(new QnBusinessEventRule);
    m_rules.append(rule);
    addRuleToList(rule);
    ui->tableView->selectRow(m_listModel->rowCount());
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {

    ui->tableView->setEnabled(true);

    if(status != 0) {
        QMessageBox::critical(this, tr("Error while saving rule"), QString::fromLatin1(errorString));
        return;
    }

    foreach (const QnResourcePtr& res, resources) {
        QnBusinessEventRulePtr rule = res.dynamicCast<QnBusinessEventRule>();
        if (!rule)
            continue;

        QnBusinessEventRulePtr old = ruleById(m_rules, rule->getUniqueId());
        if (old) {
            m_rules.replace(m_rules.indexOf(old), rule);

            QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), IdRole, rule->getUniqueId());
            if (!ruleIdx.isEmpty()) {
                int rowNum = ruleIdx.first().row();
                m_listModel->removeRow(rowNum);
                m_listModel->insertRow(rowNum, itemFromRule(rule));
                ui->tableView->selectRow(rowNum);
            }
        } else {
            m_rules.append(rule);
            addRuleToList(rule);
            ui->tableView->selectRow(m_listModel->rowCount() - 1);
        }
    }

 }

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {

    ui->tableView->setEnabled(true);

    if(response.status != 0) {
        qDebug() << response.errorString;
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
        return;
    }
/*
    QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), Qt::UserRole + 1, m_deletingRule->getUniqueId());
    if (!ruleIdx.isEmpty()) {
        int rowNum = ruleIdx.first().row();
        m_listModel->removeRow(rowNum);
        ui->tableView->clearSelection();
    }
*/
}

void QnBusinessRulesDialog::at_ruleHasChangesChanged(QnBusinessRuleWidget* source, bool value) {
    updateItemData(source, 0, value ? QLatin1String("*") : QString());
}

void QnBusinessRulesDialog::at_ruleEventTypeChanged(QnBusinessRuleWidget* source, BusinessEventType::Value value) {
    updateItemData(source, 1, BusinessEventType::toString(value));
}

void QnBusinessRulesDialog::at_ruleEventResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource) {
    updateItemData(source, 2, getResourceName(resource));
}

void QnBusinessRulesDialog::at_ruleEventStateChanged(QnBusinessRuleWidget* source, ToggleState::Value value) {
//    updateItemData(source, 0, value ? QLatin1String("*") : QString());
}

void QnBusinessRulesDialog::at_ruleActionTypeChanged(QnBusinessRuleWidget* source, BusinessActionType::Value value) {
    updateItemData(source, 4, BusinessActionType::toString(value));
}

void QnBusinessRulesDialog::at_ruleActionResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource) {
    updateItemData(source, 5, getResourceName(resource));
}
/*

void QnBusinessRulesDialog::at_ruleChanged(QnBusinessRuleWidget *widget, QnBusinessEventRulePtr rule) {




    QStandardItem *eventTypeItem = m_listModel->item(rowNum, 1);
    eventTypeItem->setText(eventTypeString(rule));

    QStandardItem *eventResourceItem = m_listModel->item(rowNum, 2);
    if (BusinessEventType::isResourceRequired(rule->eventType())) {
        QnResourcePtr resource = rule->eventResource();
        eventResourceItem->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        eventResourceItem->setText(getResourceName(resource));
    } else {
        eventResourceItem->setIcon(QIcon());
        eventResourceItem->setText(QString());
    }

    QStandardItem *actionTypeItem = m_listModel->item(rowNum, 4);
    actionTypeItem->setText(BusinessActionType::toString(rule->actionType()));

    QStandardItem *actionResourceItem = m_listModel->item(rowNum, 5);
    if (BusinessActionType::isResourceRequired(rule->actionType())) {
        QnResourcePtr resource = rule->actionResource();
        actionResourceItem->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        actionResourceItem->setText(getResourceName(resource));
    } else {
        actionResourceItem->setIcon(QIcon());
        actionResourceItem->setText(QString());
    }

}*/

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)

    QStandardItem* item = m_listModel->itemFromIndex(current.sibling(current.row(), 0));

    QString id = item->data(IdRole).toString();

    if (m_currentDetailsWidget) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
    }

    QnBusinessEventRulePtr rule = ruleById(m_rules, id);
    if (!rule)
        return;

    m_currentDetailsWidget = (QnBusinessRuleWidget *)item->data(WidgetRole).value<QWidget *>();

    if (!m_currentDetailsWidget) {
        m_currentDetailsWidget = new QnBusinessRuleWidget(rule, this);
        item->setData(QVariant::fromValue<QWidget* >(m_currentDetailsWidget), WidgetRole);
        connect(m_currentDetailsWidget, SIGNAL(hasChangesChanged(QnBusinessRuleWidget*,bool)),
                this, SLOT(at_ruleHasChangesChanged(QnBusinessRuleWidget*,bool)));
        connect(m_currentDetailsWidget, SIGNAL(eventTypeChanged(QnBusinessRuleWidget*,BusinessEventType::Value)),
                this, SLOT(at_ruleEventTypeChanged(QnBusinessRuleWidget*,BusinessEventType::Value)));
        connect(m_currentDetailsWidget, SIGNAL(eventStateChanged(QnBusinessRuleWidget*,ToggleState::Value)),
                this, SLOT(at_ruleEventStateChanged(QnBusinessRuleWidget*,ToggleState::Value)));
        connect(m_currentDetailsWidget, SIGNAL(eventResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)),
                this, SLOT(at_ruleEventResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)));
        connect(m_currentDetailsWidget, SIGNAL(actionTypeChanged(QnBusinessRuleWidget*,BusinessActionType::Value)),
                this, SLOT(at_ruleActionTypeChanged(QnBusinessRuleWidget*,BusinessActionType::Value)));
        connect(m_currentDetailsWidget, SIGNAL(actionResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)),
                this, SLOT(at_ruleActionResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)));
    }
    ui->detailsLayout->addWidget(m_currentDetailsWidget);
    m_currentDetailsWidget->setVisible(true);
}

QList<QStandardItem *> QnBusinessRulesDialog::itemFromRule(QnBusinessEventRulePtr rule) {
    QStandardItem *numberItem = new QStandardItem();
    numberItem->setData(rule->getUniqueId(), IdRole);

    QStandardItem *eventTypeItem = new QStandardItem(eventTypeString(rule));
    QStandardItem *eventResourceItem = new QStandardItem(QString());
    if (BusinessEventType::isResourceRequired(rule->eventType())) {
        QnResourcePtr resource = rule->eventResource();
        eventResourceItem->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        eventResourceItem->setText(getResourceName(resource));
    }
    QStandardItem *spacerItem = new QStandardItem(tr("->"));

    QStandardItem *actionTypeItem = new QStandardItem(BusinessActionType::toString(rule->actionType()));
    QStandardItem *actionResourceItem = new QStandardItem(QString());
    if (BusinessActionType::isResourceRequired(rule->actionType())) {
        QnResourcePtr resource = rule->actionResource();
        actionResourceItem->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        actionResourceItem->setText(getResourceName(resource));
    }

    QList<QStandardItem *> result;

    result << numberItem << eventTypeItem << eventResourceItem << spacerItem << actionTypeItem << actionResourceItem;
    return result;
}

void QnBusinessRulesDialog::addRuleToList(QnBusinessEventRulePtr rule) {
    m_listModel->appendRow(itemFromRule(rule));
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    int handle = m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    ui->tableView->setEnabled(false);
    //TODO: update row
    //TODO: rule caption should be modified with "Saving..."
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    int handle = m_connection->deleteAsync(rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
    ui->tableView->setEnabled(false);
    //TODO: rule caption should be modified with "Removing..."
}

int QnBusinessRulesDialog::rowNumByWidget(QnBusinessRuleWidget *widget) const {
    QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), WidgetRole,
                                                 QVariant::fromValue<QWidget *>(widget),
                                                 1, Qt::MatchExactly);
    if (ruleIdx.isEmpty())
        return -1;
    return ruleIdx.first().row();
}

void QnBusinessRulesDialog::updateItemData(QnBusinessRuleWidget *widget, int column, QString value) {
    int row = rowNumByWidget(widget);
    if (row < 0)
        return;

    QStandardItem *item = m_listModel->item(row, column);
    item->setText(value);
}
