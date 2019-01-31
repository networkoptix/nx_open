#include "ptz_preset_hotkey_item_delegate.h"

#include <QtCore/QEvent>

#include <ui/models/ptz_manage_model.h>

#include <nx/vms/client/desktop/common/widgets/char_combo_box.h>

using namespace nx::vms::client::desktop;

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
QnPtzPresetHotkeyItemDelegate::QnPtzPresetHotkeyItemDelegate(QWidget* parent):
    base_type(parent),
    m_filter(new QnComboBoxContainerEventFilter(this))
{}

QnPtzPresetHotkeyItemDelegate::~QnPtzPresetHotkeyItemDelegate() {
    return;
}

QWidget *QnPtzPresetHotkeyItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
    CharComboBox *result = new CharComboBox(parent);

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
    CharComboBox *comboBox = dynamic_cast<CharComboBox *>(editor);
    if(!comboBox)
        return;

    bool ok = false;
    int hotkey = index.data(Qt::EditRole).toInt(&ok);
    if(!ok || hotkey < 0 || hotkey > 9)
        hotkey = -1;

    comboBox->setCurrentIndex(comboBox->findData(hotkey));
}

void QnPtzPresetHotkeyItemDelegate::setModelData(
    QWidget* editor,
    QAbstractItemModel* model,
    const QModelIndex& index) const
{
    if(!model)
        return;

    CharComboBox* const comboBox = qobject_cast<CharComboBox*>(editor);
    if(!comboBox)
        return;

    const int hotkey = comboBox->itemData(comboBox->currentIndex()).toInt();
    if (model->setData(index, hotkey, Qt::EditRole))
        return;

    const QnPtzManageModel * const ptzModel = qobject_cast<QnPtzManageModel*>(model);
    if (!ptzModel)
        return;

    const QString existingId = ptzModel->hotkeys().value(hotkey);
    NX_ASSERT(!existingId.isEmpty(), Q_FUNC_INFO,
        "we should get here only if the selected hotkey is in use");
    if (existingId.isEmpty())
        return;

    const QnPtzManageModel::RowData existing = ptzModel->rowData(existingId);
    const QString message = (existing.rowType == QnPtzManageModel::PresetRow
        ? tr("Hotkey used by preset \"%1\"").arg(existing.presetModel.preset.name)
        : tr("Hotkey used by tour \"%1\"").arg(existing.tourModel.tour.name));

    QnMessageBox messageBox(QnMessageBoxIcon::Warning, message,
        QString(), QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        qobject_cast<QWidget*>(parent()));
    messageBox.addButton(tr("Reassign"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    if (messageBox.exec() == QDialogButtonBox::Cancel)
        return;

    const auto rowNumber = ptzModel->rowNumber(existing);
    const auto existingIndex = ptzModel->index(rowNumber, QnPtzManageModel::HotkeyColumn);
    model->setData(existingIndex, QnPtzHotkey::kNoHotkey, Qt::EditRole);
    model->setData(index, hotkey, Qt::EditRole);
}

