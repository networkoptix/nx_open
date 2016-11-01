#pragma once

#include "resource_item_delegate.h"


class QnLicenseListItemDelegate : public QnResourceItemDelegate
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
    bool m_invalidLicensesDimmed;
};
