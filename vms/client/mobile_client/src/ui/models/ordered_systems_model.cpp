// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ordered_systems_model.h"

#include <QtCore/QRegularExpression>

#include <ui/models/systems_model.h>
#include <ui/models/systems_controller.h>
#include <nx/utils/string.h>
#include <nx/utils/algorithm/comparator.h>

QnOrderedSystemsModel::QnOrderedSystemsModel(QObject* parent):
    base_type(parent)
{
    setSourceModel(new QnSystemsModel(new SystemsController(), this));

    setTriggeringRoles({
        QnSystemsModel::SearchRoleId,
        QnSystemsModel::SystemNameRoleId,
        QnSystemsModel::SystemIdRoleId,
        QnSystemsModel::LocalIdRoleId,
        QnSystemsModel::IsFactorySystemRoleId,
        QnSystemsModel::IsOnlineRoleId});
}

bool QnOrderedSystemsModel::lessThan(
    const QModelIndex& left,
    const QModelIndex& right) const
{
    /**
     * Sort order should be the folloing:
     * 1. Online localhost systems
     * 2. Online factory systems
     * 3. Online cloud systems
     * 4. Online local systems
     * 5. Offline localhost systems
     * 6. Offline cloud systems
     * 7. Offline local systems
     */
    static auto kComparator =
        []()
        {
            const auto caseInsensetiveStringValueAccessor =
                [](int roleId)
                {
                    return [roleId](const QModelIndex& index)
                        {
                            return index.data(roleId).value<QString>().toLower();
                        };
                };

            const auto boolSortValueAccessor =
                [](int roleId)
                {
                    return [roleId](const QModelIndex& index)
                        {
                            // Return inverted value as "false" < "true".
                            return !index.data(roleId).toBool();
                        };
                };

            const auto localhostSortValueAccessor =
                [](const QModelIndex& index)
                {
                    static const QRegularExpression kLocalhostRegExp(
                        "\\b(127\\.0\\.0\\.1|localhost)\\b",
                        QRegularExpression::CaseInsensitiveOption
                            | QRegularExpression::DontCaptureOption);

                    return !index.data(QnSystemsModel::SearchRoleId)
                        .toString().contains(kLocalhostRegExp);
                };

            return nx::utils::algorithm::Comparator(/*ascending*/ true,
                boolSortValueAccessor(QnSystemsModel::IsOnlineRoleId),
                localhostSortValueAccessor,
                boolSortValueAccessor(QnSystemsModel::IsFactorySystemRoleId),
                boolSortValueAccessor(QnSystemsModel::IsCloudSystemRoleId),
                caseInsensetiveStringValueAccessor(QnSystemsModel::SystemNameRoleId),
                caseInsensetiveStringValueAccessor(QnSystemsModel::SystemIdRoleId));
        }();

    return kComparator(left, right);
}

int QnOrderedSystemsModel::searchRoleId()
{
    return QnSystemsModel::SearchRoleId;
}
