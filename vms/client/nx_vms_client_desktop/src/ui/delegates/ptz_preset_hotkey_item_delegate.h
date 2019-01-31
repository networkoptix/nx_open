#ifndef QN_PTZ_PRESET_HOTKEY_ITEM_DELEGATE_H
#define QN_PTZ_PRESET_HOTKEY_ITEM_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

class QnComboBoxContainerEventFilter;

class QnPtzPresetHotkeyItemDelegate: public QStyledItemDelegate {
    Q_OBJECT
    typedef QStyledItemDelegate base_type;

public:
    QnPtzPresetHotkeyItemDelegate(QWidget* parent);
    virtual ~QnPtzPresetHotkeyItemDelegate();

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    virtual void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index) const override;

private:
    QnComboBoxContainerEventFilter *m_filter;
};


#endif // QN_PTZ_PRESET_HOTKEY_ITEM_DELEGATE_H
