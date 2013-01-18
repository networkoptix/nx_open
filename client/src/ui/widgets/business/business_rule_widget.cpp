#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>
#include <QtGui/QMessageBox>
#include <QtGui/QIcon>

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

    // make sure size is equal to ui->aggregationComboBox->count()
    const int aggregationSteps[] = {
        1,                    /* 1 second. */
        60,                   /* 1 minute. */
        60 * 60,              /* 1 hour. */
        60 * 60 * 24          /* 1 day. */
    };

} // namespace

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_model(NULL),
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

    ui->eventDefinitionFrame->installEventFilter(this);
    ui->actionDefinitionFrame->installEventFilter(this);

    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Window).lighter());
    this->setPalette(pal);

    connect(ui->eventTypeComboBox,          SIGNAL(currentIndexChanged(int)),   this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,        SIGNAL(currentIndexChanged(int)),   this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->eventResourcesHolder,       SIGNAL(clicked()),                  this, SLOT(at_eventResourcesHolder_clicked()));
    connect(ui->actionResourcesHolder,      SIGNAL(clicked()),                  this, SLOT(at_actionResourcesHolder_clicked()));

    connect(ui->actionTypeComboBox,         SIGNAL(currentIndexChanged(int)),   this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    connect(ui->aggregationCheckBox,        SIGNAL(toggled(bool)),              this, SLOT(at_aggregationPeriodChanged()));
    connect(ui->aggregationValueSpinBox,    SIGNAL(editingFinished()),          this, SLOT(at_aggregationPeriodChanged()));
    connect(ui->aggregationPeriodComboBox,  SIGNAL(currentIndexChanged(int)),   this, SLOT(at_aggregationPeriodChanged()));

    connect(ui->aggregationCheckBox, SIGNAL(toggled(bool)), ui->aggregationValueSpinBox, SLOT(setEnabled(bool)));
    connect(ui->aggregationCheckBox, SIGNAL(toggled(bool)), ui->aggregationPeriodComboBox, SLOT(setEnabled(bool)));

    //TODO: connect notifyChaged on subitems
    //TODO: setup onResourceChanged to update widgets depending on resource, e.g. max fps or channel list

    setVisible(false);
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
}

void QnBusinessRuleWidget::setModel(QnBusinessRuleViewModel *model) {
    if (m_model)
        disconnect(m_model, 0, this, 0);

    m_model = model;
    setVisible(m_model);
    if (!m_model)
        return;

    connect(m_model, SIGNAL(dataChanged(QnBusinessRuleViewModel*, QnBusinessRuleViewModel::Fields)),
            this, SLOT(at_model_dataChanged(QnBusinessRuleViewModel*, QnBusinessRuleViewModel::Fields)));
    at_model_dataChanged(m_model, QnBusiness::AllFieldsMask);
}

void QnBusinessRuleWidget::at_model_dataChanged(QnBusinessRuleViewModel *model,  QnBusiness::Fields fields) {
    if (!model || m_model != model)
        return;

    if (fields & QnBusiness::EventTypeField) {
        initEventTypes();

        QModelIndexList eventTypeIdx = m_eventTypesModel->match(m_eventTypesModel->index(0, 0), Qt::UserRole + 1, (int)model->eventType());
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());
    }

    //TODO: dependencies
    if (fields & QnBusiness::EventStateField) {
        initEventStates();

        QModelIndexList stateIdx = m_eventStatesModel->match(m_eventStatesModel->index(0, 0), Qt::UserRole + 1, (int)model->eventState());
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());
    }

    if (fields & QnBusiness::EventResourcesField) {
        updateEventResources();
    }

    if (fields & QnBusiness::EventParamsField) {
        initEventParameters();
    }

    if (fields & QnBusiness::ActionTypeField) {
        QModelIndexList actionTypeIdx = m_actionTypesModel->match(m_actionTypesModel->index(0, 0), Qt::UserRole + 1, (int)model->actionType());
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());
    }

    if (fields & QnBusiness::ActionResourcesField) {
        updateActionResources();
    }

    if (fields & QnBusiness::ActionParamsField) {
        initActionParameters();
    }

    if (fields & QnBusiness::AggregationField) {
        int msecs = model->aggregationPeriod();
        ui->aggregationCheckBox->setChecked(msecs > 0);
        if (msecs > 0) {
            int idx = 0;
            while (idx < ui->aggregationPeriodComboBox->count() - 1
                   && msecs >= aggregationSteps[idx+1]
                   && msecs % aggregationSteps[idx+1] == 0)
                idx++;

            ui->aggregationPeriodComboBox->setCurrentIndex(idx);
            ui->aggregationValueSpinBox->setValue(msecs / aggregationSteps[idx]);
        }
    }
    //TODO: setup widget depending on resource, e.g. max fps or channel list
}

