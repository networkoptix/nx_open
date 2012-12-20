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

#include <events/abstract_business_event.h>

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

} // namespace

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_expanded(false),
    m_rule(0),
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

    QPalette pal = ui->editFrame->palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Window).lighter());
    ui->editFrame->setPalette(pal);
    ui->editFrame->setVisible(m_expanded);

    connect(ui->eventTypeComboBox,      SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,    SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->actionTypeComboBox,     SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));
    //TODO: setup onResourceChanged to update widgets depending on resource, e.g. max fps or channel list

    connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));
    connect(ui->resetButton,  SIGNAL(clicked()), this, SLOT(resetFromRule()));

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->applyButton,  SIGNAL(clicked()), this, SLOT(at_applyButton_clicked()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));

    initEventTypes();
    updateDisplay();
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
}

void QnBusinessRuleWidget::setRule(QnBusinessEventRulePtr rule) {
    m_rule = rule;
    resetFromRule();
    updateDisplay();
}

QnBusinessEventRulePtr QnBusinessRuleWidget::getRule() const {
    return m_rule;
}

void QnBusinessRuleWidget::setExpanded(bool expanded) {
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    ui->editFrame->setVisible(expanded);

    QIcon icon;
    if (m_expanded)
        icon.addFile(QString::fromUtf8(":/skin/titlebar/unfullscreen_hovered.png"));
    else
        icon.addFile(QString::fromUtf8(":/skin/titlebar/fullscreen_hovered.png"));
    ui->expandButton->setIcon(icon);
    if (expanded)
        resetFromRule();
}

