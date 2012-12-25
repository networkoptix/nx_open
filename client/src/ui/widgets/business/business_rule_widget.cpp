#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>
#include <QtGui/QMessageBox>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/business/business_event_widget_factory.h>
#include <ui/widgets/business/business_action_widget_factory.h>

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

QnBusinessRuleWidget::QnBusinessRuleWidget(QnBusinessEventRulePtr rule, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_rule(rule),
    m_hasChanges(false),
    m_eventParameters(NULL),
    m_actionParameters(NULL),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventResourcesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this)),
    m_actionResourcesModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    ui->eventTypeComboBox->setModel(m_eventTypesModel);
    ui->eventResourceComboBox->setModel(m_eventResourcesModel);
    ui->eventStatesComboBox->setModel(m_eventStatesModel);
    ui->actionTypeComboBox->setModel(m_actionTypesModel);
    ui->actionResourceComboBox->setModel(m_actionResourcesModel);

    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Window).lighter());
    this->setPalette(pal);

    connect(ui->eventTypeComboBox,      SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,    SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->eventResourceComboBox,  SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventResourceComboBox_currentIndexChanged(int)));
    connect(ui->actionTypeComboBox,     SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));
    connect(ui->actionResourceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionResourceComboBox_currentIndexChanged(int)));

    //TODO: connect notifyChaged on subitems
    //TODO: setup onResourceChanged to update widgets depending on resource, e.g. max fps or channel list

/*    connect(ui->resetButton,  SIGNAL(clicked()), this, SLOT(resetFromRule()));

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->applyButton,  SIGNAL(clicked()), this, SLOT(at_applyButton_clicked()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));*/

    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(updateResources()));
    connect(qnResPool, SIGNAL(resourceChanged(QnResourcePtr)), this, SLOT(updateResources()));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(updateResources()));

    initEventTypes();
    resetFromRule();
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

void QnBusinessRuleWidget::initEventResources(BusinessEventType::Value eventType) {
    //TODO: update list on respool changed, keep selected if any
    //TODO: think about opening list of rules before resources are loaded (or when offline)
    m_eventResourcesModel->clear();

    QnResourceList resources;

    if (BusinessEventType::requiresCameraResource(eventType))
        resources = qnResPool->getResources().filtered<QnVirtualCameraResource>();
    else if (BusinessEventType::requiresServerResource(eventType))
        resources = qnResPool->getResources().filtered<QnMediaServerResource>();

    foreach (const QnResourcePtr &resource, resources) {
        if (resource->isDisabled())
            continue;
        QStandardItem *item = new QStandardItem(getResourceName(resource));
        item->setData(resource->getUniqueId());
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        QList<QStandardItem *> row;
        row << item;
        m_eventResourcesModel->appendRow(row);
    }
    m_eventResourcesModel->sort(0);
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
}

