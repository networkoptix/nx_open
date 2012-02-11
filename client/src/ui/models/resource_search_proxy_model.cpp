#include "resource_search_proxy_model.h"
#include "resource_model.h"

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent): 
    QSortFilterProxyModel(parent)
{}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel() {
    return;
}

void QnResourceSearchProxyModel::addCriterion(const QnResourceCriterion &criterion) {
    m_criterionGroup.addCriterion(criterion);

    invalidateFilter();
}

bool QnResourceSearchProxyModel::removeCriterion(const QnResourceCriterion &criterion) {
    bool removed = m_criterionGroup.removeCriterion(criterion);
    if(removed)
        invalidateFilter();

    return removed;
}

bool QnResourceSearchProxyModel::replaceCriterion(const QnResourceCriterion &from, const QnResourceCriterion &to) {
    bool replaced = m_criterionGroup.replaceCriterion(from, to);
    if(replaced)
        invalidateFilter();

    return replaced;
}

bool QnResourceSearchProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (!source_parent.isValid())
        return true; /* Include root node. */

    QModelIndex index = source_parent.child(source_row, 0);
    if(!index.isValid())
        return true;

    QnResourcePtr resource = index.data(QnResourceModel::ResourceRole).value<QnResourcePtr>();
    if(resource.isNull())
        return true;

    QnResourceCriterion::Operation operation = m_criterionGroup.match(resource);
    if(operation == QnResourceCriterion::REJECT)
        return false;

    return true;
}

#if 0
QString QnResourceSearchProxyModel::normalized(const QString &pattern) const {
    QString result = pattern;
    result.replace(QChar('"'), QString());
    result.replace(QLatin1String("  "), QLatin1String(" "));
    return result;
}

void QnResourceSearchProxyModel::ensureParsedCriterion() const {
    QString filter = filterRegExp().pattern();
    if(m_criterionString == filter) 
        return;

    m_criterionString = filter;

    /* Empty filter matches nothing. */
    if (filter.isEmpty()) {
        m_parsedCriterion = QnResourceCriterion();
        return;
    }

    /* For non-fixed string regexes, use search string matching. */
    const QRegExp::PatternSyntax patternSyntax = filterRegExp().patternSyntax();
    if (patternSyntax != QRegExp::FixedString) {
        m_parsedCriterion = QnResourceCriterion(filterRegExp());
        return;
    }

    /* Parse filter string. */
    QnResourceCriterionGroup parsedCriterion(QnResourceCriterionGroup::ANY_GROUP);

    filter = normalized(filter);
    QRegExp rx(QLatin1String("(\\W?)(\\w+:)?(\\w*)"), Qt::CaseSensitive, QRegExp::RegExp2);
    int pos = 0;
    while ((pos = rx.indexIn(filter, pos)) != -1) {
        pos += rx.matchedLength();
        if (rx.matchedLength() == 0)
            break;

        const QString sign = rx.cap(1);
        const QString key = rx.cap(2).toLower();
        const QString pattern = rx.cap(3);
        if (pattern.isEmpty())
            continue;

        QnResourceCriterion::Operation operation = QnResourceCriterion::INCLUDE;
        QnResourceCriterion::Type type = QnResourceCriterion::CONTAINMENT;
        QnResourceCriterion::Target target = QnResourceCriterion::SEARCH_STRING;
        QVariant targetValue = pattern;
        if (!key.isEmpty()) {
            if (key == QLatin1String("id:")) {
                target = QnResourceCriterion::ID;
                type = QnResourceCriterion::EQUALITY;
            } else if (key == QLatin1String("name:")) {
                target = QnResourceCriterion::NAME;
            } else if (key == QLatin1String("tag")) {
                target = QnResourceCriterion::TAGS;
            }
        } else if (pattern == QLatin1String("live")) {
            target = QnResourceCriterion::FLAGS;
            targetValue = static_cast<int>(QnResource::live);
        }

        if (sign == QLatin1String("-"))
            operation = QnResourceCriterion::EXCLUDE;

        parsedCriterion.addCriterion(QnResourceCriterion(targetValue, type, target, operation));
    }
}

#if 0
void QnResourceSearchProxyModel::buildFilters(const QSet<QString> parts[], QRegExp *filters)
{
    for (uint i = 0; i < NumFilterCategories; ++i) {
        if (!parts[i].isEmpty()) {
            QString pattern;
            foreach (const QString &part, parts[i]) {
                if (!pattern.isEmpty())
                    pattern += QLatin1Char('|');
                pattern += QRegExp::escape(part);
            }
            pattern = QLatin1Char('(') + pattern + QLatin1Char(')');
            if (i < Id) {
                pattern = QLatin1String(".*") + pattern + QLatin1String(".*");
                filters[i] = QRegExp(pattern, Qt::CaseInsensitive, QRegExp::RegExp2);
            } else {
                filters[i] = QRegExp(pattern, Qt::CaseSensitive, QRegExp::RegExp2);
            }
        }
    }
}
#endif

#endif