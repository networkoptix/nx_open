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
    m_actionParameters(NULL)
{
    ui->setupUi(this);

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
    if (!m_model) {
        ui->eventTypeComboBox->setModel(NULL);
        ui->eventStatesComboBox->setModel(NULL);
        ui->actionTypeComboBox->setModel(NULL);
        return;
    }

    ui->eventTypeComboBox->setModel(m_model->eventTypesModel());
    ui->eventStatesComboBox->setModel(m_model->eventStatesModel());
    ui->actionTypeComboBox->setModel(m_model->actionTypesModel());

    connect(m_model, SIGNAL(dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)),
            this, SLOT(at_model_dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)));
    at_model_dataChanged(m_model, QnBusiness::AllFieldsMask);
}

void QnBusinessRuleWidget::at_model_dataChanged(QnBusinessRuleViewModel *model,  QnBusiness::Fields fields) {
    if (!model || m_model != model)
        return;

    if (fields & QnBusiness::EventTypeField) {

        QModelIndexList eventTypeIdx = m_model->eventTypesModel()->match(
                    m_model->eventTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventType());
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        bool prolonged = BusinessEventType::hasToggleState(m_model->eventType());
        ui->eventStatesComboBox->setVisible(prolonged);

        bool isResourceRequired = BusinessEventType::isResourceRequired(m_model->eventType());
        ui->eventResourcesHolder->setVisible(isResourceRequired);
        ui->eventAtLabel->setVisible(isResourceRequired);
        ui->eventDropLabel->setVisible(isResourceRequired);
    }

    //TODO: dependencies
    if (fields & QnBusiness::EventStateField) {
        QModelIndexList stateIdx = m_model->eventStatesModel()->match(
                    m_model->eventStatesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventState());
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());
    }

    if (fields & QnBusiness::ActionIsInstantField) {
        bool onlyInstantActions = m_model->actionTypeShouldBeInstant();

        ui->aggregationCheckBox->setVisible(onlyInstantActions);
        ui->aggregationValueSpinBox->setVisible(onlyInstantActions);
        ui->aggregationPeriodComboBox->setVisible(onlyInstantActions);
    }

    if (fields & QnBusiness::EventResourcesField) {
        ui->eventResourcesHolder->setText(m_model->data(QnBusiness::SourceColumn).toString());
        ui->eventResourcesHolder->setIcon(m_model->data(QnBusiness::SourceColumn, Qt::DecorationRole).value<QIcon>());
    }

    if (fields & QnBusiness::EventParamsField) {
        initEventParameters();
    }

    if (fields & QnBusiness::ActionTypeField) {
        QModelIndexList actionTypeIdx = m_model->actionTypesModel()->match(
                    m_model->actionTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->actionType());
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

        bool isResourceRequired = BusinessActionType::isResourceRequired(m_model->actionType());
        ui->actionResourcesHolder->setVisible(isResourceRequired);
        ui->actionAtLabel->setVisible(isResourceRequired);
        ui->actionDropLabel->setVisible(isResourceRequired);
    }

    if (fields & QnBusiness::ActionResourcesField) {
        ui->actionResourcesHolder->setText(m_model->data(QnBusiness::TargetColumn).toString()); //TODO: UserRole? do not show fps ot other details here
        ui->actionResourcesHolder->setIcon(m_model->data(QnBusiness::TargetColumn, Qt::DecorationRole).value<QIcon>());
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

    int typeIdx = m_model->eventTypesModel()->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;
    m_model->setEventType(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index) {
    if (!m_model)
        return;

    int typeIdx = m_model->eventStatesModel()->item(index)->data().toInt();
    ToggleState::Value val = (ToggleState::Value)typeIdx;
    m_model->setEventState(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    if (!m_model)
        return;

    int typeIdx = m_model->actionTypesModel()->item(index)->data().toInt();
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
