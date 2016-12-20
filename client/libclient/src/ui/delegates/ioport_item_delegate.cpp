#include "ioport_item_delegate.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QComboBox>
#include <core/resource/resource.h>
#include <ui/style/globals.h>
#include "ui/models/ioports_view_model.h"

namespace {

static const int kColumnSizeExtendPx = 20;

}


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
        option->palette.setColor(QPalette::Highlight, qnGlobals->businessRuleDisabledHighlightColor());
    }
}

void QnIOPortItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.column() == QnIOPortsViewModel::NameColumn)
        base_type::updateEditorGeometry(editor, option, index);
    else
        editor->setGeometry(option.rect);
}

QWidget* QnIOPortItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem& option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(parent)

    QnIOPortData ioPort = index.data(Qn::IOPortDataRole).value<QnIOPortData>();
    switch (index.column())
    {
        case QnIOPortsViewModel::TypeColumn:
        {
            QComboBox* comboBox = new QComboBox(parent);
            if (ioPort.supportedPortTypes.testFlag(Qn::PT_Disabled))
                comboBox->addItem(QnIOPortsViewModel::portTypeToString(Qn::PT_Disabled), Qn::PT_Disabled);
            if (ioPort.supportedPortTypes.testFlag(Qn::PT_Input))
                comboBox->addItem(QnIOPortsViewModel::portTypeToString(Qn::PT_Input), Qn::PT_Input);
            if (ioPort.supportedPortTypes.testFlag(Qn::PT_Output))
                comboBox->addItem(QnIOPortsViewModel::portTypeToString(Qn::PT_Output), Qn::PT_Output);
            return comboBox;
        }
        case QnIOPortsViewModel::DefaultStateColumn:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->addItem(QnIOPortsViewModel::stateToString(Qn::IO_OpenCircuit), Qn::IO_OpenCircuit);
            comboBox->addItem(QnIOPortsViewModel::stateToString(Qn::IO_GroundedCircuit), Qn::IO_GroundedCircuit);
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
    switch (index.column())
    {
        case QnIOPortsViewModel::TypeColumn:
        case QnIOPortsViewModel::DefaultStateColumn:
        {
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
                model->setData(index, comboBox->itemData(comboBox->currentIndex()));
            return;
        }
        case QnIOPortsViewModel::NameColumn:
            if (QLineEdit* lineEdit = dynamic_cast<QLineEdit *>(editor))
                model->setData(index, lineEdit->text());
            return;
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

QSize QnIOPortItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize result = base_type::sizeHint(option, index);
    result.setWidth(result.width() + kColumnSizeExtendPx);

    if (index.isValid() && index.model() && index.model()->flags(index).testFlag(Qt::ItemIsEditable))
    {
        QScopedPointer<QWidget> editor(createEditor(nullptr, option, index));
        QSize editorHint = editor->sizeHint();
        result.setWidth(std::max(result.width(), editorHint.width()));
    }

    return result;
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
