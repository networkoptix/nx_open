#include "ptz_preset_hotkey_item_delegate.h"

#include <ui/widgets/char_combo_box.h>


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
    for(int i = 0; i <= 9; i++)
        result->addItem(QString::number(i), i);
    
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

    QnCharComboBox *comboBox = dynamic_cast<QnCharComboBox *>(editor);
    if(!comboBox)
        return;

    int hotkey = comboBox->itemData(comboBox->currentIndex()).toInt();
    model->setData(index, hotkey, Qt::EditRole);
}