bool QnBusinessRuleWidget::expanded() const {
    return m_expanded;
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
    if (BusinessEventType::hasToggleState(eventType))
        values << ToggleState::Any << ToggleState::On << ToggleState::Off;

    foreach (ToggleState::Value val, values) {
        QStandardItem *item = new QStandardItem(ToggleState::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initEventParameters(BusinessEventType::Value eventType) {
    if (m_eventParameters) {
        ui->eventLayout->removeWidget(m_eventParameters);
        m_eventParameters->setVisible(false);
    }

    if (m_eventWidgetsByType.contains(eventType)) {
        m_eventParameters = m_eventWidgetsByType.find(eventType).value();
    } else {
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(eventType, this);
        m_eventWidgetsByType[eventType] = m_eventParameters;
    }
    if (m_eventParameters) {
        ui->eventLayout->addWidget(m_eventParameters);
        m_eventParameters->setVisible(true);
    }
}

void QnBusinessRuleWidget::initActionTypes(ToggleState::Value eventState) {

    m_actionTypesModel->clear();
    // what type of actions to show: prolonged or instant
    bool instantActionsFilter = (eventState != ToggleState::NotDefined) || (!BusinessEventType::hasToggleState(getCurrentEventType()));

    for (int i = BusinessActionType::BA_FirstType; i <= BusinessActionType::BA_LastType; i++) {
        BusinessActionType::Value val = (BusinessActionType::Value)i;

        if (BusinessActionType::hasToggleState(val) == instantActionsFilter)
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
        ui->actionLayout->removeWidget(m_actionParameters);
        m_actionParameters->setVisible(false);
    }

    if (m_actionWidgetsByType.contains(actionType)) {
        m_actionParameters = m_actionWidgetsByType.find(actionType).value();
    } else {
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(actionType, this);
        m_actionWidgetsByType[actionType] = m_actionParameters;
    }

    if (m_actionParameters) {
        ui->actionLayout->addWidget(m_actionParameters);
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

// Handlers

void QnBusinessRuleWidget::updateDisplay() {
    ui->summaryFrame->setVisible(m_rule);
    ui->deleteButton->setVisible(m_rule);
    if (m_rule)
        updateSummary();
}

void QnBusinessRuleWidget::updateSummary() {
    if (!m_rule)
        return;

    QLatin1String formatString("<html><head/><body><p>%1: %2</p></body></html>");

    QLatin1String eventString("%1 %2 %3");
    QLatin1String actionString("%1 %2");

    QLatin1String cameraString("at <img src=\":/skin/tree/camera.png\" width=\"16\" height=\"16\"/>"\
            "<span style=\" font-style:italic;\">%1</span>");

    QString eventResource = BusinessEventType::isResourceRequired(m_rule->eventType())
            ? QString(cameraString).arg(getResourceName(m_rule->eventResource()))
            : QString();

    QString actionResource = BusinessActionType::isResourceRequired(m_rule->actionType())
            ? QString(cameraString).arg(getResourceName(m_rule->actionResource()))
            : QString();

    QString eventSummary = QString(eventString)
            .arg(m_eventParameters ? m_eventParameters->description()
                                   : BusinessEventType::toString(m_rule->eventType()))
            .arg(ToggleState::toString(BusinessEventParameters::getToggleState(m_rule->eventParams())))
            .arg(eventResource);

    QString actionSummary = QString(actionString)
            .arg(m_actionParameters ? m_actionParameters->description()
                                    : BusinessActionType::toString(m_rule->actionType()))
            .arg(actionResource);

    QString summary = QString(formatString)
            .arg(eventSummary)
            .arg(actionSummary);


    ui->summaryLabel->setText(summary);
}

void QnBusinessRuleWidget::resetFromRule() {
    if (m_rule) {
        QModelIndexList eventTypeIdx = m_eventTypesModel->match(m_eventTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->eventType());
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        if (m_rule->eventResource()) {
            QModelIndexList eventResourceIdx = m_eventResourcesModel->match(m_eventResourcesModel->index(0, 0), Qt::UserRole + 1,
                                                                            m_rule->eventResource()->getUniqueId());
            ui->eventResourceComboBox->setCurrentIndex(eventResourceIdx.isEmpty() ? 0 : eventResourceIdx.first().row());
        }

        QModelIndexList stateIdx = m_eventStatesModel->match(m_eventStatesModel->index(0, 0), Qt::UserRole + 1, (int)BusinessEventParameters::getToggleState(m_rule->eventParams()));
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());

        QModelIndexList actionTypeIdx = m_actionTypesModel->match(m_actionTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->actionType());
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

        if (m_rule->actionResource()) {
            QModelIndexList actionResourceIdx = m_actionResourcesModel->match(m_actionResourcesModel->index(0, 0), Qt::UserRole + 1,
                                                                              m_rule->actionResource()->getUniqueId());
            ui->actionResourceComboBox->setCurrentIndex(actionResourceIdx.isEmpty() ? 0 : actionResourceIdx.first().row());
        }

        if (m_eventParameters)
            m_eventParameters->loadParameters(m_rule->eventParams());
        //TODO: setup widget depending on resource, e.g. max fps or channel list

        if (m_actionParameters)
            m_actionParameters->loadParameters(m_rule->actionParams());
        //TODO: setup widget depending on resource, e.g. max fps or channel list
    } else {
        ui->eventTypeComboBox->setCurrentIndex(0);
        ui->eventStatesComboBox->setCurrentIndex(0);
        ui->actionTypeComboBox->setCurrentIndex(0);
    }
}

void QnBusinessRuleWidget::at_expandButton_clicked() {
    setExpanded(!m_expanded);
}

void QnBusinessRuleWidget::at_deleteButton_clicked() {
    if (QMessageBox::question(this,
                              tr("Confirm rule deletion"),
                              tr("Are you sure you want to delete this rule?"),
                              QMessageBox::Ok,
                              QMessageBox::Cancel) == QMessageBox::Cancel)
        return;
    emit deleteConfirmed(this, m_rule);
}

void QnBusinessRuleWidget::at_applyButton_clicked() {
    QnBusinessEventRulePtr rule = m_rule ? m_rule : QnBusinessEventRulePtr(new QnBusinessEventRule);

    rule->setEventType(getCurrentEventType());

    if (BusinessEventType::isResourceRequired(rule->eventType())) {
        rule->setEventResource(getCurrentEventResource());
    } else {
        rule->setEventResource(QnResourcePtr());
    }

    QnBusinessParams eventParams;
    if (m_eventParameters)
        eventParams = m_eventParameters->parameters();
    BusinessEventParameters::setToggleState(&eventParams, getCurrentEventToggleState());
    rule->setEventParams(eventParams);

    rule->setActionType(getCurrentActionType());

    if (BusinessActionType::isResourceRequired(rule->actionType())) {
        rule->setActionResource(getCurrentActionResource());
    } else {
        rule->setActionResource(QnResourcePtr());
    }

    if (m_actionParameters)
        rule->setActionParams(m_actionParameters->parameters());

    emit apply(this, rule);
    updateSummary();
}

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventTypesModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;

    bool isResourceRequired = BusinessEventType::isResourceRequired(val);
    ui->eventAtLabel->setVisible(isResourceRequired);
    ui->eventResourceComboBox->setVisible(isResourceRequired);
    if (isResourceRequired)
        initEventResources(val);
    initEventStates(val);
    initEventParameters(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventStatesModel->item(index)->data().toInt();
    ToggleState::Value val = (ToggleState::Value)typeIdx;

    initActionTypes(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_actionTypesModel->item(index)->data().toInt();
    BusinessActionType::Value val = (BusinessActionType::Value)typeIdx;

    bool isResourceRequired = BusinessActionType::isResourceRequired(val);
    ui->actionAtLabel->setVisible(isResourceRequired);
    ui->actionResourceComboBox->setVisible(isResourceRequired);
    if (isResourceRequired)
        initActionResources(val);

    initActionParameters(val);
}
