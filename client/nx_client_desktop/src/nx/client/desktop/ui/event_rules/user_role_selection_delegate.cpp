#include "user_role_selection_delegate.h"

#include <client/client_globals.h>
#include <nx/utils/uuid.h>
#include <ui/models/user_roles_model.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

UserRoleSelectionDelegate::UserRoleSelectionDelegate(GetRoleStatus getStatus, QObject* parent):
    base_type(parent),
    m_getStatus(getStatus)
{
}

UserRoleSelectionDelegate::~UserRoleSelectionDelegate()
{
}

void UserRoleSelectionDelegate::paint(QPainter* painter,
    const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    base_type::paint(painter, option, index);
}

void UserRoleSelectionDelegate::initStyleOption(QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    const bool checked = index.sibling(index.row(), QnUserRolesModel::CheckColumn)
        .data(Qt::CheckStateRole).toInt() == Qt::Checked;

    const auto status = (m_getStatus
        ? m_getStatus(index.data(Qn::UuidRole).value<QnUuid>())
        : RoleStatus::valid);

    if (checked)
    {
        option->palette.setBrush(QPalette::Text,
            status != RoleStatus::invalid || option->state.testFlag(QStyle::State_Selected)
                ? option->palette.highlightedText()
                : qnGlobals->errorTextColor());
    }
    else
    {
        option->palette.setBrush(QPalette::HighlightedText,
            option->palette.text());
    }

    if (index.column() == QnUserRolesModel::NameColumn)
    {
        static const QnIcon::SuffixesList uncheckedSuffixes {
            { QnIcon::Normal,   lit("") },
            { QnIcon::Selected, lit("") },
            { QnIcon::Disabled, lit("disabled") }};

        static const QnIcon::SuffixesList checkedValidSuffixes {
            { QnIcon::Normal,   lit("selected") },
            { QnIcon::Selected, lit("selected") },
            { QnIcon::Disabled, lit("disabled") }};

        static const QnIcon::SuffixesList checkedInvalidSuffixes {
            { QnIcon::Normal,   lit("error") },
            { QnIcon::Selected, lit("selected") },
            { QnIcon::Disabled, lit("disabled") }};

        if (checked)
        {
            switch (status)
            {
                case RoleStatus::valid:
                    option->icon = qnSkin->icon(lit("tree/users.png"), QString(), &checkedValidSuffixes);
                    break;

                case RoleStatus::invalid:
                    option->icon = qnSkin->icon(lit("tree/users_alert.png"), QString(), &checkedInvalidSuffixes);
                    break;

                case RoleStatus::intermediate:
                    option->icon = qnSkin->icon(lit("tree/users_alert.png"), QString(), &checkedValidSuffixes);
                    break;
            }
        }
        else
        {
            option->icon = status == RoleStatus::valid
                ? qnSkin->icon(lit("tree/users.png"), QString(), &uncheckedSuffixes)
                : qnSkin->icon(lit("tree/users_alert.png"), QString(), &uncheckedSuffixes);
        }

        option->decorationSize = QnSkin::maximumSize(option->icon);
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
