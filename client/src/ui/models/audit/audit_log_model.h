#ifndef QN_AUDIT_LOG_MODEL_H
#define QN_AUDIT_LOG_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include "api/model/audit/audit_record.h"
#include "client/client_color_types.h"
#include <ui/customization/customized.h>

class QnAuditLogModel: public Customized<QAbstractItemModel>
{
    Q_OBJECT
    Q_PROPERTY(QnAuditLogColors colors READ colors WRITE setColors)
    
    typedef Customized<QAbstractItemModel> base_type;
public:
    enum Column {
        SelectRowColumn,
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
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QnAuditRecordList data() const;
    QnAuditRecordList checkedRows();
    

    QList<Column> columns() const;
    virtual void setColumns(const QList<Column> &columns);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override  { return m_columns.size(); }

    QnAuditLogColors colors() const;
    void setColors(const QnAuditLogColors &colors);
    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const;
public slots:
    void clear();
protected:
    QnAuditRecord rawData(int row) const;
private:
    class DataIndex;

    QString getResourceNameString(QnUuid id) const;
    QString textData(const Column& column,const QnAuditRecord& action, int row) const;
    QString htmlData(const Column& column,const QnAuditRecord& data, int row, bool hovered) const;
    QString formatDateTime(int timestampSecs, bool showDate = true, bool showTime = true) const;
    QString formatDuration(int durationSecs) const;
    QString eventTypeToString(Qn::AuditRecordType recordType) const;
    QString eventDescriptionText(const QnAuditRecord& data) const;
    QVariant colorForType(Qn::AuditRecordType actionType) const;
protected:
    DataIndex* m_index;
    QList<Column> m_columns;
    QnAuditLogColors m_colors;
};

#endif // QN_AUDIT_LOG_MODEL_H
