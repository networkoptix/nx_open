// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation_list_model.h"

#include <QtCore/QCoreApplication>

#include <client/client_globals.h>
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/vms/client/desktop/application_context.h>

using namespace nx::vms::client::desktop;

QnTranslationListModel::QnTranslationListModel(QObject* parent):
    base_type(parent)
{
    using namespace nx::i18n;
    TranslationManager* translationManager = appContext()->translationManager();
    for (const Translation& translation: translationManager->translations())
    {
        TranslationInfo info;
        info.localeCode = translation.localeCode;
        info.flag = QIcon(QString(":/flags/%1.png").arg(translation.localeCode));

        auto scopedTranslation = translationManager->installScopedLocale(translation.localeCode);
        info.languageName = QCoreApplication::translate("Language", "Language Name",
            "Language name that will be displayed to the user.");
        m_translations.append(info);
    }
}

QnTranslationListModel::~QnTranslationListModel()
{
}

int QnTranslationListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_translations.size();

    return 0;
}

Qt::ItemFlags QnTranslationListModel::flags(const QModelIndex& /*index*/) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant QnTranslationListModel::data(const QModelIndex& index, int role) const
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
        case TranslationRole:
            return QVariant::fromValue<TranslationInfo>(translation);
        default:
            break;
    }

    return QVariant();
}
