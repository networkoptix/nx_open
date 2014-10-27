
#include "translation_list_model.h"

#include <QtGui/QIcon>

#include <common/common_globals.h>


QnTranslationListModel::QnTranslationListModel(QObject *parent):
    base_type(parent)
{}

QnTranslationListModel::~QnTranslationListModel() {
    return;
}

const QList<QnTranslation> &QnTranslationListModel::translations() const {
    return m_translations;
}

void QnTranslationListModel::setTranslations(const QList<QnTranslation> &translations) {
    beginResetModel();

    m_translations = translations;
    
    m_hasExternal = false;
    for(const QnTranslation &translation: m_translations) {
        if(!isInternal(translation)) {
            m_hasExternal = true;
            break;
        }
    }

    endResetModel();
}

int QnTranslationListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_translations.size();

    return 0;
}

Qt::ItemFlags QnTranslationListModel::flags(const QModelIndex &) const {
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant QnTranslationListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnTranslation &translation = m_translations[index.row()];

    switch(role) {
    case Qt::EditRole:
    case Qt::DisplayRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::ToolTipRole:
        if(!m_hasExternal) {
            return translation.languageName();
        } else {
            if(isInternal(translation)) {
                return tr("%1 (built-in)").arg(translation.languageName());
            } else {
                return tr("%1 (external)").arg(translation.languageName());
            }
        }
    case Qt::DecorationRole:
        return QIcon(QString(lit(":/flags/%1.png")).arg(translation.localeCode()));
    case Qn::TranslationRole:
        return QVariant::fromValue<QnTranslation>(translation);
    default:
        break;
    }

    return QVariant();
}

bool QnTranslationListModel::setData(const QModelIndex &, const QVariant &, int) {
    return false;
}

bool QnTranslationListModel::isInternal(const QnTranslation &translation) {
    for(const QString &filePath: translation.filePaths())
        if(!filePath.startsWith(lit(":/")))
            return false;
    return true;
}