void QnBusinessRuleWidget::initEventTypes() {
    if (!m_model)
        return;

    m_eventTypesModel->clear();
    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        BusinessEventType::Value val = (BusinessEventType::Value)i;

        QStandardItem *item = new QStandardItem(BusinessEventType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initEventStates() {
    if (!m_model)
        return;

    m_eventStatesModel->clear();

    QList<ToggleState::Value> values;
    values << ToggleState::NotDefined;
    bool prolonged = BusinessEventType::hasToggleState(m_model->eventType());
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

void QnBusinessRuleWidget::initEventParameters() {
    // TODO: connect/disconnect

    if (m_eventParameters) {
        ui->eventParamsLayout->removeWidget(m_eventParameters);
        m_eventParameters->setVisible(false);
    }

    if (!m_model)
        return;

    if (m_eventWidgetsByType.contains(m_model->eventType())) {
        m_eventParameters = m_eventWidgetsByType.find(m_model->eventType()).value();
    } else {
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(m_model->eventType(), this, context());
        m_eventWidgetsByType[m_model->eventType()] = m_eventParameters;
    }
    if (m_eventParameters) {
        ui->eventParamsLayout->addWidget(m_eventParameters);
        m_eventParameters->setVisible(true);

        m_eventParameters->loadParameters(m_model->eventParams());
    }
}

void QnBusinessRuleWidget::initActionTypes() {
    if (!m_model)
        return;

    m_actionTypesModel->clear();
    // what type of actions to show: prolonged or instant
    bool onlyInstantActions = (m_model->eventState() == ToggleState::On || m_model->eventState() == ToggleState::Off)
            || (!BusinessEventType::hasToggleState(m_model->eventType()));

    for (int i = 0; i < BusinessActionType::BA_Count; i++) {
        BusinessActionType::Value val = (BusinessActionType::Value)i;

        if (BusinessActionType::hasToggleState(val) && onlyInstantActions)
            continue;

        QStandardItem *item = new QStandardItem(BusinessActionType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }

    ui->aggregationCheckBox->setVisible(onlyInstantActions);
    ui->aggregationValueSpinBox->setVisible(onlyInstantActions);
    ui->aggregationPeriodComboBox->setVisible(onlyInstantActions);

}

void QnBusinessRuleWidget::initActionParameters() {
    if (m_actionParameters) {
        //ui->actionLayout->removeWidget(m_actionParameters);
        ui->actionParamsLayout->removeWidget(m_actionParameters);
        m_actionParameters->setVisible(false);
        disconnect(m_actionParameters, 0, this, 0);
    }

    if (!m_model)
        return;

    if (m_actionWidgetsByType.contains(m_model->actionType())) {
        m_actionParameters = m_actionWidgetsByType.find(m_model->actionType()).value();
    } else {
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(m_model->actionType(), this, context());
        m_actionWidgetsByType[m_model->actionType()] = m_actionParameters;
    }

    if (m_actionParameters) {
        //ui->actionLayout->addWidget(m_actionParameters);
        ui->actionParamsLayout->addWidget(m_actionParameters);
        m_actionParameters->setVisible(true);
        m_actionParameters->loadParameters(m_model->actionParams());
    }
}

bool QnBusinessRuleWidget::eventFilter(QObject *object, QEvent *event) {
    if (!m_model)
        return base_type::eventFilter(object, event);;

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
                QnResourceList resources = m_model->eventResources();
                foreach(QnResourcePtr res, m_dropResources) {
                    if (resources.contains(res))
                        continue;
                    resources.append(res);
                }
                m_model->setEventResources(resources);
            } else if (object == ui->actionDefinitionFrame) {
                QnResourceList resources = m_model->actionResources();
                foreach(QnResourcePtr res, m_dropResources) {
                    if (resources.contains(res))
                        continue;
                    resources.append(res);
                }
                m_model->setActionResources(resources);
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

// Handlers

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    if (!m_model)
        return;

    int typeIdx = m_eventTypesModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;
    m_model->setEventType(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index) {
    if (!m_model)
        return;

    int typeIdx = m_eventStatesModel->item(index)->data().toInt();
    ToggleState::Value val = (ToggleState::Value)typeIdx;
    m_model->setEventState(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    if (!m_model)
        return;

    int typeIdx = m_actionTypesModel->item(index)->data().toInt();
    BusinessActionType::Value val = (BusinessActionType::Value)typeIdx;
    m_model->setActionType(val);
}

void QnBusinessRuleWidget::at_eventResourcesHolder_clicked() {
    if (!m_model)
        return;

    QnSelectCamerasDialog dialog(this, context()); //TODO: #GDM or servers?
    dialog.setSelectedResources(m_model->eventResources());
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_model->setEventResources(dialog.getSelectedResources());
}

void QnBusinessRuleWidget::at_actionResourcesHolder_clicked() {
    if (!m_model)
        return;

    QnSelectCamerasDialog dialog(this, context());
    dialog.setSelectedResources(m_model->actionResources());
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_model->setActionResources(dialog.getSelectedResources());
}

void QnBusinessRuleWidget::at_aggregationPeriodChanged() {
    if (!m_model)
        return;

    int val = ui->aggregationCheckBox->isChecked() ? ui->aggregationValueSpinBox->value() : 0;
    int idx = ui->aggregationPeriodComboBox->currentIndex();
    m_model->setAggregationPeriod(val * aggregationSteps[idx]);
}

void QnBusinessRuleWidget::updateEventResources() {
    if (!m_model)
        return;

    QPushButton* item = ui->eventResourcesHolder;

    bool isResourceRequired = BusinessEventType::isResourceRequired(m_model->eventType());
    item->setVisible(isResourceRequired);
    ui->eventAtLabel->setVisible(isResourceRequired);
    ui->eventDropLabel->setVisible(isResourceRequired);

    if (isResourceRequired) {
        item->setText(m_model->data(2).toString());
        item->setIcon(m_model->data(2, Qt::DecorationRole).value<QIcon>());
    }

}

void QnBusinessRuleWidget::updateActionResources() {
    if (!m_model)
        return;

    QPushButton* item = ui->actionResourcesHolder;

    bool isResourceRequired = BusinessActionType::isResourceRequired(m_model->actionType());
    item->setVisible(isResourceRequired);
    ui->actionAtLabel->setVisible(isResourceRequired);
    ui->actionDropLabel->setVisible(isResourceRequired);

    if (isResourceRequired) {
        item->setText(m_model->data(5).toString()); //TODO: UserRole? do not show fps ot other details here
        item->setIcon(m_model->data(5, Qt::DecorationRole).value<QIcon>());
    }
}
