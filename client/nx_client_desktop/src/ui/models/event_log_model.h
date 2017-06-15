#pragma once

#include <vector>

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include <business/business_fwd.h>
#include <business/business_action_parameters.h>

#include <ui/workbench/workbench_context_aware.h>

class QnBusinessStringsHelper;

class QnEventLogModel: public QAbstractItemModel, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QAbstractItemModel;
public:
    enum Column
    {
        DateTimeColumn,
        EventColumn,
        EventCameraColumn,
        ActionColumn,
        ActionCameraColumn,
        DescriptionColumn,
        ColumnCount
    };

    QnEventLogModel(QObject* parent = nullptr);
    virtual ~QnEventLogModel();

    void setEvents(const QVector<QnBusinessActionDataListPtr> &events);

    QList<Column> columns() const;
    void setColumns(const QList<Column>& columns);

    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QnResourceList resourcesForPlayback(const QModelIndex& index) const;

    QnBusiness::EventType eventType(int row) const;
    QnResourcePtr eventResource(int row) const;
    qint64 eventTimestamp(int row) const;

    public slots:
    void clear();

private:
    class DataIndex;

    QnResourcePtr getResource(Column column, const QnBusinessActionData& action) const;
    QVariant foregroundData(Column column, const QnBusinessActionData& action) const;

    static QVariant iconData(Column column, const QnBusinessActionData& action);
    QVariant mouseCursorData(Column column, const QnBusinessActionData& action) const;
    QString textData(Column column, const QnBusinessActionData& action) const;
    QString tooltip(Column column, const QnBusinessActionData& action) const;

    static int helpTopicIdData(Column column, const QnBusinessActionData& action);

    QString motionUrl(Column column, const QnBusinessActionData& action) const;
    QString getSubjectNameById(const QnUuid& id) const;
    QString getSubjectsText(const std::vector<QnUuid>& ids) const;
    static QString getResourceNameString(const QnUuid& id);

    bool hasVideoLink(const QnBusinessActionData& action) const;
    bool hasAccessToCamera(const QnUuid& cameraId) const;
    bool hasAccessToArchive(const QnUuid& cameraId) const;
private:
    QList<Column> m_columns;
    QBrush m_linkBrush;
    QScopedPointer<DataIndex> m_index;
    std::unique_ptr<QnBusinessStringsHelper> m_helper;
};
