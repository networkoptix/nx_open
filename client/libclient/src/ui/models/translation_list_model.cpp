
#include "translation_list_model.h"

#include <QtGui/QIcon>

#include <client/client_globals.h>


QnTranslationListModel::QnTranslationListModel(QObject* parent):
    base_type(parent)
{
}

QnTranslationListModel::~QnTranslationListModel()
{
}

const QList<QnTranslation>& QnTranslationListModel::translations() const
{
    return m_translations;
}

void QnTranslationListModel::setTranslations(const QList<QnTranslation>& translations)
{
    beginResetModel();
    m_translations = translations;
    endResetModel();
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

    const QnTranslation& translation = m_translations[index.row()];

    switch (role)
    {
        case Qt::EditRole:
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
        case Qt::ToolTipRole:
            return translation.languageName();
        case Qt::DecorationRole:
            return QIcon(QString(lit(":/flags/%1.png")).arg(translation.localeCode()));
        case Qn::TranslationRole:
            return QVariant::fromValue<QnTranslation>(translation);
        default:
            break;
    }

    return QVariant();
}
