// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation_list_model.h"

#include <QtCore/QCoreApplication>
#include <QtQml/QtQml>

#include <nx/branding.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/utils/translation/scoped_locale.h>
#include <nx/vms/utils/translation/translation_manager.h>

namespace nx::vms::client::core {

void TranslationListModel::registerQmlType()
{
    qmlRegisterType<TranslationListModel>("nx.vms.client.core", 1, 0, "TranslationListModel");
}

TranslationListModel::TranslationListModel(QObject* parent):
    base_type(parent)
{
    using namespace nx::vms::utils;
    TranslationManager* const manager = appContext()->translationManager();
    for (const Translation& translation: manager->translations())
    {
        TranslationInfo info;
        info.localeCode = translation.localeCode;
        info.flag = QIcon(QString(":/flags/%1.png").arg(translation.localeCode));

        auto scopedTranslation = manager->installScopedLocale(translation.localeCode);
        info.languageName = QCoreApplication::translate("Language", "Language Name",
            "Language name that will be displayed to the user.");
        if (!NX_ASSERT(info.languageName != "Language Name"))
            info.languageName = info.localeCode;

        m_translations.append(info);
    }
}

TranslationListModel::~TranslationListModel()
{
}

int TranslationListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_translations.size();

    return 0;
}

Qt::ItemFlags TranslationListModel::flags(const QModelIndex& /*index*/) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant TranslationListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()
        || index.model() != this
        || !hasIndex(index.row(), index.column(), index.parent()))
    {
        return QVariant();
    }

    const TranslationInfo& translation = m_translations[index.row()];

    switch (role)
    {
        case Qt::EditRole:
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
        case Qt::ToolTipRole:
            return translation.languageName;
        case Qt::DecorationRole:
            return translation.flag;
        case LocaleCodeRole:
            return translation.localeCode;
        default:
            break;
    }

    return QVariant{};
}

QHash<int, QByteArray> TranslationListModel::roleNames() const
{
    auto result = base_type::roleNames();
    result[LocaleCodeRole] = "localeCode";
    return result;
}

int TranslationListModel::localeIndex(const QString& currentLocale)
{
    int defaultLanguageIndex = -1;
    int result = -1;
    for (int i = 0; i < m_translations.size(); i++)
    {
        const auto translation = m_translations[i];

        if (translation.localeCode == currentLocale)
            result = i;

        if (translation.localeCode == nx::branding::defaultLocale())
            defaultLanguageIndex = i;
    }

    if (result < 0)
    {
        NX_ASSERT(defaultLanguageIndex >= 0, "default language must definitely be present in translations");
        result = std::max(defaultLanguageIndex, 0);
    }
    return result;
}

} // namespace nx::vms::client::core
