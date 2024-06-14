// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS
#include <QtWidgets/QLabel>

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>

namespace QnBusiness {

bool actionAllowedForUser(
    const nx::vms::event::AbstractActionPtr& action,
    const QnUserResourcePtr& user);

} // namespace QnBusiness

template<typename ValidationPolicy>
static bool isResourcesListValid(
    nx::vms::client::desktop::SystemContext* context,
    const ValidationPolicy& policy,
    const QnResourceList& resources)
{
    typedef typename ValidationPolicy::resource_type ResourceType;

    auto filtered = resources.filtered<ResourceType>();

    if (filtered.isEmpty())
        return ValidationPolicy::emptyListIsValid();

    if (filtered.size() > 1 && !ValidationPolicy::multiChoiceListIsValid())
        return false;

    for (const auto& resource: filtered)
    {
        if (!policy.isResourceValid(context, resource))
            return false;
    }

    return true;
}

template<typename ValidationPolicy>
static bool isResourcesListValid(
    nx::vms::client::desktop::SystemContext* context, const QnResourceList& resources)
{
    ValidationPolicy policy;
    return isResourcesListValid(context, policy, resources);
}

class QnSendEmailActionDelegate: public QnResourceSelectionDialogDelegate
{
    Q_DECLARE_TR_FUNCTIONS(QnSendEmailActionDelegate)
public:
    QnSendEmailActionDelegate(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent);
    ~QnSendEmailActionDelegate() {}

    void init(QWidget* parent) override;

    QString validationMessage(const QSet<nx::Uuid>& selected) const override;
    bool isValid(const nx::Uuid& resourceId) const override;

    static bool isValidList(const QSet<nx::Uuid>& ids, const QString& additional);

    static QString getText(const QSet<nx::Uuid>& ids, const bool detailed,
        const QString& additional);
private:
    static QStringList parseAdditional(const QString& additional);
    static bool isValidUser(const QnUserResourcePtr& user);
private:
    QLabel* m_warningLabel;
};
