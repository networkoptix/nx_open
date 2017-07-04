#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QSortFilterProxyModel>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/models/resource/resource_compare_helper.h>

struct QnResourceSearchQuery
{
    QString text;
    Qn::ResourceFlags flags = 0;

    QnResourceSearchQuery()
    {
    }

    QnResourceSearchQuery(const QString& text, Qn::ResourceFlags flags = 0):
        text(text),
        flags(flags)
    {
    }

    bool operator==(const QnResourceSearchQuery& other) const
    {
        return text == other.text && flags == other.flags;
    }
};

/**
 * A resource search filtering model.
 */
class QnResourceSearchProxyModel: public QSortFilterProxyModel, protected QnResourceCompareHelper
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    explicit QnResourceSearchProxyModel(QObject* parent = nullptr);
    virtual ~QnResourceSearchProxyModel() override;

    QnResourceSearchQuery query() const;
    void setQuery(const QnResourceSearchQuery& query);

protected:

    // --------------------------------------------------------------
    // Add a override function for lessThan to achieve customization
    // --------------------------------------------------------------
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

signals:
    void criteriaChanged();

    /**
     * This signal is emitted when the tree prepares to start a recursive operation
     * that may lead to a lot of dataChanged signals emitting.
     */
    void beforeRecursiveOperation();

    /**
     * This signal is emitted when the tree ends a recursive operation.
     */
    void afterRecursiveOperation();

public slots:
    void invalidateFilter();

    /**
     * Performs delayed invalidation of the filter.
     *
     * The filter will be invalidated only once even if this function was
     * called several times.
     */
    void invalidateFilterLater();

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    bool m_invalidating = false;
    QnResourceSearchQuery m_query;
};

Q_DECLARE_METATYPE(QnResourceSearchProxyModel*)
