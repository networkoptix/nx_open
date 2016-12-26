#include "ioport_item_delegate.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>

#include <client/client_globals.h>

#include <text/time_strings.h>

#include <ui/style/helper.h>
#include <ui/models/ioports_view_model.h>
#include <ui/workaround/widgets_signals_workaround.h>


QWidget* QnIoPortItemDelegate::createWidget(
    QAbstractItemModel* model, const QModelIndex& index, QWidget* parent) const
{
    switch (index.column())
    {
        case QnIOPortsViewModel::NumberColumn:
        case QnIOPortsViewModel::IdColumn:
            return base_type::createWidget(model, index, parent);

        case QnIOPortsViewModel::TypeColumn:
        case QnIOPortsViewModel::DefaultStateColumn:
        case QnIOPortsViewModel::ActionColumn:
        {
            if (index.flags().testFlag(Qt::ItemIsEditable))
            {
                auto comboBox = new QComboBox(parent);
                comboBox->setEditable(false);

                switch (index.column())
                {
                    case QnIOPortsViewModel::TypeColumn:
                    {
                        QnIOPortData data = index.data(Qn::IOPortDataRole).value<QnIOPortData>();
                        if (data.supportedPortTypes.testFlag(Qn::PT_Disabled))
                        {
                            comboBox->addItem(QnIOPortsViewModel::portTypeToString(
                                Qn::PT_Disabled), Qn::PT_Disabled);
                        }
                        if (data.supportedPortTypes.testFlag(Qn::PT_Input))
                        {
                            comboBox->addItem(QnIOPortsViewModel::portTypeToString(
                                Qn::PT_Input), Qn::PT_Input);
                        }
                        if (data.supportedPortTypes.testFlag(Qn::PT_Output))
                        {
                            comboBox->addItem(QnIOPortsViewModel::portTypeToString(
                                Qn::PT_Output), Qn::PT_Output);
                        }
                        break;
                    }

                    case QnIOPortsViewModel::DefaultStateColumn:
                    {
                        comboBox->addItem(QnIOPortsViewModel::stateToString(
                            Qn::IO_OpenCircuit), Qn::IO_OpenCircuit);
                        comboBox->addItem(QnIOPortsViewModel::stateToString(
                            Qn::IO_GroundedCircuit), Qn::IO_GroundedCircuit);
                        break;
                    }

                    case QnIOPortsViewModel::ActionColumn:
                    {
                        comboBox->addItem(QnIOPortsViewModel::actionToString(
                            QnIOPortsViewModel::ToggleState), QnIOPortsViewModel::ToggleState);
                        comboBox->addItem(QnIOPortsViewModel::actionToString(
                            QnIOPortsViewModel::Impulse), QnIOPortsViewModel::Impulse);
                        break;
                    }

                    default:
                    {
                        NX_ASSERT(false); //< should never ever get here
                        break;
                    }
                }

                auto commit =
                    [model, comboBox](int i)
                    {
                        const auto index = indexForWidget(comboBox);

                        if (index.isValid())
                            model->setData(index, comboBox->itemData(i), Qt::EditRole);
                    };

                connect(comboBox, QnComboboxCurrentIndexChanged, comboBox, commit);
                return comboBox;
            }
            else
            {
                if (index.column() == QnIOPortsViewModel::ActionColumn)
                    return nullptr;

                auto lineEdit = new QLineEdit(parent);
                lineEdit->setReadOnly(true);
                return lineEdit;
            }
        }

        case QnIOPortsViewModel::NameColumn:
        {
            auto lineEdit = new QLineEdit(parent);
            auto commit =
                [model, lineEdit](const QString& text)
                {
                    const auto index = indexForWidget(lineEdit);

                    if (index.isValid())
                        model->setData(index, text, Qt::EditRole);
                };

            connect(lineEdit, &QLineEdit::textChanged, lineEdit, commit);
            return lineEdit;
        }

        case QnIOPortsViewModel::DurationColumn:
        {
            if (!index.flags().testFlag(Qt::ItemIsEditable))
                return nullptr;

            auto spinBox = new QSpinBox(parent);
            spinBox->setMinimum(1);
            spinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));

            auto commit =
                [model, spinBox](int value)
                {
                    const auto index = indexForWidget(spinBox);

                    if (index.isValid())
                        model->setData(index, value, Qt::EditRole);
                };

            connect(spinBox, QnSpinboxIntValueChanged, spinBox, commit);
            return spinBox;
        }
    }

    return nullptr;
}

bool QnIoPortItemDelegate::updateWidget(QWidget* widget, const QModelIndex& index) const
{
    if (base_type::updateWidget(widget, index))
        return true;

    switch (index.column())
    {
        case QnIOPortsViewModel::NumberColumn:
        case QnIOPortsViewModel::IdColumn:
            return false; //< should have been handled by base_type

        case QnIOPortsViewModel::TypeColumn:
        case QnIOPortsViewModel::DefaultStateColumn:
        case QnIOPortsViewModel::ActionColumn:
        {
            if (index.flags().testFlag(Qt::ItemIsEditable))
            {
                auto comboBox = qobject_cast<QComboBox*>(widget);
                if (!comboBox)
                    return false;

                comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                return true;
            }
            else
            {
                if (index.column() == QnIOPortsViewModel::ActionColumn)
                    return !widget; //< should delete if widget exists

                auto lineEdit = qobject_cast<QLineEdit*>(widget);
                if (!lineEdit)
                    return false;

                lineEdit->setReadOnly(true);
                lineEdit->setText(index.data(Qt::DisplayRole).toString());
                return true;
            }
        }

        case QnIOPortsViewModel::NameColumn:
        {
            auto lineEdit = qobject_cast<QLineEdit*>(widget);
            if (!lineEdit)
                return false;

            const auto newText = index.data(Qt::EditRole).toString();
            if (lineEdit->text() != newText)
                lineEdit->setText(newText);

            lineEdit->setReadOnly(!index.flags().testFlag(Qt::ItemIsEditable));
            return true;
        }

        case QnIOPortsViewModel::DurationColumn:
        {
            if (!index.flags().testFlag(Qt::ItemIsEditable))
                return !widget; //< should delete if widget exists

            auto spinBox = qobject_cast<QSpinBox*>(widget);
            if (!spinBox)
                return false;

            int newValue = index.data(Qt::EditRole).toInt();
            if (spinBox->value() != newValue)
                spinBox->setValue(newValue);

            return true;
        }
    }

    return true;
}

QSize QnIoPortItemDelegate::sizeHint(QWidget* widget, const QModelIndex& index) const
{
    auto size = base_type::sizeHint(widget, index);
    auto lineEdit = qobject_cast<QLineEdit*>(widget);

    if (!lineEdit || !lineEdit->isReadOnly())
        return size;

    //TODO: Currently we rely on this assumption of how our style lays out LineEdit contents.
    static const int kFrameWidth = 1;
    size.setWidth(lineEdit->fontMetrics().width(lineEdit->text())
        + (style::Metrics::kStandardPadding + kFrameWidth) * 2);

    return size;
}
