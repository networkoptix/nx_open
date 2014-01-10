#include "business_rule_item_delegate.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QComboBox>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <business/events/motion_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/camera_output_business_action.h>
#include <business/actions/sendmail_business_action.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_criterion.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/models/business_rules_view_model.h>
#include <ui/models/notification_sound_model.h>
#include <ui/style/globals.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectResourcesDialogButton ------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget *parent):
    base_type(parent),
    m_dialogDelegate(NULL),
    m_target(QnResourceSelectionDialog::CameraResourceTarget)
{
    connect(this, SIGNAL(clicked()), this, SLOT(at_clicked()));
}

QnResourceList QnSelectResourcesDialogButton::resources() const {
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(QnResourceList resources) {
    m_resources = resources;
}

QnResourceSelectionDialogDelegate* QnSelectResourcesDialogButton::dialogDelegate() const {
    return m_dialogDelegate;
}

void QnSelectResourcesDialogButton::setDialogDelegate(QnResourceSelectionDialogDelegate* delegate) {
    m_dialogDelegate = delegate;
}

QnResourceSelectionDialog::SelectionTarget  QnSelectResourcesDialogButton::selectionTarget() const {
    return m_target;
}

void QnSelectResourcesDialogButton::setSelectionTarget(QnResourceSelectionDialog::SelectionTarget target) {
    m_target = target;
}

void QnSelectResourcesDialogButton::at_clicked() {
    QnResourceSelectionDialog dialog(m_target, this);
    dialog.setSelectedResources(m_resources);
    dialog.setDelegate(m_dialogDelegate);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
        return;
    m_resources = dialog.selectedResources();
    emit commit();
}

void QnSelectResourcesDialogButton::initStyleOption(QStyleOptionButton *option) const {
    base_type::initStyleOption(option);
}

void QnSelectResourcesDialogButton::paintEvent(QPaintEvent *event) {
    base_type::paintEvent(event);
}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnBusinessRuleItemDelegate ---------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////
QnBusinessRuleItemDelegate::QnBusinessRuleItemDelegate(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
}

QnBusinessRuleItemDelegate::~QnBusinessRuleItemDelegate() {

}

QSize QnBusinessRuleItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QSize sh = base_type::sizeHint(option, index);
    if (index.column() == QnBusiness::EventColumn || index.column() == QnBusiness::ActionColumn)
        sh.setWidth(sh.width() * 1.5);
    return sh;
}

void QnBusinessRuleItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const  {
    base_type::initStyleOption(option, index);
    if (index.data(Qn::DisabledRole).toBool()) {
        if (QStyleOptionViewItemV4 *vopt = qstyleoption_cast<QStyleOptionViewItemV4 *>(option)) {
            vopt->state &= ~QStyle::State_Enabled;
        }
        option->palette.setColor(QPalette::Highlight, qnGlobals->businessRuleDisabledHighlightColor());
    } else if (!index.data(Qn::ValidRole).toBool()) {
        QColor clr = index.data(Qt::BackgroundRole).value<QColor>();
        option->palette.setColor(QPalette::Highlight, clr.lighter());
    }
}

QWidget* QnBusinessRuleItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    switch (index.column()) {
    case QnBusiness::SourceColumn:
    {
        QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
        //TODO: #GDM server selection dialog?
        connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

        BusinessEventType::Value eventType = (BusinessEventType::Value)index.data(Qn::EventTypeRole).toInt();
        if (eventType == BusinessEventType::Camera_Motion)
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraMotionAllowedPolicy>(btn));
        else if (eventType == BusinessEventType::Camera_Input)
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraInputAllowedPolicy>(btn));

        return btn;
    }
    case QnBusiness::TargetColumn:
    {
        BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(Qn::ActionTypeRole).toInt();

        switch (actionType) {
        case BusinessActionType::ShowPopup:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->addItem(tr("For All Users"), QnBusinessActionParameters::EveryOne);
            comboBox->addItem(tr("For Administrators Only"), QnBusinessActionParameters::AdminOnly);
            return comboBox;
        }
        case BusinessActionType::PlaySound:
        case BusinessActionType::PlaySoundRepeated:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->setModel(context()->instance<QnAppServerNotificationCache>()->persistentGuiModel());
            return comboBox;
        }
        case BusinessActionType::SayText:
            return base_type::createEditor(parent, option, index);
        default:
            break;
        }

        QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
        connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

        if (actionType == BusinessActionType::CameraRecording) {
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraRecordingAllowedPolicy>(btn));
        }
        else if (actionType == BusinessActionType::CameraOutput || actionType == BusinessActionType::CameraOutputInstant) {
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraOutputAllowedPolicy>(btn));
        }
        else if (actionType == BusinessActionType::SendMail) {
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnUserEmailAllowedPolicy>(btn));
            btn->setSelectionTarget(QnResourceSelectionDialog::UserResourceTarget);
        }
        return btn;
    }
    case QnBusiness::EventColumn:
    {
        QComboBox* comboBox = new QComboBox(parent);
        for (int i = 0; i < BusinessEventType::Count; i++) {
            BusinessEventType::Value val = (BusinessEventType::Value)i;
            comboBox->addItem(QnBusinessStringsHelper::eventName(val), val);
        }
        return comboBox;
    }
    case QnBusiness::ActionColumn:
    {
        bool instant = index.data(Qn::ActionIsInstantRole).toBool();
        QComboBox* comboBox = new QComboBox(parent);
        for (int i = 0; i < BusinessActionType::Count; i++) {
            BusinessActionType::Value val = (BusinessActionType::Value)i;
            if (instant && BusinessActionType::hasToggleState(val))
                continue;
            comboBox->addItem(BusinessActionType::toString(val), val);
        }
        return comboBox;
    }
    case QnBusiness::AggregationColumn:
    {
        QnAggregationWidget* widget = new QnAggregationWidget(parent);
        widget->setShort(true);
        return widget;
    }
    default:
        break;
    }

    return base_type::createEditor(parent, option, index);
}


void QnBusinessRuleItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    switch (index.column()) {
    case QnBusiness::SourceColumn:
    {
        if(QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
            btn->setResources(index.data(Qn::EventResourcesRole).value<QnResourceList>());
            btn->setText(index.data(Qn::ShortTextRole).toString());
            return;
        }
        break;
    }
    case QnBusiness::TargetColumn:
    {
        BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(Qn::ActionTypeRole).toInt();

        switch (actionType) {
        case BusinessActionType::ShowPopup:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        case BusinessActionType::PlaySound:
        case BusinessActionType::PlaySoundRepeated:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
                comboBox->setCurrentIndex(soundModel->rowByFilename(index.data(Qt::EditRole).toString()));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        case BusinessActionType::SayText:
        {
           base_type::setEditorData(editor, index);
           return;
        }
        default:
            break;
        }

        if(QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
            btn->setResources(index.data(Qn::ActionResourcesRole).value<QnResourceList>());
            btn->setText(index.data(Qn::ShortTextRole).toString());
            return;
        }
        break;
    }
    case QnBusiness::EventColumn:
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
            comboBox->setCurrentIndex(comboBox->findData(index.data(Qn::EventTypeRole)));
            connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
        }
        return;
    case QnBusiness::ActionColumn:
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
            comboBox->setCurrentIndex(comboBox->findData(index.data(Qn::ActionTypeRole)));
            connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
        }
        return;
    case QnBusiness::AggregationColumn:
        if (QnAggregationWidget* widget = dynamic_cast<QnAggregationWidget *>(editor)) {
            widget->setValue(index.data(Qt::EditRole).toInt());
            connect(widget, SIGNAL(valueChanged()), this, SLOT(at_editor_commit()));
        }
        return;
    default:
        break;
    }

    base_type::setEditorData(editor, index);
}

void QnBusinessRuleItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    switch (index.column()) {
    case QnBusiness::SourceColumn:
    {
        if(QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
            model->setData(index, QVariant::fromValue<QnResourceList>(btn->resources()));
            return;
        }

        break;
    }
    case QnBusiness::TargetColumn:
    {
        BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(Qn::ActionTypeRole).toInt();

        switch (actionType) {
        case BusinessActionType::ShowPopup:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                model->setData(index, comboBox->itemData(comboBox->currentIndex()));
            }
            return;
        }
        case BusinessActionType::PlaySound:
        case BusinessActionType::PlaySoundRepeated:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
                if (!soundModel->loaded())
                    return;
                QString filename = soundModel->filenameByRow(comboBox->currentIndex());
                model->setData(index, filename);
            }
            return;
        }
        case BusinessActionType::SayText:
        {
            base_type::setModelData(editor, model, index);
            return;
        }
        default:
            break;
        }

        if(QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
            model->setData(index, QVariant::fromValue<QnResourceList>(btn->resources()));
            return;
        }

        break;
    }
    case QnBusiness::EventColumn:
    case QnBusiness::ActionColumn:
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
            model->setData(index, comboBox->itemData(comboBox->currentIndex()));
        }
        return;
    case QnBusiness::AggregationColumn:
        if (QnAggregationWidget* widget = dynamic_cast<QnAggregationWidget *>(editor)) {
            model->setData(index, widget->value());
        }
        return;
    default:
        break;
    }
    base_type::setModelData(editor, model, index);
}

bool QnBusinessRuleItemDelegate::eventFilter(QObject *object, QEvent *event) {
    QComboBox *editor = qobject_cast<QComboBox*>(object);
    if (editor && event->type() == QEvent::FocusOut)
        return false;
    return base_type::eventFilter(object, event);
}

void QnBusinessRuleItemDelegate::at_editor_commit() {
    if (QWidget* w = dynamic_cast<QWidget*> (sender()))
        emit commitData(w);
}
