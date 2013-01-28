#include "business_rule_item_delegate.h"

#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QComboBox>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <ui/models/business_rules_view_model.h>
#include <ui/style/globals.h>

namespace {

    class QnRecordingEnabledDelegate: public QnSelectCamerasDialogDelegate {

    public:
        QnRecordingEnabledDelegate(QWidget* parent):
            QnSelectCamerasDialogDelegate(parent),
            m_recordingLabel(new QLabel(parent))
        {
            QPalette palette = parent->palette();
            palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
            m_recordingLabel->setPalette(palette);
        }

        virtual void setWidgetLayout(QLayout *layout) override {
            layout->addWidget(m_recordingLabel);
        }

        virtual void modelDataChanged(const QnResourceList &selected) override {
            int disabled = 0;
            QnVirtualCameraResourceList cameras = selected.filtered<QnVirtualCameraResource>();
            foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
                if (camera->isScheduleDisabled()) {
                    disabled++;
                }
            }
            m_recordingLabel->setText(tr("Recording is disabled for %1 of %2 selected cameras")
                                      .arg(disabled)
                                      .arg(cameras.size()));
            m_recordingLabel->setVisible(disabled > 0);
        }
    private:
        QLabel* m_recordingLabel;

    };

}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectResourcesDialogButton ------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget *parent):
    base_type(parent),
    m_dialogDelegate(NULL)
{
    connect(this, SIGNAL(clicked()), this, SLOT(at_clicked()));
}

QnResourceList QnSelectResourcesDialogButton::resources() {
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(QnResourceList resources) {
    m_resources = resources;
}

QnSelectCamerasDialogDelegate* QnSelectResourcesDialogButton::dialogDelegate() {
    return m_dialogDelegate;
}

void QnSelectResourcesDialogButton::setDialogDelegate(QnSelectCamerasDialogDelegate* delegate) {
    m_dialogDelegate = delegate;
}

void QnSelectResourcesDialogButton::at_clicked() {
    QnSelectCamerasDialog dialog(this); //TODO: #GDM servers dialog?
    dialog.setSelectedResources(m_resources);
    dialog.setDelegate(m_dialogDelegate);
    if (dialog.exec() != QDialog::Accepted)
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
        //option->palette.setColor(QPalette::Highlight, QColor(127, 0, 0, 127)); //TODO: #GDM skin colors
    } /*else
        option->palette.setColor(QPalette::Highlight, QColor(127, 127, 127, 255));*/
}

QWidget* QnBusinessRuleItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    switch (index.column()) {
        case QnBusiness::SourceColumn:
            {
                QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
                btn->setText(tr("Select cameras...")); //TODO: target type
                connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));
                return btn;
            }
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();
                if (actionType == BusinessActionType::BA_SendMail)
                    break;

                if (actionType == BusinessActionType::BA_ShowPopup) {
                    QComboBox* comboBox = new QComboBox(parent);
                    comboBox->addItem(tr("For All Users"), 0);
                    comboBox->addItem(tr("For Administrators Only"), 1);
                    connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                    return comboBox;
                }

                QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
                btn->setText(tr("Select cameras...")); //TODO: target type
                connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

                if (actionType == BusinessActionType::BA_CameraRecording)
                    btn->setDialogDelegate(new QnRecordingEnabledDelegate(parent));
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
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();
                if (actionType == BusinessActionType::BA_SendMail)
                    break;

                if (actionType == BusinessActionType::BA_ShowPopup) {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                        comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                    }
                    return;
                }

                if(QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
                    int role = index.column() == QnBusiness::SourceColumn
                            ? QnBusiness::EventResourcesRole
                            : QnBusiness::ActionResourcesRole;

                    btn->setResources(index.data(role).value<QnResourceList>());
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
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = (BusinessActionType::Value)index.data(QnBusiness::ActionTypeRole).toInt();
                if (actionType == BusinessActionType::BA_SendMail)
                    break;

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
