
#pragma once

#include <core/resource/resource_fwd.h>

class QnSearchBookmarksModel : public QAbstractItemModel 
{
    Q_OBJECT

public:
    enum Column
    {
        kName
        , kStartTime
        , kLength
        , kTags
        , kCamera
        , kColumnsCount
    };

public:
    QnSearchBookmarksModel(QObject *parent = nullptr);

    virtual ~QnSearchBookmarksModel();

    void applyFilter(bool clearBookmarksCache);

    void setDates(const QDate &start
        , const QDate &finish);

    void setFilterText(const QString &text);

    void setCameras(const QnVirtualCameraResourceList &cameras);

    /// QAbstractItemModel overrides

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &child) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
        
private:
    typedef QAbstractItemModel Base;
    class Impl;

    Impl * const m_impl;
};