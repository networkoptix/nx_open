#include "business_rule_item_delegate.h"

#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QComboBox>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_criterion.h>

#include <business/events/camera_input_business_event.h>
#include <business/events/motion_business_event.h>
#include <business/actions/camera_output_business_action.h>
#include <business/actions/recording_business_action.h>

#include <ui/models/business_rules_view_model.h>
#include <ui/style/globals.h>

#include <utils/common/email.h>

namespace {

    template<class T>
    class QnCheckAndWarnDelegate: public QnSelectCamerasDialogDelegate {
    public:
        QnCheckAndWarnDelegate(QWidget* parent):
            QnSelectCamerasDialogDelegate(parent),
            m_warningLabel(NULL)
        {}

        virtual void init(QWidget* parent) override {
            m_warningLabel = new QLabel(parent);
            QPalette palette = parent->palette();
            palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
            m_warningLabel->setPalette(palette);

            parent->layout()->addWidget(m_warningLabel);
        }

        virtual bool validate(const QnResourceList &selected) override {
            if (!m_warningLabel)
                return true;
            int invalid = 0;
            QnSharedResourcePointerList<T> resources = selected.filtered<T>();
            foreach (const QnSharedResourcePointer<T> &resource, resources) {
                if (!isResourceValid(resource)) {
                    invalid++;
                }
            }
            m_warningLabel->setText(getText(invalid, resources.size()));
            m_warningLabel->setVisible(invalid > 0);
            return true;
        }

    protected:
        virtual bool isResourceValid(const QnSharedResourcePointer<T> &resource) const = 0;
        virtual QString getText(int invalid, int total) const = 0;

    private:
        QLabel* m_warningLabel;
    };


    class QnMotionEnabledDelegate: public QnCheckAndWarnDelegate<QnVirtualCameraResource> {
    public:
        QnMotionEnabledDelegate(QWidget* parent): QnCheckAndWarnDelegate(parent){}
    protected:
        virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override {
            return QnMotionBusinessEvent::isResourceValid(camera);
        }
        virtual QString getText(int invalid, int total) const override {
            return tr("Recording or motion detection is disabled for %1 of %2 selected cameras.").arg(invalid).arg(total);
        }
    };

    class QnRecordingEnabledDelegate: public QnCheckAndWarnDelegate<QnVirtualCameraResource> {
    public:
        QnRecordingEnabledDelegate(QWidget* parent): QnCheckAndWarnDelegate(parent){}
    protected:
        virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override {
            return QnRecordingBusinessAction::isResourceValid(camera);
        }
        virtual QString getText(int invalid, int total) const override {
            return tr("Recording is disabled for %1 of %2 selected cameras.").arg(invalid).arg(total);
        }
    };

    class QnInputEnabledDelegate: public QnCheckAndWarnDelegate<QnVirtualCameraResource> {
    public:
        QnInputEnabledDelegate(QWidget* parent): QnCheckAndWarnDelegate(parent){}
    protected:
        virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override {
            return QnCameraInputEvent::isResourceValid(camera);
        }
        virtual QString getText(int invalid, int total) const override {
            return tr("%1 of %2 selected cameras have no input ports.").arg(invalid).arg(total);
        }
    };

    class QnOutputEnabledDelegate: public QnCheckAndWarnDelegate<QnVirtualCameraResource> {
    public:
        QnOutputEnabledDelegate(QWidget* parent): QnCheckAndWarnDelegate(parent){}
    protected:
        virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override {
            return QnCameraOutputBusinessAction::isResourceValid(camera);
        }
        virtual QString getText(int invalid, int total) const override {
            return tr("%1 of %2 selected cameras have not output relays.").arg(invalid).arg(total);
        }
    };

    class QnEmailValidDelegate: public QnCheckAndWarnDelegate<QnUserResource> {
    public:
        QnEmailValidDelegate(QWidget* parent): QnCheckAndWarnDelegate(parent){}
    protected:
        virtual bool isResourceValid(const QnUserResourcePtr &user) const override {
            return isEmailValid(user->getEmail());
        }
        virtual QString getText(int invalid, int total) const override {
            return tr("%1 of %2 selected users have invalid email.").arg(invalid).arg(total);
        }
    };

}

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

QnSelectCamerasDialogDelegate* QnSelectResourcesDialogButton::dialogDelegate() const {
    return m_dialogDelegate;
}

void QnSelectResourcesDialogButton::setDialogDelegate(QnSelectCamerasDialogDelegate* delegate) {
    m_dialogDelegate = delegate;
}

Qn::NodeType QnSelectResourcesDialogButton::nodeType() const {
    return m_nodeType;
}

void QnSelectResourcesDialogButton::setNodeType(Qn::NodeType nodeType) {
    m_nodeType = nodeType;
}

void QnSelectResourcesDialogButton::at_clicked() {
    QnSelectCamerasDialog dialog(this, m_nodeType); //TODO: #GDM servers dialog?
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
                if (eventType == BusinessEventType::BE_Camera_Motion)
                    btn->setDialogDelegate(new QnMotionEnabledDelegate(btn));
                else if (eventType == BusinessEventType::BE_Camera_Input)
                    btn->setDialogDelegate(new QnInputEnabledDelegate(btn));

                return btn;
            }
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();

                if (actionType == BusinessActionType::BA_ShowPopup) {
                    QComboBox* comboBox = new QComboBox(parent);
                    comboBox->addItem(tr("For All Users"), 0);
                    comboBox->addItem(tr("For Administrators Only"), 1);
                    connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                    return comboBox;
                }

                QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
                connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

                if (actionType == BusinessActionType::BA_CameraRecording) {
                    btn->setDialogDelegate(new QnRecordingEnabledDelegate(btn));
                }
                else if (actionType == BusinessActionType::BA_CameraOutput) {
                    btn->setDialogDelegate(new QnOutputEnabledDelegate(btn));
                }
                else if (actionType == BusinessActionType::BA_SendMail) {
                    btn->setDialogDelegate(new QnEmailValidDelegate(btn));
                    btn->setNodeType(Qn::UsersNode);
                }
                return btn;
            }
        case QnBusiness::EventColumn:
            {
                QComboBox* comboBox = new QComboBox(parent);
                for (int i = 0; i < BusinessEventType::BE_Count; i++) {
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
                for (int i = 0; i < BusinessActionType::BA_Count; i++) {
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

                if (actionType == BusinessActionType::BA_ShowPopup) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
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

                if (actionType == BusinessActionType::BA_ShowPopup) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        model->setData(index, comboBox->itemData(comboBox->currentIndex()));
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
