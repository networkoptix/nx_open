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

    static int WidgetRole   = Qt::UserRole + 1;
    static int ModifiedRole = Qt::UserRole + 2;
}

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent):
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog()),
    m_listModel(new QStandardItemModel(this)),
    m_currentDetailsWidget(NULL),
    m_connection(connection)
{
    ui->setupUi(this);

    QStringList header;
    header << tr("#") << tr("Event") << tr("Source") << QString() << tr("Action") << tr("Target");
    m_listModel->setHorizontalHeaderLabels(header);
    /*m_listModel->setColumnCount(6);
    m_listModel->setHeaderData(0, Qt::Horizontal, tr("#"));
    m_listModel->setHeaderData(1, Qt::Horizontal, tr("Event"));
    m_listModel->setHeaderData(2, Qt::Horizontal, tr("Source"));
    m_listModel->setHeaderData(3, Qt::Horizontal, QString());
    m_listModel->setHeaderData(4, Qt::Horizontal, tr("Action"));
    m_listModel->setHeaderData(5, Qt::Horizontal, tr("Target"));*/


    QnBusinessEventRules rules;
    connection->getBusinessRules(rules); // TODO: replace synchronous call
    foreach (QnBusinessEventRulePtr rule, rules) {
        QnBusinessRuleWidget* w = createWidget(rule);
        m_listModel->appendRow(createRow(w));
        w->resetFromRule(); //here row data will be updated
    }

    ui->tableView->setModel(m_listModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    //ui->tableView->resizeColumnsToContents();
    ui->tableView->clearSelection();

    ui->refreshButton->setVisible(false);

    //TODO: show description label if no rules are loaded

    connect(ui->newRuleButton,  SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->saveButton,     SIGNAL(clicked()), this, SLOT(at_saveButton_clicked()));
    connect(ui->saveAllButton,  SIGNAL(clicked()), this, SLOT(at_saveAllButton_clicked()));
    connect(ui->deleteButton,   SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->undoButton,     SIGNAL(clicked()), this, SLOT(at_undoButton_clicked()));
    connect(ui->closeButton,    SIGNAL(clicked()), this, SLOT(reject()));

    updateControlButtons();

    ui->horizontalSlider->setMaximum(1024);
    ui->horizontalSlider->setLockedValue(512);
    ui->horizontalSlider->setRecordedValue(600);
    ui->horizontalSlider->setValue(712);
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    QnBusinessEventRulePtr rule = QnBusinessEventRulePtr(new QnBusinessEventRule());
    //TODO: wizard dialog?
    rule->setEventType(BusinessEventType::BE_Camera_Disconnect);
    rule->setActionType(BusinessActionType::BA_Alert);

    QnBusinessRuleWidget* w = createWidget(rule);
    m_listModel->appendRow(createRow(w));
    w->resetFromRule(); //here row data will be updated
    w->setHasChanges(true);

    //ui->tableView->resizeColumnsToContents();
    ui->tableView->selectRow(m_listModel->rowCount() - 1);
}

void QnBusinessRulesDialog::at_saveButton_clicked() {
    if (!m_currentDetailsWidget && !m_currentDetailsWidget->hasChanges())
        return;
    saveRule(m_currentDetailsWidget);
}

void QnBusinessRulesDialog::at_saveAllButton_clicked() {
    for (int i = 0; i < m_listModel->rowCount(); i++) {
        QStandardItem* item = m_listModel->item(i);
        QnBusinessRuleWidget* w = (QnBusinessRuleWidget *)item->data(WidgetRole).value<QWidget *>();
        if (!w || !w->hasChanges())
            continue;
        saveRule(w);
    }
}

void QnBusinessRulesDialog::at_deleteButton_clicked() {
    if (!m_currentDetailsWidget && !m_currentDetailsWidget->hasChanges())
        return;

    if (QMessageBox::question(this,
                              tr("Confirm rule deletion"),
                              tr("Are you sure you want to delete this rule?"),
                              QMessageBox::Ok,
                              QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    deleteRule(m_currentDetailsWidget);
}

void QnBusinessRulesDialog::at_undoButton_clicked() {
    if (!m_currentDetailsWidget && !m_currentDetailsWidget->hasChanges())
        return;
    m_currentDetailsWidget->resetFromRule();
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {

    if (!m_processingWidgets.contains(handle))
        return;

    QnBusinessRuleWidget* w = m_processingWidgets[handle];
    w->setEnabled(true);
    m_processingWidgets.remove(handle);

    bool success = (status == 0 && resources.size() == 1);
    if(!success) {
        QMessageBox::critical(this, tr("Error while saving rule"), QString::fromLatin1(errorString));
        return;
    }
    w->setHasChanges(false);

    updateControlButtons();

    //TODO: load changes from resource

    /*if (success) {
        QnResourcePtr res = resources.first();
        QnBusinessEventRulePtr rule = res.dynamicCast<QnBusinessEventRule>();
        if (!rule)
            success = false;
        else
            w->rule()->update(rule);
    }*/


}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {

    if (!m_processingWidgets.contains(handle))
        return;
    QnBusinessRuleWidget* w = m_processingWidgets[handle];
    w->setEnabled(true);
    m_processingWidgets.remove(handle);

    if(response.status != 0) {
        qDebug() << response.errorString;
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
        return;
    }

    updateControlButtons();
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
    QStandardItem *item = tableItem(source, 0);
    item->setText(value ? QLatin1String("*") : QString());
    item->setData(value, ModifiedRole);

    updateControlButtons();
}

void QnBusinessRulesDialog::at_ruleEventTypeChanged(QnBusinessRuleWidget* source, BusinessEventType::Value value) {
    QStandardItem *item = tableItem(source, 1);
    item->setText(BusinessEventType::toString(value));
    //ui->tableView->resizeColumnsToContents();
}

void QnBusinessRulesDialog::at_ruleEventResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource) {
    QStandardItem *item = tableItem(source, 2);
    if (resource) {
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else {
        item->setIcon(QIcon());
        item->setText(QString());
    }
    //ui->tableView->resizeColumnsToContents();
}

void QnBusinessRulesDialog::at_ruleEventStateChanged(QnBusinessRuleWidget* source, ToggleState::Value value) {
    //QStandardItem *item = tableItem(source, 1);
    //item->setText(eventTypeString(source->rule()));
}

void QnBusinessRulesDialog::at_ruleActionTypeChanged(QnBusinessRuleWidget* source, BusinessActionType::Value value) {
    QStandardItem *item = tableItem(source, 4);
    item->setText(BusinessActionType::toString(value));
    //ui->tableView->resizeColumnsToContents();
}

void QnBusinessRulesDialog::at_ruleActionResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource) {
    QStandardItem *item = tableItem(source, 5);
    if (resource) {
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else {
        item->setIcon(QIcon());
        item->setText(QString());
    }
    //ui->tableView->resizeColumnsToContents();
}

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)

    if (m_currentDetailsWidget) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
    }

    QStandardItem* item = m_listModel->itemFromIndex(current.sibling(current.row(), 0));
    m_currentDetailsWidget = (QnBusinessRuleWidget *)item->data(WidgetRole).value<QWidget *>();

    if (m_currentDetailsWidget) {
        ui->detailsLayout->addWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(true);
    }

    updateControlButtons();
}

QnBusinessRuleWidget* QnBusinessRulesDialog::createWidget(QnBusinessEventRulePtr rule) {
    QnBusinessRuleWidget* w = new QnBusinessRuleWidget(rule, this);
    connect(w, SIGNAL(hasChangesChanged(QnBusinessRuleWidget*,bool)),
            this, SLOT(at_ruleHasChangesChanged(QnBusinessRuleWidget*,bool)));
    connect(w, SIGNAL(eventTypeChanged(QnBusinessRuleWidget*,BusinessEventType::Value)),
            this, SLOT(at_ruleEventTypeChanged(QnBusinessRuleWidget*,BusinessEventType::Value)));
    connect(w, SIGNAL(eventStateChanged(QnBusinessRuleWidget*,ToggleState::Value)),
            this, SLOT(at_ruleEventStateChanged(QnBusinessRuleWidget*,ToggleState::Value)));
    connect(w, SIGNAL(eventResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)),
            this, SLOT(at_ruleEventResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)));
    connect(w, SIGNAL(actionTypeChanged(QnBusinessRuleWidget*,BusinessActionType::Value)),
            this, SLOT(at_ruleActionTypeChanged(QnBusinessRuleWidget*,BusinessActionType::Value)));
    connect(w, SIGNAL(actionResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)),
            this, SLOT(at_ruleActionResourceChanged(QnBusinessRuleWidget*,QnResourcePtr)));
    w->setVisible(false);
    return w;
}

