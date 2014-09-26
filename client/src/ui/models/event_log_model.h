#ifndef QN_EVENT_LOG_MODEL_H
#define QN_EVENT_LOG_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include <business/business_fwd.h>
#include <business/business_action_parameters.h>


class QnEventLogModel: public QAbstractItemModel {
    Q_OBJECT
    typedef QAbstractItemModel base_type;

public:
    enum Column {
        DateTimeColumn,
        EventColumn,
        EventCameraColumn,
        ActionColumn,
        ActionCameraColumn,
        DescriptionColumn,
        ColumnCount
    };

    QnEventLogModel(QObject *parent = NULL);
    virtual ~QnEventLogModel();

    void setEvents(const QVector<QnBusinessActionDataListPtr> &events);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    bool hasMotionUrl(const QModelIndex &index) const;

    QnBusiness::EventType eventType(int row) const;
    QnResourcePtr eventResource(int row) const;
    qint64 eventTimestamp(int row) const;

public slots:
    void clear();

private:
    class DataIndex;

    QnResourcePtr getResource(const Column &column, const QnBusinessActionData &action) const;
    QVariant fontData(const Column& column, const QnBusinessActionData &action) const;
    QVariant foregroundData(const Column& column, const QnBusinessActionData &action) const;

    static QVariant iconData(const Column& column, const QnBusinessActionData &action);
    static QVariant mouseCursorData(const Column& column, const QnBusinessActionData &action);
    static QString textData(const Column& column, const QnBusinessActionData &action);
    static int helpTopicIdData(const Column& column, const QnBusinessActionData &action);

    static QString motionUrl(Column column, const QnBusinessActionData& action);
    static QString formatUrl(const QString& url);
    static QnResourcePtr getResourceById(const QnUuid& id);
    static QString getResourceNameString(QnUuid id);
    static QString getUserGroupString(QnBusinessActionParameters::UserGroup value);

    void at_resource_removed(const QnResourcePtr &resource);

private:
    QList<Column> m_columns;
    QBrush m_linkBrush;
    QFont m_linkFont;
    DataIndex* m_index;
    static QHash<QnUuid, QnResourcePtr> m_resourcesHash;
};

#endif // QN_EVENT_LOG_MODEL_H
