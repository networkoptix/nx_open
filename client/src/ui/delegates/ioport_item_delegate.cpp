#include "ioport_item_delegate.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QComboBox>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <business/business_resource_validation.h>

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
#include "ui/models/ioports_view_model.h"


///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnIOPortItemDelegate ---------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////
QnIOPortItemDelegate::QnIOPortItemDelegate(QObject *parent):
    base_type(parent)
    //QnWorkbenchContextAware(parent)
{
}

QnIOPortItemDelegate::~QnIOPortItemDelegate() {

}

void QnIOPortItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const  
{
    base_type::initStyleOption(option, index);
    if (index.data(Qn::DisabledRole).toBool()) {
        if (QStyleOptionViewItemV4 *vopt = qstyleoption_cast<QStyleOptionViewItemV4 *>(option)) {
            vopt->state &= ~QStyle::State_Enabled;
        }
        //option->palette.setColor(QPalette::Highlight, qnGlobals->businessRuleDisabledHighlightColor());
    }
}

QWidget* QnIOPortItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  
{
    QnIOPortData ioPort = index.data(Qn::IOPortDataRole).value<QnIOPortData>();

    switch (index.column()) 
    {
        case QnIOPortsViewModel::TypeColumn:
        {
            QComboBox* comboBox = new QComboBox(parent);
            if (ioPort.supportedPortTypes & Qn::PT_Disabled)
                comboBox->addItem(tr("Disabled"), Qn::PT_Disabled);
            if (ioPort.supportedPortTypes & Qn::PT_Input)
                comboBox->addItem(tr("Input"), Qn::PT_Input);
            if (ioPort.supportedPortTypes & Qn::PT_Output)
                comboBox->addItem(tr("Output"), Qn::PT_Output);
            return comboBox;
        }
        case QnIOPortsViewModel::DefaultStateColumn:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->addItem(tr("Open circuit"), Qn::IO_OpenCircuit);
            comboBox->addItem(tr("Grounded circuit"), Qn::IO_GroundedCircuit);
            return comboBox;
        }
        case QnIOPortsViewModel::NameColumn:
            {
                QLineEdit* lineEdit = new QLineEdit(parent);
                return lineEdit;
            }
        case QnIOPortsViewModel::AutoResetColumn:
        {
            QSpinBox* spinBox = new QSpinBox(parent);
            spinBox->setMinimum(0);
            spinBox->setMaximum(999999);
            return spinBox;
        }
    default:
        break;
    }

    return base_type::createEditor(parent, option, index);
}


void QnIOPortItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    switch (index.column()) {
        case QnIOPortsViewModel::TypeColumn:
        case QnIOPortsViewModel::DefaultStateColumn:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
                comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        case QnIOPortsViewModel::NameColumn:
        {
            if (QLineEdit* lineEdit = dynamic_cast<QLineEdit *>(editor)) {
                lineEdit->setText(index.data(Qt::EditRole).toString());
                connect(lineEdit, SIGNAL(textChanged(QString)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        case QnIOPortsViewModel::AutoResetColumn:
        {
            if (QSpinBox* spinBox = dynamic_cast<QSpinBox *>(editor)) {
                spinBox->setValue(index.data(Qt::EditRole).toInt());
                connect(spinBox, SIGNAL(valueChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        }
        default:
            break;
    }

    base_type::setEditorData(editor, index);
}

void QnIOPortItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const 
{
    switch (index.column()) {
    case QnIOPortsViewModel::TypeColumn:
    case QnIOPortsViewModel::DefaultStateColumn:
    {
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
            model->setData(index, comboBox->itemData(comboBox->currentIndex()));
        return;
    }
    case QnIOPortsViewModel::AutoResetColumn:
        {
            if (QSpinBox* spinBox = dynamic_cast<QSpinBox *>(editor))
                model->setData(index, spinBox->value());
            return;
        }
    default:
        break;
    }
    base_type::setModelData(editor, model, index);
}

bool QnIOPortItemDelegate::eventFilter(QObject *object, QEvent *event) {
    QComboBox *editor = qobject_cast<QComboBox*>(object);
    if (editor && event->type() == QEvent::FocusOut)
        return false;
    return base_type::eventFilter(object, event);
}

void QnIOPortItemDelegate::at_editor_commit() {
    if (QWidget* w = dynamic_cast<QWidget*> (sender()))
        emit commitData(w);
}
