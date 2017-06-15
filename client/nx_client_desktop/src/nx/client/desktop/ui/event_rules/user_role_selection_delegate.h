#pragma once

#include <functional>

#include <QtWidgets/QStyledItemDelegate>

class QnUuid;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class UserRoleSelectionDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    enum class RoleStatus
    {
        invalid,
        valid,
        intermediate
    };

    using GetRoleStatus = std::function<RoleStatus(const QnUuid&)>;

public:
    explicit UserRoleSelectionDelegate(
        GetRoleStatus getStatus = GetRoleStatus(),
        QObject* parent = nullptr);

    virtual ~UserRoleSelectionDelegate() override;

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    const GetRoleStatus m_getStatus;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
