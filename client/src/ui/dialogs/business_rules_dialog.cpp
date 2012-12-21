#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

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
}

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent):
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog()),
    m_listModel(new QStandardItemModel(this)),
    m_connection(connection)
{
    ui->setupUi(this);

    ui->tableView->setModel(m_listModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::Interactive);

    connection->getBusinessRules(m_rules); // synchronous call
    foreach (QnBusinessEventRulePtr rule, m_rules) {
        addRuleToList(rule);
    }

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    ui->tableView->resizeColumnsToContents();

    //TODO: show description label if no rules are loaded
    ui->newRuleWidget->setExpanded(true);

    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    //connect(ui->newRuleButton, SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->newRuleWidget, SIGNAL(apply(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(saveRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
 //   ui->newRuleWidget->setExpanded(!ui->newRuleWidget->expanded());
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    Q_UNUSED(resources)

    if (!m_savingWidgets.contains(handle))
        return;

    QWidget* w = m_savingWidgets[handle];
    w->setEnabled(true);
    if(status != 0) {
        QMessageBox::critical(this, tr("Error while saving rule"), QString::fromLatin1(errorString));
    }
    else {
        //TODO: update rule from resources
    }
    m_savingWidgets.remove(handle);
}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {
/*    if (!m_deletingWidgets.contains(handle))
        return;

    QWidget* w = m_deletingWidgets[handle];
    if(response.status != 0) {
        qDebug() << response.errorString;
        w->setEnabled(true);
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
    }
    else {
        ui->verticalLayout->removeWidget(w);
        w->setVisible(false);
    }
    m_deletingWidgets.remove(handle);*/
}

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    //TODO: ask if not saved changes
    Q_UNUSED(previous)

    QStandardItem* item = m_listModel->itemFromIndex(current.sibling(current.row(), 0));

    QString id = item->data().toString();
    QnBusinessEventRulePtr rule = ruleById(m_rules, id);
    if (rule)
        ui->newRuleWidget->setRule(rule);
    //else
      //  qDebug() << "current changed" << item->text() ;
}

void QnBusinessRulesDialog::addRuleToList(QnBusinessEventRulePtr rule) {
  /*  QnBusinessRuleWidget *w = new QnBusinessRuleWidget(this);
    w->setRule(rule);
    connect(w, SIGNAL(apply(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(saveRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
    connect(w, SIGNAL(deleteConfirmed(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(deleteRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
    ui->verticalLayout->insertWidget(0, w);*/

    QStandardItem *eventTypeItem = new QStandardItem(eventTypeString(rule));
    eventTypeItem->setData(rule->getUniqueId());

    QStandardItem *eventResourceItem = new QStandardItem(QString());
    if (BusinessEventType::isResourceRequired(rule->eventType())) {
        QnResourcePtr resource = rule->eventResource();
        eventResourceItem->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        eventResourceItem->setText(getResourceName(resource));
    }
    QStandardItem *spacerItem = new QStandardItem(tr("->"));

    QStandardItem *actionTypeItem = new QStandardItem(tr("do ") + BusinessActionType::toString(rule->actionType()));
    QStandardItem *actionResourceItem = new QStandardItem(QString());
    if (BusinessActionType::isResourceRequired(rule->actionType())) {
        QnResourcePtr resource = rule->actionResource();
        actionResourceItem->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        actionResourceItem->setText(getResourceName(resource));
    }

    QList<QStandardItem *> row;
    row << eventTypeItem << eventResourceItem << spacerItem << actionTypeItem << actionResourceItem;
    m_listModel->appendRow(row);
}


void QnBusinessRulesDialog::newRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    saveRule(widget, rule);
    addRuleToList(rule);
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    if (m_savingWidgets.values().contains(widget))
        return;

    int handle = m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    m_savingWidgets[handle] = widget;
    widget->setEnabled(false);
    //TODO: update row
    //TODO: rule caption should be modified with "Saving..."
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
  /*  if (m_deletingWidgets.values().contains(widget))
        return;

    int handle = m_connection->deleteAsync(rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
    m_deletingWidgets[handle] = widget;
    widget->setEnabled(false);*/
    //TODO: rule caption should be modified with "Removing..."
}
