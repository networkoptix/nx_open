#include "business_rule_item_delegate.h"

#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QComboBox>

#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_criterion.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/models/business_rules_view_model.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectResourcesDialogButton ------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget *parent):
    base_type(parent),
    m_dialogDelegate(NULL),
    m_nodeType(Qn::ServersNode)
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

Qn::NodeType QnSelectResourcesDialogButton::nodeType() const {
    return m_nodeType;
}

void QnSelectResourcesDialogButton::setNodeType(Qn::NodeType nodeType) {
    m_nodeType = nodeType;
}

void QnSelectResourcesDialogButton::at_clicked() {
    QnResourceSelectionDialog dialog(this, m_nodeType); //TODO: #GDM servers dialog?
    dialog.setSelectedResources(m_resources);
    dialog.setDelegate(m_dialogDelegate);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
        return;
    m_resources = dialog.getSelectedResources();
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
    if (index.data(QnBusiness::DisabledRole).toBool()) {
        if (QStyleOptionViewItemV4 *vopt = qstyleoption_cast<QStyleOptionViewItemV4 *>(option)) {
            vopt->state &= ~QStyle::State_Enabled;
         //   vopt->features |= QStyleOptionViewItemV2::Alternate;
        }
        option->palette.setColor(QPalette::Highlight, QColor(64, 64, 64)); //TODO: #GDM skin colors
    } else if (!index.data(QnBusiness::ValidRole).toBool()) {
        QColor clr = index.data(Qt::BackgroundRole).value<QColor>();
        option->palette.setColor(QPalette::Highlight, clr.lighter()); //TODO: #GDM skin colors
    }
}

QWidget* QnBusinessRuleItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    switch (index.column()) {
        case QnBusiness::SourceColumn:
            {
                QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
                //TODO: #GDM server selection dialog?
                connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

                BusinessEventType::Value eventType = (BusinessEventType::Value)index.data(QnBusiness::EventTypeRole).toInt();
                if (eventType == BusinessEventType::Camera_Motion)
                    btn->setDialogDelegate(new QnMotionEnabledDelegate(btn));
                else if (eventType == BusinessEventType::Camera_Input)
                    btn->setDialogDelegate(new QnInputEnabledDelegate(btn));

                return btn;
            }
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();

                if (actionType == BusinessActionType::ShowPopup) {
                    QComboBox* comboBox = new QComboBox(parent);
                    comboBox->addItem(tr("For All Users"), QnBusinessActionParameters::EveryOne);
                    comboBox->addItem(tr("For Administrators Only"), QnBusinessActionParameters::AdminOnly);
                    connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                    return comboBox;
                } else if (actionType == BusinessActionType::PlaySound) {
                    QComboBox* comboBox = new QComboBox(parent);
                    comboBox->setModel(context()->instance<QnAppServerNotificationCache>()->persistentGuiModel());
                    connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                    return comboBox;
                }

                QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
                connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

                if (actionType == BusinessActionType::CameraRecording) {
                    btn->setDialogDelegate(new QnRecordingEnabledDelegate(btn));
                }
                else if (actionType == BusinessActionType::CameraOutput || actionType == BusinessActionType::CameraOutputInstant) {
                    btn->setDialogDelegate(new QnOutputEnabledDelegate(btn));
                }
                else if (actionType == BusinessActionType::SendMail) {
                    btn->setDialogDelegate(new QnEmailValidDelegate(btn));
                    btn->setNodeType(Qn::UsersNode);
                }
                return btn;
            }
        case QnBusiness::EventColumn:
            {
                QComboBox* comboBox = new QComboBox(parent);
                for (int i = 0; i < BusinessEventType::Count; i++) {
                    BusinessEventType::Value val = (BusinessEventType::Value)i;
                    comboBox->addItem(BusinessEventType::toString(val), val);
                }
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                return comboBox;
            }
        case QnBusiness::ActionColumn:
            {
                bool instant = index.data(QnBusiness::InstantActionRole).toBool();
                QComboBox* comboBox = new QComboBox(parent);
                for (int i = 0; i < BusinessActionType::Count; i++) {
                    BusinessActionType::Value val = (BusinessActionType::Value)i;
                    if (instant && BusinessActionType::hasToggleState(val))
                        continue;
                    comboBox->addItem(BusinessActionType::toString(val), val);
                }
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                return comboBox;
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
                    btn->setResources(index.data(QnBusiness::EventResourcesRole).value<QnResourceList>());
                    btn->setText(index.data(QnBusiness::ShortTextRole).toString());
                    return;
                }
                break;
            }
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();

                if (actionType == BusinessActionType::ShowPopup) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                    }
                    return;
                }

                if (actionType == BusinessActionType::PlaySound) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
                        comboBox->setCurrentIndex(soundModel->rowByFilename(index.data(Qt::EditRole).toString()));
                    }
                    return;
                }


                if(QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
                    btn->setResources(index.data(QnBusiness::ActionResourcesRole).value<QnResourceList>());
                    btn->setText(index.data(QnBusiness::ShortTextRole).toString());
                    return;
                }
                break;
            }
        case QnBusiness::EventColumn:
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                comboBox->setCurrentIndex(comboBox->findData(index.data(QnBusiness::EventTypeRole)));
            }
            return;
        case QnBusiness::ActionColumn:
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                comboBox->setCurrentIndex(comboBox->findData(index.data(QnBusiness::ActionTypeRole)));
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
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();

                if (actionType == BusinessActionType::ShowPopup) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        model->setData(index, comboBox->itemData(comboBox->currentIndex()));
                    }
                    return;
                } else if (actionType == BusinessActionType::PlaySound) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
                        if (!soundModel->loaded())
                            return;
                        QString filename = soundModel->filenameByRow(comboBox->currentIndex());
                        model->setData(index, filename);
                    }
                    return;
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
        default:
            break;
    }
    base_type::setModelData(editor, model, index);
}

void QnBusinessRuleItemDelegate::at_editor_commit() {
    if (QWidget* w = dynamic_cast<QWidget*> (sender()))
        emit commitData(w);
}
