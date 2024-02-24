// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/license/license_usage_fwd.h>

#include "resource_item_delegate.h"

class QnLicenseListItemDelegate:
    public QnResourceItemDelegate
{
    using base_type = QnResourceItemDelegate;

public:
    explicit QnLicenseListItemDelegate(
        nx::vms::client::desktop::SystemContext* systemContext,
        bool invalidLicensesDimmed,
        QObject* parent = nullptr);
    virtual ~QnLicenseListItemDelegate();

    bool invalidLicensesDimmed() const;
    void setInvalidLicensesDimmed(bool value);

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

private:
    nx::vms::license::Validator* m_validator;
    bool m_invalidLicensesDimmed;
};
