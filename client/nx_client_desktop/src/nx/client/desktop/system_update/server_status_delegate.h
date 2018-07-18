#pragma once

#include <vector>
#include <QtWidgets/QStyledItemDelegate>
#include <nx/api/updates2/updates2_status_data.h>

namespace nx {
namespace client {
namespace desktop {

struct UpdateItem;

// Fancy delegate to show status of updating server.
// It creates a complex widget to display state of each server.
class ServerStatusItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    using base_type = QStyledItemDelegate;

public:
    explicit ServerStatusItemDelegate(QWidget *parent = 0);
    ~ServerStatusItemDelegate();

    QPixmap getCurrentAnimationFrame() const;
signals:
    // Event is emitted when we click 'retry' or 'cancel'
    void updateItemCommand(std::shared_ptr<UpdateItem> item) const;

protected:
    class ServerStatusWidget;

    virtual QWidget* createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
    void setEditorData(QWidget* editor, const QModelIndex &index) const override;

private:
    QScopedPointer<QMovie> m_updateAnimation;
};

} // namespace desktop
} // namespace client
} // namespace nx
