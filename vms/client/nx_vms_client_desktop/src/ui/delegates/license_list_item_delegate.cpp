// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_list_item_delegate.h"

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/license/validator.h>
#include <ui/models/license_list_model.h>

QnLicenseListItemDelegate::QnLicenseListItemDelegate(QObject* parent, bool invalidLicensesDimmed):
    base_type(parent),
    m_validator(new nx::vms::license::Validator(systemContext(), this)),
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

    if (index.data(nx::vms::client::core::ResourceRole).value<QnResourcePtr>())
        option->font.setWeight(QFont::Normal);

    if (index.column() == QnLicenseListModel::LicenseKeyColumn)
        option->font = monospaceFont(option->font);

    option->fontMetrics = QFontMetrics(option->font);

    if (!m_invalidLicensesDimmed || index.data(Qt::ForegroundRole).canConvert<QBrush>())
        return;

    if (auto license = index.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>())
    {
        if (!m_validator->isValid(license))
            option->state &= ~QStyle::State_Enabled;
    }
}