QList<QStandardItem *> QnBusinessRulesDialog::createRow(QnBusinessRuleWidget* widget) {

    //TODO: source -> event-> target -> action (source = system if none)
    QStandardItem *statusItem = new QStandardItem();
    statusItem->setData(QVariant::fromValue<QWidget* >(widget), WidgetRole);
    QStandardItem *eventTypeItem = new QStandardItem();
    QStandardItem *eventResourceItem = new QStandardItem();
    QStandardItem *spacerItem = new QStandardItem(QLatin1String("->"));
    QStandardItem *actionTypeItem = new QStandardItem();
    QStandardItem *actionResourceItem = new QStandardItem();
    QList<QStandardItem *> result;

    result << statusItem << eventTypeItem << eventResourceItem << spacerItem << actionTypeItem << actionResourceItem;
    return result;
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleWidget* widget) {
    if (m_processingWidgets.values().contains(widget))
        return;

    //save changes to the rule field
    widget->apply();

    QnBusinessEventRulePtr rule = widget->rule();
    int handle = m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    widget->setEnabled(false);
    m_processingWidgets[handle] = widget;
    //TODO: update row
    //TODO: rule caption should be modified with "Saving..."
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleWidget* widget) {
    if (m_processingWidgets.values().contains(widget))
        return;

    QnBusinessEventRulePtr rule = widget->rule();
    int handle = m_connection->deleteAsync(rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
    widget->setEnabled(false);
    m_processingWidgets[handle] = widget;

    //TODO: rule caption should be modified with "Removing..."
}

QStandardItem* QnBusinessRulesDialog::tableItem(QnBusinessRuleWidget *widget, int column) const {
    QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), WidgetRole,
                                                 QVariant::fromValue<QWidget *>(widget),
                                                 1, Qt::MatchExactly);
    if (ruleIdx.isEmpty())
        return NULL;
    int row = ruleIdx.first().row();
    return m_listModel->item(row, column);
}

void QnBusinessRulesDialog::updateControlButtons() {
    ui->saveButton->setEnabled(m_currentDetailsWidget && m_currentDetailsWidget->hasChanges());
    ui->saveAllButton->setEnabled(!m_listModel->match(m_listModel->index(0, 0), ModifiedRole, true, 1, Qt::MatchExactly).isEmpty());
    ui->deleteButton->setEnabled(m_currentDetailsWidget);
    ui->undoButton->setEnabled(m_currentDetailsWidget && m_currentDetailsWidget->hasChanges());
}
