// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark.h>

class QnSearchBookmarksModelPrivate;

class QnSearchBookmarksModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column
    {
        kName,
        kCamera,
        kStartTime,
        kLength,
        kCreationTime,
        kCreator,
        kTags,
        kDescription,
        kColumnsCount
    };

public:
    QnSearchBookmarksModel(QObject* parent = nullptr);
    virtual ~QnSearchBookmarksModel();


    static QnBookmarkSortOrder defaultSortOrder();
    static int sortFieldToColumn(nx::vms::api::BookmarkSortField field);

    void applyFilter();

    void setRange(qint64 utcStartTimeMs, qint64 utcFinishTimeMs);

    void setFilterText(const QString& text);

    void setCameras(const std::set<QnUuid>& cameras);

    const std::set<QnUuid>& cameras() const;

    void cancelUpdateOperation();

public: //< QAbstractItemModel overrides

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /// TODO: #ynikitenkov Refactor to use proxy model
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    QModelIndex index(
        int row,
        int column = 0,
        const QModelIndex& parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex& child) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

private:
    Q_DECLARE_PRIVATE(QnSearchBookmarksModel)
    const QScopedPointer<QnSearchBookmarksModelPrivate> d_ptr;
};
