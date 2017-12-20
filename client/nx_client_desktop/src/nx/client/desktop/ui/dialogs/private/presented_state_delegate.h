#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class PresentedStateDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    PresentedStateDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    void setEditorData(QWidget *editor,
        const QModelIndex &index) const override;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
