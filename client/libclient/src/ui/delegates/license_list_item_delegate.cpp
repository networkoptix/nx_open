#include "license_list_item_delegate.h"

#include <ui/models/license_list_model.h>
#include <ui/style/custom_style.h>


QnLicenseListItemDelegate::QnLicenseListItemDelegate(QObject* parent, bool invalidLicensesDimmed):
    base_type(parent),
    m_invalidLicensesDimmed(invalidLicensesDimmed)
{
}

QnLicenseListItemDelegate::~QnLicenseListItemDelegate()
{
}

bool QnLicenseListItemDelegate::invalidLicensesDimmed() const
{
    return m_invalidLicensesDimmed;
}

void QnLicenseListItemDelegate::setInvalidLicensesDimmed(bool value)
{
    m_invalidLicensesDimmed = value;
}

void QnLicenseListItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (!index.isValid())
        return;

    if (index.column() == QnLicenseListModel::LicenseKeyColumn)
    {
        option->font = monospaceFont(option->font);
        option->fontMetrics = QFontMetrics(option->font);
    }

    if (!m_invalidLicensesDimmed || index.data(Qt::ForegroundRole).canConvert<QBrush>())
        return;

    if (auto license = index.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>())
    {
        if (!license->isValid())
            option->state &= ~QStyle::State_Enabled;
    }
}
