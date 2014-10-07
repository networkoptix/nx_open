#include "business_rule_item_delegate.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QComboBox>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <business/business_resource_validation.h>
//#include <business/events/motion_business_event.h>
//#include <business/events/camera_input_business_event.h>
//#include <business/actions/recording_business_action.h>
//#include <business/actions/camera_output_business_action.h>
//#include <business/actions/sendmail_business_action.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_criterion.h>

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
        option->palette.setColor(QPalette::Highlight, qnGlobals->businessRuleInvalidHighlightColor());
    }
}

QWidget* QnBusinessRuleItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    switch (index.column()) {
    case QnBusiness::SourceColumn:
    {
        QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
        //TODO: #GDM #Business server selection dialog?
        connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

        QnBusiness::EventType eventType = index.data(Qn::EventTypeRole).value<QnBusiness::EventType>();
        if (eventType == QnBusiness::CameraMotionEvent)
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraMotionPolicy>(btn));
        else if (eventType == QnBusiness::CameraInputEvent)
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraInputPolicy>(btn));

        return btn;
    }
    case QnBusiness::TargetColumn:
    {
        QnBusiness::ActionType actionType = index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>();

        switch (actionType) {
        case QnBusiness::ShowPopupAction:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->addItem(tr("For All Users"), QnBusinessActionParameters::EveryOne);
            comboBox->addItem(tr("For Administrators Only"), QnBusinessActionParameters::AdminOnly);
            return comboBox;
        }
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->setModel(context()->instance<QnAppServerNotificationCache>()->persistentGuiModel());
            return comboBox;
        }
        case QnBusiness::SayTextAction:
            return base_type::createEditor(parent, option, index);
        default:
            break;
        }

        QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
        connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

        if (actionType == QnBusiness::CameraRecordingAction) {
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraRecordingPolicy>(btn));
        }
        else if (actionType == QnBusiness::CameraOutputAction || actionType == QnBusiness::CameraOutputOnceAction) {
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraOutputPolicy>(btn));
        }
        else if (actionType == QnBusiness::SendMailAction) {
            btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnUserEmailPolicy>(btn));
            btn->setSelectionTarget(QnResourceSelectionDialog::UserResourceTarget);
        }
        return btn;
    }
    case QnBusiness::EventColumn:
    {
        QComboBox* comboBox = new QComboBox(parent);
        for (int i = 1; i < QnBusiness::EventCount; i++) {
            QnBusiness::EventType val = (QnBusiness::EventType) i;
            comboBox->addItem(QnBusinessStringsHelper::eventName(val), val);
        }
        return comboBox;
    }
    case QnBusiness::ActionColumn:
    {
        bool instant = index.data(Qn::ActionIsInstantRole).toBool();
        QComboBox* comboBox = new QComboBox(parent);
        for (int i = 1; i < QnBusiness::ActionCount; i++) {
            QnBusiness::ActionType val = (QnBusiness::ActionType)i;
            if (instant && QnBusiness::hasToggleState(val))
                continue;
            if (!QnBusiness::isImplemented(val))
                continue;
            comboBox->addItem(QnBusinessStringsHelper::actionName(val), val);
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
        QnBusiness::ActionType actionType = index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>();

        switch (actionType) {
        case QnBusiness::ShowPopupAction:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
                comboBox->setCurrentIndex(soundModel->rowByFilename(index.data(Qt::EditRole).toString()));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        case QnBusiness::SayTextAction:
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
            comboBox->setCurrentIndex(comboBox->findData(index.data(Qn::EventTypeRole).value<QnBusiness::EventType>()));
            connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
        }
        return;
    case QnBusiness::ActionColumn:
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
            comboBox->setCurrentIndex(comboBox->findData(index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>()));
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
        QnBusiness::ActionType actionType = index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>();

        switch (actionType) {
        case QnBusiness::ShowPopupAction:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                model->setData(index, comboBox->itemData(comboBox->currentIndex()));
            }
            return;
        }
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
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
        case QnBusiness::SayTextAction:
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
