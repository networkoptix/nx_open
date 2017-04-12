#pragma once

#include <client_core/connection_context_aware.h>

#include "resource_item_delegate.h"

class QnLicenseValidator;

class QnLicenseListItemDelegate : public QnResourceItemDelegate, public QnConnectionContextAware
{
    using base_type = QnResourceItemDelegate;

public:
    explicit QnLicenseListItemDelegate(QObject* parent = nullptr, bool invalidLicensesDimmed = true);
    virtual ~QnLicenseListItemDelegate();

    bool invalidLicensesDimmed() const;
    void setInvalidLicensesDimmed(bool value);

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

private:
    QnLicenseValidator* m_validator;
    bool m_invalidLicensesDimmed;
};
