#ifndef QN_AUDIT_LOG_MODEL_H
#define QN_AUDIT_LOG_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include "api/model/audit/audit_record.h"


class QnAuditLogModel: public QAbstractItemModel 
{
    Q_OBJECT
    typedef QAbstractItemModel base_type;

public:
    enum Column {
        TimestampColumn,
        EndTimestampColumn,
        DurationColumn,
        UserNameColumn,
        UserHostColumn,
        //ColumnCount,
        DateColumn,
        TimeColumn,
        UserActivityColumn, // not implemented yet
        EventTypeColumn,
        DescriptionColumn,
        PlayButtonColumn
    };

    QnAuditLogModel(QObject *parent = NULL);
    virtual ~QnAuditLogModel();

    void setData(const QnAuditRecordList &data);
    QnAuditRecordList data() const;

    QList<Column> columns() const;
    virtual void setColumns(const QList<Column> &columns);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    bool hasMotionUrl(const QModelIndex &index) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override  { return m_columns.size(); }
public slots:
    void clear();
private:
    class DataIndex;

    QString getResourceNameString(QnUuid id) const;
    QString textData(const Column& column,const QnAuditRecord& action, int row) const;
    QString htmlData(const Column& column,const QnAuditRecord& data, int row) const;
    QString formatDateTime(int timestampSecs, bool showDate = true, bool showTime = true) const;
    QString formatDuration(int durationSecs) const;
    QString eventTypeToString(Qn::AuditRecordType recordType) const;
    QString eventDescriptionText(const QnAuditRecord& data) const;
private:
    DataIndex* m_index;
    QList<Column> m_columns;
};

#endif // QN_AUDIT_LOG_MODEL_H
