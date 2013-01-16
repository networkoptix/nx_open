#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtGui/QMessageBox>

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>

#include <ui/style/resource_icon_cache.h>

#include <ui/workbench/workbench_context.h>
#include "ui/workbench/workbench_access_controller.h"

#include <utils/settings.h>

namespace {
//TODO: tr
    static QLatin1String prolongedEvent("While %1");
    static QLatin1String instantEvent("On %1 %2");

    QString toggleStateToString(ToggleState::Value state) {
        switch (state) {
        case ToggleState::On: return QObject::tr("start");
        case ToggleState::Off: return QObject::tr("stop");
            default: return QString();
        }
        return QString();
    }

    QString eventTypeString(BusinessEventType::Value eventType,
                            ToggleState::Value eventState,
                            BusinessActionType::Value actionType){
        QString typeStr = BusinessEventType::toString(eventType);
        if (BusinessActionType::hasToggleState(actionType))
            return QString(prolongedEvent).arg(typeStr);
        else
            return QString(instantEvent).arg(typeStr)
                    .arg(toggleStateToString(eventState));
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

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::BusinessRulesDialog()),
    m_listModel(new QStandardItemModel(this)),
    m_currentDetailsWidget(NULL),
    m_connection(connection)
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);

    ui->tableView->setModel(m_listModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->installEventFilter(this);

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    //ui->tableView->resizeColumnsToContents();
    ui->tableView->clearSelection();

    //TODO: show description label if no rules are loaded

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(at_saveAllButton_clicked()));
    connect(ui->buttonBox,                                  SIGNAL(accepted()),this, SLOT(at_saveAllButton_clicked()));
    connect(ui->addRuleButton,                              SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->deleteRuleButton,                           SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));

    connect(context,  SIGNAL(userChanged(const QnUserResourcePtr &)),          this, SLOT(at_context_userChanged()));

//    connect(ui->closeButton,    SIGNAL(clicked()), this, SLOT(reject()));

    at_context_userChanged();
    updateControlButtons();
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

bool QnBusinessRulesDialog::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent->key() == Qt::Key_Delete) {
            at_deleteButton_clicked();
            return true;
        }
    }
    return base_type::eventFilter(object, event);
}

void QnBusinessRulesDialog::at_context_userChanged() {
   // bool enabled = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;

    if (m_currentDetailsWidget) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
    }

    for(int i = 0; i < m_listModel->rowCount(); i++) {
        QnBusinessRuleWidget* w = (QnBusinessRuleWidget *)m_listModel->item(i, 0)->data(WidgetRole).value<QWidget *>();
        delete w;
    }
    m_listModel->clear();

    QStringList header;
    header << tr("#") << tr("Event") << tr("Source") << QString() << tr("Action") << tr("Target");
    m_listModel->setHorizontalHeaderLabels(header);

    if ((accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        QnBusinessEventRules rules;
        m_connection->getBusinessRules(rules); // TODO: replace synchronous call
        foreach (QnBusinessEventRulePtr rule, rules) {
            QnBusinessRuleWidget* w = createWidget(rule);
            m_listModel->appendRow(createRow(w));
            w->resetFromRule(); //here row data will be updated
        }
    }

    updateControlButtons();
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

    ui->tableView->selectRow(m_listModel->rowCount() - 1);
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

    if (m_currentDetailsWidget->rule()->getId() &&
            QMessageBox::question(this,
                              tr("Confirm rule deletion"),
                              tr("Are you sure you want to delete this rule?"),
                              QMessageBox::Ok,
                              QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    deleteRule(m_currentDetailsWidget);
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

    if (success) {
        QnResourcePtr res = resources.first();
        QnBusinessEventRulePtr rule = res.dynamicCast<QnBusinessEventRule>();
        if (!rule)
            success = false;
        else
            w->rule()->setId(rule->getId());
    }


}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {

    if (!m_processingWidgets.contains(handle))
        return;
    QnBusinessRuleWidget* w = m_processingWidgets[handle];
    w->setEnabled(true);
    m_processingWidgets.remove(handle);

    if(response.status != 0) {
        //TODO: #GDM remove password from error message
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
        return;
    }

    updateControlButtons();

    QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), WidgetRole,
                                                 QVariant::fromValue<QWidget *>(w),
                                                 1, Qt::MatchExactly);
    if (ruleIdx.isEmpty())
        return;
    int row = ruleIdx.first().row();
    m_listModel->removeRow(row);

    if (m_currentDetailsWidget == w) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
        ui->tableView->clearSelection();
    }

    delete w;
}

void QnBusinessRulesDialog::at_widgetHasChangesChanged(QnBusinessRuleWidget* source, bool value) {
    QStandardItem *item = tableItem(source, 0);
    item->setText(value ? QLatin1String("*") : QString());
    item->setData(value, ModifiedRole);

    updateControlButtons();
}