void QnBusinessRuleWidget::initEventParameters(BusinessEventType::Value eventType) {
    if (m_eventParameters) {
        ui->eventParamsLayout->removeWidget(m_eventParameters);
        //ui->eventLayout->removeWidget(m_eventParameters);
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
        //ui->eventLayout->addWidget(m_eventParameters);
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

void QnBusinessRuleWidget::initActionResources(BusinessActionType::Value actionType) {
    //TODO: update list on respool changed, keep selected if any
    //TODO: think about opening list of rules before resources are loaded (or when offline)
    m_actionResourcesModel->clear();

    // all actions that do require resource are require camera resource
    Q_UNUSED(actionType)
    QnResourceList resources = qnResPool->getResources().filtered<QnVirtualCameraResource>();

    foreach (const QnResourcePtr &resource, resources) {
        if (resource->isDisabled())
            continue;
        QStandardItem *item = new QStandardItem(getResourceName(resource));
        item->setData(resource->getUniqueId());
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        QList<QStandardItem *> row;
        row << item;
        m_actionResourcesModel->appendRow(row);
    }
    m_actionResourcesModel->sort(0);
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

BusinessEventType::Value QnBusinessRuleWidget::getCurrentEventType() const {
    int typeIdx = m_eventTypesModel->item(ui->eventTypeComboBox->currentIndex())->data().toInt();
    return (BusinessEventType::Value)typeIdx;
}

QnResourcePtr QnBusinessRuleWidget::getCurrentEventResource() const {
    QString uniqueId = m_eventResourcesModel->item(ui->eventResourceComboBox->currentIndex())->data().toString();
    return qnResPool->getResourceByUniqId(uniqueId);
}

ToggleState::Value QnBusinessRuleWidget::getCurrentEventToggleState() const {
    int typeIdx = m_eventStatesModel->item(ui->eventStatesComboBox->currentIndex())->data().toInt();
    return (ToggleState::Value)typeIdx;
}


BusinessActionType::Value QnBusinessRuleWidget::getCurrentActionType() const {
    int typeIdx = m_actionTypesModel->item(ui->actionTypeComboBox->currentIndex())->data().toInt();
    return (BusinessActionType::Value)typeIdx;
}

QnResourcePtr QnBusinessRuleWidget::getCurrentActionResource() const {
    QString uniqueId = m_actionResourcesModel->item(ui->actionResourceComboBox->currentIndex())->data().toString();
    return qnResPool->getResourceByUniqId(uniqueId);
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

    if (m_rule->eventResource()) {
        QModelIndexList eventResourceIdx = m_eventResourcesModel->match(m_eventResourcesModel->index(0, 0), Qt::UserRole + 1,
                                                                        m_rule->eventResource()->getUniqueId());
        ui->eventResourceComboBox->setCurrentIndex(eventResourceIdx.isEmpty() ? 0 : eventResourceIdx.first().row());
    } else {
        ui->eventResourceComboBox->setCurrentIndex(0);
    }

    ToggleState::Value toggleState = BusinessEventParameters::getToggleState(m_rule->eventParams());
    QModelIndexList stateIdx = m_eventStatesModel->match(m_eventStatesModel->index(0, 0), Qt::UserRole + 1, (int)toggleState);
    ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());

    QModelIndexList actionTypeIdx = m_actionTypesModel->match(m_actionTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->actionType());
    ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

    if (m_rule->actionResource()) {
        QModelIndexList actionResourceIdx = m_actionResourcesModel->match(m_actionResourcesModel->index(0, 0), Qt::UserRole + 1,
                                                                          m_rule->actionResource()->getUniqueId());
        ui->actionResourceComboBox->setCurrentIndex(actionResourceIdx.isEmpty() ? 0 : actionResourceIdx.first().row());
    } else {
        ui->actionResourceComboBox->setCurrentIndex(0);
    }


    if (m_eventParameters)
        m_eventParameters->loadParameters(m_rule->eventParams());
    //TODO: setup widget depending on resource, e.g. max fps or channel list

    if (m_actionParameters)
        m_actionParameters->loadParameters(m_rule->actionParams());
    //TODO: setup widget depending on resource, e.g. max fps or channel list

    emit eventTypeChanged(this, m_rule->eventType());
    emit eventResourceChanged(this, m_rule->eventResource());
    emit eventStateChanged(this, toggleState);
    emit actionTypeChanged(this, m_rule->actionType());
    emit actionResourceChanged(this, m_rule->actionResource());

    setHasChanges(false);
}

void QnBusinessRuleWidget::updateResources() {
    BusinessEventType::Value eventType = getCurrentEventType();
    if (BusinessEventType::isResourceRequired(eventType))
        initEventResources(eventType);

    BusinessActionType::Value actionType = getCurrentActionType();
    if (BusinessActionType::isResourceRequired(actionType))
        initActionResources(actionType);
}

void QnBusinessRuleWidget::apply() {
    //TODO: validate params!

    m_rule->setEventType(getCurrentEventType());

    if (BusinessEventType::isResourceRequired(m_rule->eventType())) {
        m_rule->setEventResource(getCurrentEventResource());
    } else {
        m_rule->setEventResource(QnResourcePtr());
    }

    QnBusinessParams eventParams;
    if (m_eventParameters)
        eventParams = m_eventParameters->parameters();
    BusinessEventParameters::setToggleState(&eventParams, getCurrentEventToggleState());
    m_rule->setEventParams(eventParams);

    m_rule->setActionType(getCurrentActionType());

    if (BusinessActionType::isResourceRequired(m_rule->actionType())) {
        m_rule->setActionResource(getCurrentActionResource());
    } else {
        m_rule->setActionResource(QnResourcePtr());
    }

    if (m_actionParameters)
        m_rule->setActionParams(m_actionParameters->parameters());
}

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventTypesModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;

    bool isResourceRequired = BusinessEventType::isResourceRequired(val);
    ui->eventAtLabel->setVisible(isResourceRequired);
    ui->eventResourceComboBox->setVisible(isResourceRequired);
    if (isResourceRequired)
        initEventResources(val);
    else
        emit eventResourceChanged(this, QnResourcePtr());
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

void QnBusinessRuleWidget::at_eventResourceComboBox_currentIndexChanged(int index) {
    setHasChanges(true);

    QString uniqueId = m_eventResourcesModel->item(index)->data().toString();
    emit eventResourceChanged(this, qnResPool->getResourceByUniqId(uniqueId));
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_actionTypesModel->item(index)->data().toInt();
    BusinessActionType::Value val = (BusinessActionType::Value)typeIdx;

    bool isResourceRequired = BusinessActionType::isResourceRequired(val);
    ui->actionAtLabel->setVisible(isResourceRequired);
    ui->actionResourceComboBox->setVisible(isResourceRequired);
    if (isResourceRequired)
        initActionResources(val);
    else
        emit actionResourceChanged(this, QnResourcePtr());

    initActionParameters(val);

    setHasChanges(true);
    emit actionTypeChanged(this, val);
}

void QnBusinessRuleWidget::at_actionResourceComboBox_currentIndexChanged(int index) {
    setHasChanges(true);

    QString uniqueId = m_actionResourcesModel->item(index)->data().toString();
    emit actionResourceChanged(this, qnResPool->getResourceByUniqId(uniqueId));
}
