#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>
#include <QtGui/QMessageBox>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/dialogs/select_cameras_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/business/business_event_widget_factory.h>
#include <ui/widgets/business/business_action_widget_factory.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>

#include <utils/settings.h>


namespace {
    QString extractHost(const QString &url) {
        /* Try it as a host address first. */
        QHostAddress hostAddress(url);
        if(!hostAddress.isNull())
            return hostAddress.toString();

        /* Then go default QUrl route. */
        return QUrl(url).host();
    }

    QString toggleStateToString(ToggleState::Value value, bool prolonged) {
        switch( value )
        {
            case ToggleState::Off:
                return QObject::tr("Stops");
            case ToggleState::On:
                return QObject::tr("Starts");
            case ToggleState::NotDefined:
            if (prolonged)
                return QObject::tr("Starts/Stops");
            else
                return QObject::tr("Occurs");
        }
        return QString();
    }

} // namespace

QnBusinessRuleWidget::QnBusinessRuleWidget(QnBusinessEventRulePtr rule, QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_rule(rule),
    m_hasChanges(false),
    m_eventParameters(NULL),
    m_actionParameters(NULL),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    ui->eventTypeComboBox->setModel(m_eventTypesModel);
    ui->eventStatesComboBox->setModel(m_eventStatesModel);
    ui->actionTypeComboBox->setModel(m_actionTypesModel);

    ui->eventParamsFrame->setVisible(false);

    ui->eventDefinitionFrame->installEventFilter(this);
    ui->actionDefinitionFrame->installEventFilter(this);

    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Window).lighter());
    this->setPalette(pal);

    connect(ui->eventTypeComboBox,      SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,    SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->eventResourcesHolder,   SIGNAL(clicked()),                this, SLOT(at_eventResourcesHolder_clicked()));
    connect(ui->actionResourcesHolder,  SIGNAL(clicked()),                this, SLOT(at_actionResourcesHolder_clicked()));

    connect(ui->actionTypeComboBox,     SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    //TODO: connect notifyChaged on subitems
    //TODO: setup onResourceChanged to update widgets depending on resource, e.g. max fps or channel list

/*    connect(ui->resetButton,  SIGNAL(clicked()), this, SLOT(resetFromRule()));

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->applyButton,  SIGNAL(clicked()), this, SLOT(at_applyButton_clicked()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));*/

    initEventTypes();
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
}

QnBusinessEventRulePtr QnBusinessRuleWidget::rule() const {
    return m_rule;
}

bool QnBusinessRuleWidget::hasChanges() const {
    return m_hasChanges;
}

