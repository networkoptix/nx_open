#include "resource_search_proxy_model.h"
#include "resource_model.h"

bool QnResourceSearchProxyModelPrivate::matchesFilters(const QRegExp filters[], Node *node, int source_row, const QModelIndex &source_parent) const
{
    Q_Q(const QnResourceSearchProxyModel);

    if (!filters[Id].isEmpty()) {
        const QString id = QString::number(node->id());
        if (filters[Id].exactMatch(id))
            return true;
    }

    if (!filters[Name].isEmpty()) {
        const QString name = node->name();
        if (filters[Name].exactMatch(name))
            return true;
    }

    if (!filters[Tags].isEmpty()) {
        QString tags = node->resource()->tagList().join(QLatin1String("\",\""));
        if (!tags.isEmpty())
            tags = QLatin1Char('"') + tags + QLatin1Char('"');
        if (filters[Tags].exactMatch(tags))
            return true;
    }

    if (!filters[Text].isEmpty()) {
        Q_ASSERT(source_parent.isValid()); // checked in filterAcceptsRow()
        const int filter_role = q->filterRole();
        const int filter_column = q->filterKeyColumn();
        if (filter_column == -1) {
            const int columnCount = source_parent.model()->columnCount(source_parent);
            for (int column = 0; column < columnCount; ++column) {
                const QModelIndex source_index = source_parent.child(source_row, column);
                const QString text = source_index.data(filter_role).toString();
                if (filters[Text].exactMatch(text))
                    return true;
            }
        } else {
            const QModelIndex source_index = source_parent.child(source_row, filter_column);
            if (source_index.isValid()) { // the column may not exist
                const QString text = source_index.data(filter_role).toString();
                if (filters[Text].exactMatch(text))
                    return true;
            }
        }
    }

    return false;
}

QString QnResourceSearchProxyModelPrivate::normalizedFilterString(const QString &str)
{
    QString ret = str;
    ret.replace(QLatin1Char('"'), QString());
    return ret;
}

void QnResourceSearchProxyModelPrivate::buildFilters(const QSet<QString> parts[], QRegExp *filters)
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

void QnResourceSearchProxyModelPrivate::parseFilterString()
{
    Q_Q(QnResourceSearchProxyModel);

    flagsFilter = 0;
    for (uint i = 0; i < NumFilterCategories; ++i) {
        // ### don't invalidate filters which weren't changed
        filters[i] = QRegExp();
        negfilters[i] = QRegExp();
    }

    parsedFilterString = q->filterRegExp().pattern();
    if (parsedFilterString.isEmpty())
        return;

    const QRegExp::PatternSyntax patternSyntax = q->filterRegExp().patternSyntax();
    if (patternSyntax != QRegExp::FixedString && patternSyntax != QRegExp::Wildcard && patternSyntax != QRegExp::WildcardUnix)
        return;

    QSet<QString> filters_[NumFilterCategories];
    QSet<QString> negfilters_[NumFilterCategories];

    const QString filterString = normalizedFilterString(parsedFilterString);
    QRegExp rx(QLatin1String("(\\W?)(\\w+:)?(\\w*)"), Qt::CaseSensitive, QRegExp::RegExp2);
    int pos = 0;
    while ((pos = rx.indexIn(filterString, pos)) != -1) {
        pos += rx.matchedLength();
        if (rx.matchedLength() == 0)
            break;

        const QString sign = rx.cap(1);
        const QString key = rx.cap(2).toLower();
        const QString pattern = rx.cap(3);
        if (pattern.isEmpty())
            continue;

        FilterCategory category = Text;
        if (!key.isEmpty()) {
            if (key == QLatin1String("id:"))
                category = Id;
            else if (key == QLatin1String("name:"))
                category = Name;
            else if (key == QLatin1String("tag:"))
                category = Tags;
        } else {
            if (pattern == QLatin1String("live"))
                flagsFilter |= QnResource::live;
        }
        if (sign == QLatin1String("-"))
            negfilters_[category].insert(pattern);
        else
            filters_[category].insert(pattern);
    }

    buildFilters(filters_, filters);
    buildFilters(negfilters_, negfilters);
}


QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent), d_ptr(new QnResourceSearchProxyModelPrivate)
{
    Q_D(QnResourceSearchProxyModel);
    d->q_ptr = this;
    d->parseFilterString();
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel()
{
}

/*QnResourcePtr QnResourceSearchProxyModel::resourceFromIndex(const QModelIndex &index) const
{
    QnResourceModel *resourceModel = qobject_cast<QnResourceModel *>(sourceModel());
    return resourceModel ? resourceModel->resource(mapToSource(index)) : QnResourcePtr(0);
}*/

/*QModelIndex QnResourceSearchProxyModel::indexFromResource(const QnResourcePtr &resource) const
{
    QnResourceModel *resourceModel = qobject_cast<QnResourceModel *>(sourceModel());
    return resourceModel ? mapFromSource(resourceModel->index(resource)) : QModelIndex();
}*/

bool QnResourceSearchProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    /*QnResourceModel *resourceModel = qobject_cast<QnResourceModel *>(sourceModel());
    if (!resourceModel)
        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);*/

    if (!source_parent.isValid())
        return true; /* Include root node. */

    if (parsedFilterString != filterRegExp().pattern())
        parseFilterString();
    if (parsedFilterString.isEmpty())
        return false;

    QModelIndex index = source_parent.child(source_row, 0);
    if(!index.isValid())
        return false;

    //Node *node = resourceModel->d_func()->node(resourceModel->index(source_row, 0, source_parent));
    //if (!node || node->id() == 0)
        //return false;

    if (matchesFilters(negfilters, index))
        return false;

    if (flagsFilter != 0) {
        if ((index.data(QnResourceModel::ResourceFlagsRole).value<quint32>() & flagsFilter) != 0)
            return true;
    }

    if (matchesFilters(filters, index))
        return true;

    return false;
}


#endif