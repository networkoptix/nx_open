#include "ptz_tour_item_delegate.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>

#include <common/common_globals.h>

#include <core/ptz/ptz_preset.h>

#include <ui/models/ptz_tour_model.h>

QnPtzTourItemDelegate::QnPtzTourItemDelegate(QObject *parent) :
    base_type(parent)
{
}

QnPtzTourItemDelegate::~QnPtzTourItemDelegate() {
}

void QnPtzTourItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const  {
    base_type::initStyleOption(option, index);
    if (!index.data(Qn::ValidRole).toBool()) {
        QColor clr = index.data(Qt::BackgroundRole).value<QColor>();
        option->palette.setColor(QPalette::Highlight, clr.lighter());
    }
    //TODO: #GDM PTZ invalid rows highlight
}

QWidget* QnPtzTourItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    const QnPtzTourModel* model = dynamic_cast<const QnPtzTourModel*>(index.model());
    if (!model)
        return base_type::createEditor(parent, option, index);

    switch (index.column()) {
    case QnPtzTourModel::NameColumn:
    {
        QComboBox* comboBox = new QComboBox(parent);
        foreach (const QnPtzPreset &preset, model->presets()) {
            comboBox->addItem(preset.name, preset.id);
        }
        return comboBox;
    }
    case QnPtzTourModel::TimeColumn:
    {
        QComboBox* comboBox = new QComboBox(parent);
        foreach (quint64 time, QnPtzTourModel::stayTimeValues()) {
            comboBox->addItem(QnPtzTourModel::timeToString(time), time);
        }
        return comboBox;
    }
    case QnPtzTourModel::SpeedColumn:
    {
        QComboBox* comboBox = new QComboBox(parent);
        foreach (qreal speed, QnPtzTourModel::speedValues()) {
            comboBox->addItem(QnPtzTourModel::speedToString(speed), speed);
        }
        return comboBox;

    }
    default:
        break;
    }

    return base_type::createEditor(parent, option, index);
}


void QnPtzTourItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    switch (index.column()) {
    case QnPtzTourModel::NameColumn:
    case QnPtzTourModel::TimeColumn:
    case QnPtzTourModel::SpeedColumn:
    {
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
            comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
            connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
        }
        return;
    }
    default:
        break;
    }
    base_type::setEditorData(editor, index);
}

void QnPtzTourItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    switch (index.column()) {
    case QnPtzTourModel::NameColumn:
    case QnPtzTourModel::TimeColumn:
    case QnPtzTourModel::SpeedColumn:
    {
        if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor)) {
            model->setData(index, comboBox->itemData(comboBox->currentIndex()));
        }
        return;
    }
    default:
        break;
    }
    base_type::setModelData(editor, model, index);
}

bool QnPtzTourItemDelegate::eventFilter(QObject *object, QEvent *event) {
    QComboBox *editor = qobject_cast<QComboBox*>(object);
    if (editor && event->type() == QEvent::FocusOut)
        return false;
    return base_type::eventFilter(object, event);
}

void QnPtzTourItemDelegate::at_editor_commit() {
    if (QWidget* w = dynamic_cast<QWidget*> (sender()))
        emit commitData(w);
}