void QnBusinessRulesDialog::at_widgetDefinitionChanged(QnBusinessRuleWidget *source,
                                                       BusinessEventType::Value eventType,
                                                       ToggleState::Value eventState,
                                                       BusinessActionType::Value actionType) {
    QStandardItem *eventItem = tableItem(source, 1);
    eventItem->setText(eventTypeString(eventType, eventState, actionType));

    QStandardItem *actionItem = tableItem(source, 4);
    actionItem->setText(BusinessActionType::toString(actionType));
}


void QnBusinessRulesDialog::at_widgetEventResourcesChanged(QnBusinessRuleWidget* source, BusinessEventType::Value eventType, const QnResourceList &resources) {
    QStandardItem *item = tableItem(source, 2);
    if (!BusinessEventType::isResourceRequired(eventType)) {
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        item->setText(tr("<System>"));
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else if (BusinessEventType::requiresServerResource(eventType)){
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Server));
        if (resources.size() == 0)
            item->setText(tr("<Any Server>"));
        else
            item->setText(tr("%1 Servers").arg(resources.size())); //TODO: fix tr to %n
    } else /*if (BusinessEventType::requiresCameraResource(eventType))*/ {
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));
        if (resources.size() == 0)
            item->setText(tr("<Any Camera>"));
        else
            item->setText(tr("%1 Cameras").arg(resources.size())); //TODO: fix tr to %n
    }
}


void QnBusinessRulesDialog::at_widgetActionResourcesChanged(QnBusinessRuleWidget* source, BusinessActionType::Value actionType, const QnResourceList &resources) {
    QStandardItem *item = tableItem(source, 5);

    if (actionType == BusinessActionType::BA_SendMail) {
        QString recipients = source->actionResourcesText();
        QStringList list = recipients.split(QLatin1Char(';'), QString::SkipEmptyParts);

        switch (list.size()){
            case 0:
                item->setIcon(qnResIconCache->icon(QnResourceIconCache::Offline, true));
                item->setText(tr("<Enter at least one address>"));
                break;
            case 1:
                item->setIcon(qnResIconCache->icon(QnResourceIconCache::User));
                item->setText(recipients);
                break;
            default:
                item->setIcon(qnResIconCache->icon(QnResourceIconCache::Users));
                item->setText(recipients);
                break;
        }
    }
    else if (!BusinessActionType::isResourceRequired(actionType)) {
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        item->setText(tr("<System>"));
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else {
        //TODO: #GDM popup action will require user resources
        if (resources.size() == 0) {
            item->setIcon(qnResIconCache->icon(QnResourceIconCache::Offline, true));
            item->setText(tr("<Select at least one camera>"));
        }
        else {
            item->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));
            item->setText(tr("%1 Cameras").arg(resources.size())); //TODO: fix tr to %n
        }
    }
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
    QnBusinessRuleWidget* w = new QnBusinessRuleWidget(rule, this, context());
    connect(w, SIGNAL(hasChangesChanged(QnBusinessRuleWidget*,bool)),
            this, SLOT(at_widgetHasChangesChanged(QnBusinessRuleWidget*,bool)));
    connect(w, SIGNAL(definitionChanged(QnBusinessRuleWidget*,BusinessEventType::Value,ToggleState::Value,BusinessActionType::Value)),
            this, SLOT(at_widgetDefinitionChanged(QnBusinessRuleWidget*,BusinessEventType::Value,ToggleState::Value,BusinessActionType::Value)));
    connect(w, SIGNAL(eventResourcesChanged(QnBusinessRuleWidget*,BusinessEventType::Value,QnResourceList)),
            this, SLOT(at_widgetEventResourcesChanged(QnBusinessRuleWidget*,BusinessEventType::Value,QnResourceList)));
    connect(w, SIGNAL(actionResourcesChanged(QnBusinessRuleWidget*,BusinessActionType::Value,QnResourceList)),
            this, SLOT(at_widgetActionResourcesChanged(QnBusinessRuleWidget*,BusinessActionType::Value,QnResourceList)));
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
    if (!rule->getId()) {
        QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), WidgetRole,
                                                     QVariant::fromValue<QWidget *>(widget),
                                                     1, Qt::MatchExactly);
        if (!ruleIdx.isEmpty()) {
            int rowNum = ruleIdx.first().row();
            m_listModel->removeRow(rowNum);
            ui->tableView->clearSelection();
        }
        return;
    }

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
    bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;

//    ui->saveButton->setEnabled(m_currentDetailsWidget && m_currentDetailsWidget->hasChanges());
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasRights);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasRights &&
         !m_listModel->match(m_listModel->index(0, 0), ModifiedRole, true, 1, Qt::MatchExactly).isEmpty());

    ui->deleteRuleButton->setEnabled(hasRights && m_currentDetailsWidget);
    ui->addRuleButton->setEnabled(hasRights);
}