void QnBusinessRuleWidget::initEventTypes() {
    m_eventTypesModel->clear();
    for (int i = BusinessEventType::BE_FirstType; i <= BusinessEventType::BE_LastType; i++) {
        BusinessEventType::Value val = (BusinessEventType::Value)i;

        QStandardItem *item = new QStandardItem(BusinessEventType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initEventStates(BusinessEventType::Value eventType) {
    m_eventStatesModel->clear();

    QList<ToggleState::Value> values;
    values << ToggleState::NotDefined;
    bool prolonged = BusinessEventType::hasToggleState(eventType);
    if (prolonged)
        values << ToggleState::On << ToggleState::Off;


    foreach (ToggleState::Value val, values) {
        QStandardItem *item = new QStandardItem(toggleStateToString(val, prolonged));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
    ui->eventStatesComboBox->setVisible(prolonged);
}

void QnBusinessRuleWidget::initEventParameters(BusinessEventType::Value eventType) {
    if (m_eventParameters) {
        ui->eventParamsLayout->removeWidget(m_eventParameters);
        m_eventParameters->setVisible(false);
    }

    if (m_eventWidgetsByType.contains(eventType)) {
        m_eventParameters = m_eventWidgetsByType.find(eventType).value();
    } else {
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(eventType, this);
        m_eventWidgetsByType[eventType] = m_eventParameters;
    }
    if (m_eventParameters) {
        ui->eventParamsLayout->addWidget(m_eventParameters);
        m_eventParameters->setVisible(true);
    }
}

void QnBusinessRuleWidget::initActionTypes(ToggleState::Value eventState) {

    m_actionTypesModel->clear();
    // what type of actions to show: prolonged or instant
    bool instantActionsFilter = (eventState == ToggleState::On || eventState == ToggleState::Off)
            || (!BusinessEventType::hasToggleState(getCurrentEventType()));

    for (int i = BusinessActionType::BA_FirstType; i <= BusinessActionType::BA_LastType; i++) {
        BusinessActionType::Value val = (BusinessActionType::Value)i;

        if (BusinessActionType::hasToggleState(val) && instantActionsFilter)
            continue;

        QStandardItem *item = new QStandardItem(BusinessActionType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }

}

void QnBusinessRuleWidget::initActionParameters(BusinessActionType::Value actionType) {
    if (m_actionParameters) {
        //ui->actionLayout->removeWidget(m_actionParameters);
        ui->actionParamsLayout->removeWidget(m_actionParameters);
        m_actionParameters->setVisible(false);
    }

    if (m_actionWidgetsByType.contains(actionType)) {
        m_actionParameters = m_actionWidgetsByType.find(actionType).value();
    } else {
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(actionType, this);
        m_actionWidgetsByType[actionType] = m_actionParameters;
    }

    if (m_actionParameters) {
        //ui->actionLayout->addWidget(m_actionParameters);
        ui->actionParamsLayout->addWidget(m_actionParameters);
        m_actionParameters->setVisible(true);
    }
}

bool QnBusinessRuleWidget::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::DragEnter) {
        QDragEnterEvent* de = static_cast<QDragEnterEvent*>(event);
        m_dropResources = QnWorkbenchResource::deserializeResources(de->mimeData());
        if (!m_dropResources.empty())
            de->acceptProposedAction();
        return true;
    } else if (event->type() == QEvent::Drop) {
        QDropEvent* de = static_cast<QDropEvent*>(event);
        if (!m_dropResources.empty()) {
            if (object == ui->eventDefinitionFrame) {
                foreach(QnResourcePtr res, m_dropResources) {
                    if (m_eventResources.contains(res))
                        continue;
                    m_eventResources.append(res);
                }
                updateEventResources();
            } else if (object == ui->actionDefinitionFrame) {
                foreach(QnResourcePtr res, m_dropResources) {
                    if (m_actionResources.contains(res))
                        continue;
                    m_actionResources.append(res);
                }
                updateActionResources();
            }
            m_dropResources = QnResourceList();
            de->acceptProposedAction();
        }
        return true;
    } else if (event->type() == QEvent::DragLeave) {
        m_dropResources = QnResourceList();
        return true;
    }

    return base_type::eventFilter(object, event);
}


BusinessEventType::Value QnBusinessRuleWidget::getCurrentEventType() const {
    int typeIdx = m_eventTypesModel->item(ui->eventTypeComboBox->currentIndex())->data().toInt();
    return (BusinessEventType::Value)typeIdx;
}

ToggleState::Value QnBusinessRuleWidget::getCurrentEventToggleState() const {
    int typeIdx = m_eventStatesModel->item(ui->eventStatesComboBox->currentIndex())->data().toInt();
    return (ToggleState::Value)typeIdx;
}


BusinessActionType::Value QnBusinessRuleWidget::getCurrentActionType() const {
    int typeIdx = m_actionTypesModel->item(ui->actionTypeComboBox->currentIndex())->data().toInt();
    return (BusinessActionType::Value)typeIdx;
}

QString QnBusinessRuleWidget::getResourceName(const QnResourcePtr& resource) const {
    if (!resource)
        return tr("<select target>");

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

void QnBusinessRuleWidget::setHasChanges(bool hasChanges) {
    if (hasChanges == m_hasChanges)
        return;
    m_hasChanges = hasChanges;
    emit hasChangesChanged(this, m_hasChanges);
}

// Handlers

void QnBusinessRuleWidget::resetFromRule() {
    QModelIndexList eventTypeIdx = m_eventTypesModel->match(m_eventTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->eventType());
    ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

    m_eventResources = m_rule->eventResources();

    ToggleState::Value toggleState = BusinessEventParameters::getToggleState(m_rule->eventParams());
    QModelIndexList stateIdx = m_eventStatesModel->match(m_eventStatesModel->index(0, 0), Qt::UserRole + 1, (int)toggleState);
    ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());

    QModelIndexList actionTypeIdx = m_actionTypesModel->match(m_actionTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->actionType());
    ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

    if (m_eventParameters)
        m_eventParameters->loadParameters(m_rule->eventParams());
    //TODO: setup widget depending on resource, e.g. max fps or channel list

    if (m_actionParameters)
        m_actionParameters->loadParameters(m_rule->actionParams());
    //TODO: setup widget depending on resource, e.g. max fps or channel list

    emit eventTypeChanged(this, m_rule->eventType());
    updateEventResources();
    emit eventStateChanged(this, toggleState);
    emit actionTypeChanged(this, m_rule->actionType());
    updateActionResources();

    setHasChanges(false);
}

void QnBusinessRuleWidget::apply() {
    //TODO: validate params!

    m_rule->setEventType(getCurrentEventType());

    QnBusinessParams eventParams;
    if (m_eventParameters)
        eventParams = m_eventParameters->parameters();
    BusinessEventParameters::setToggleState(&eventParams, getCurrentEventToggleState());
    m_rule->setEventParams(eventParams);

    if (!BusinessEventType::isResourceRequired(m_rule->eventType()))
        m_rule->setEventResources(QnResourceList());
    else if (BusinessEventType::requiresCameraResource(m_rule->eventType()))
        m_rule->setEventResources(m_eventResources.filtered<QnVirtualCameraResource>());
    else if (BusinessEventType::requiresServerResource(m_rule->eventType()))
        m_rule->setEventResources(m_eventResources.filtered<QnMediaServerResource>());

    m_rule->setActionType(getCurrentActionType());

    if (m_actionParameters)
        m_rule->setActionParams(m_actionParameters->parameters());
}

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventTypesModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;

    updateEventResources();
    initEventStates(val);
    initEventParameters(val);

    setHasChanges(true);
    emit eventTypeChanged(this, val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventStatesModel->item(index)->data().toInt();
    ToggleState::Value val = (ToggleState::Value)typeIdx;

    initActionTypes(val);

    setHasChanges(true);
    emit eventStateChanged(this, val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_actionTypesModel->item(index)->data().toInt();
    BusinessActionType::Value val = (BusinessActionType::Value)typeIdx;

    updateActionResources();
    initActionParameters(val);

    setHasChanges(true);
    emit actionTypeChanged(this, val);
}

void QnBusinessRuleWidget::at_eventResourcesHolder_clicked() {
    QnSelectCamerasDialog dialog(this, context());
    dialog.setSelectedResources(m_eventResources.filtered<QnVirtualCameraResource>());
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_eventResources.clear();
    m_eventResources.append(dialog.getSelectedResources());
    updateEventResources();
}

void QnBusinessRuleWidget::at_actionResourcesHolder_clicked() {
    QnSelectCamerasDialog dialog(this, context());
    dialog.setSelectedResources(m_actionResources);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_actionResources.clear();
    m_actionResources.append(dialog.getSelectedResources());
    updateActionResources();
}

void QnBusinessRuleWidget::updateEventResources() {
    BusinessEventType::Value eventType = getCurrentEventType();
    QPushButton* item = ui->eventResourcesHolder;

    bool isResourceRequired = BusinessEventType::isResourceRequired(eventType);
    item->setVisible(isResourceRequired);
    ui->eventAtLabel->setVisible(isResourceRequired);
    ui->eventDropLabel->setVisible(isResourceRequired);

    if (m_eventResources.size() == 1) {
        QnResourcePtr resource = m_eventResources.first();
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else if (BusinessEventType::requiresServerResource(eventType)){
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Server));
        if (m_eventResources.size() == 0)
            item->setText(tr("<Any Server>"));
        else
            item->setText(tr("%1 Servers").arg(m_eventResources.size())); //TODO: fix tr to %n
    } else if (BusinessEventType::requiresCameraResource(eventType)) {
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));
        if (m_eventResources.size() == 0)
            item->setText(tr("<Any Camera>"));
        else
            item->setText(tr("%1 Cameras").arg(m_eventResources.size())); //TODO: fix tr to %n
    }

    //TODO: filtered resources list
    emit eventResourcesChanged(this, m_eventResources);
}

void QnBusinessRuleWidget::updateActionResources() {
    BusinessActionType::Value actionType = getCurrentActionType();
    QPushButton* item = ui->actionResourcesHolder;

    bool isResourceRequired = BusinessActionType::isResourceRequired(actionType);
    item->setVisible(isResourceRequired);
    ui->actionAtLabel->setVisible(isResourceRequired);
    ui->actionDropLabel->setVisible(isResourceRequired);

    if (m_actionResources.size() == 1) {
        QnResourcePtr resource = m_actionResources.first();
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else {
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));
        if (m_actionResources.size() == 0)
            item->setText(tr("<Select at least one camera>"));
        else
            item->setText(tr("%1 Cameras").arg(m_actionResources.size())); //TODO: fix tr to %n
    }

    emit actionResourcesChanged(this, m_actionResources);
}
