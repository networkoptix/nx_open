#include "ptz_preset_hotkey_item_delegate.h"

#include <QtCore/QEvent>

#include <ui/widgets/char_combo_box.h>
#include <ui/models/ptz_manage_model.h>
#include <ui/dialogs/message_box.h>

// -------------------------------------------------------------------------- //
// QnComboBoxContainerEventFilter
// -------------------------------------------------------------------------- //
class QnComboBoxContainerEventFilter: public QObject {
public:
    QnComboBoxContainerEventFilter(QObject *parent = NULL): QObject(parent) {}

    virtual bool eventFilter(QObject *watched, QEvent *event) override {
        switch(event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            watched->removeEventFilter(this);
            return false;
        case QEvent::MouseButtonRelease:
            watched->removeEventFilter(this);
            return true; /* Eat the event. */
        default:
            return false;
        }
    }
};


// -------------------------------------------------------------------------- //
// QnPtzPresetHotkeyItemDelegate
// -------------------------------------------------------------------------- //
QnPtzPresetHotkeyItemDelegate::QnPtzPresetHotkeyItemDelegate(QObject *parent):
    base_type(parent),
    m_filter(new QnComboBoxContainerEventFilter(this))
{}

QnPtzPresetHotkeyItemDelegate::~QnPtzPresetHotkeyItemDelegate() {
    return;
}

QWidget *QnPtzPresetHotkeyItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
    QnCharComboBox *result = new QnCharComboBox(parent);

    result->addItem(tr("None"), -1);
    for(int i = 1; i <= 9; i++)
        result->addItem(QString::number(i), i);
    result->addItem(QString::number(0), 0);
    
    /* Open the popup after geometry adjustments. */
    QMetaObject::invokeMethod(result, "showPopup", Qt::QueuedConnection);

    /* Make sure popup widget is created. */
    result->view();

    /* This will prevent the popup from closing when the user releases mouse button. */
    foreach(QObject *child, result->children()) {
        if(child->inherits("QComboBoxPrivateContainer")) {
            child->installEventFilter(m_filter);
            break;
        }
    }

    return result;
}

void QnPtzPresetHotkeyItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    QnCharComboBox *comboBox = dynamic_cast<QnCharComboBox *>(editor);
    if(!comboBox)
        return;

    bool ok = false;
    int hotkey = index.data(Qt::EditRole).toInt(&ok);
    if(!ok || hotkey < 0 || hotkey > 9)
        hotkey = -1;

    comboBox->setCurrentIndex(comboBox->findData(hotkey));
}

void QnPtzPresetHotkeyItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    if(!model)
        return;

    QnCharComboBox *comboBox = qobject_cast<QnCharComboBox *>(editor);
    if(!comboBox)
        return;

    int hotkey = comboBox->itemData(comboBox->currentIndex()).toInt();

    if (!model->setData(index, hotkey, Qt::EditRole)) {
        QnPtzManageModel *ptzModel = qobject_cast<QnPtzManageModel*>(model);
        if (ptzModel) {
            QString existingId = ptzModel->hotkeys().value(hotkey);
            assert(!existingId.isEmpty()); // we can get here only if the selected hotkey is in use

            QnPtzManageModel::RowData existing = ptzModel->rowData(existingId);
            QString message = (existing.rowType == QnPtzManageModel::PresetRow)
                              ? tr("This hotkey is used by preset \"%1\"").arg(existing.presetModel.preset.name)
                              : tr("This hotkey is used by tour \"%1\"").arg(existing.tourModel.tour.name);

            QnMessageBox messageBox(QnMessageBox::Question, 0, tr("Change hotkey"), message, QnMessageBox::Cancel);
            messageBox.addButton(tr("Reassign"), QnMessageBox::AcceptRole);

            if (messageBox.exec() == QnMessageBox::Cancel)
                return;

            QModelIndex existingIndex = ptzModel->index(ptzModel->rowNumber(existing), QnPtzManageModel::HotkeyColumn);
            model->setData(existingIndex, QnPtzHotkey::NoHotkey, Qt::EditRole);
            model->setData(index, hotkey, Qt::EditRole);
        }
    }
}

