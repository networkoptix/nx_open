// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_AUDIT_LOG_MODEL_H
#define QN_AUDIT_LOG_MODEL_H

#include <QtCore/QAbstractListModel>

#include <api/model/audit/audit_record.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/id.h>

class QnAuditLogModel: public QAbstractListModel, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    enum Column
    {
        SelectRowColumn,
        TimestampColumn,
        EndTimestampColumn,
        DurationColumn,
        UserNameColumn,
        UserHostColumn,
        DateColumn,
        TimeColumn,
        UserActivityColumn,
        EventTypeColumn,
        DescriptionColumn,

        CameraNameColumn,
        CameraIpColumn,

        ColumnCount
    };

    static const QByteArray ChildCntParamName;
    static const QByteArray CheckedParamName;

    QnAuditLogModel(QObject *parent = nullptr);
    virtual ~QnAuditLogModel();

    /*
    * Model uses reference to the data. Data MUST be alive till model clear call
    */
    virtual void setData(const QnLegacyAuditRecordRefList &data);

    /*
    * Interleave colors for grouping records from a same session
    */
    void calcColorInterleaving();
    void clearData();
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void setData(const QModelIndexList &indexList, const QVariant &value, int role = Qt::EditRole);
    QnLegacyAuditRecordRefList checkedRows();

    virtual void setColumns(const QList<Column> &columns);

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const;

    bool matches(const QnLegacyAuditRecord* record, const QStringList& keywords) const;

    void setHeaderHeight(int value);

    static bool hasDetail(const QnLegacyAuditRecord* record);
    static void setDetail(QnLegacyAuditRecord* record, bool showDetail);
    static QString eventTypeToString(Qn::LegacyAuditRecordType recordType);

    static QnVirtualCameraResourceList getCameras(const QnLegacyAuditRecord* record);

public slots:
    void clear();

protected:
    QnLegacyAuditRecord* rawData(int row) const;

private:
    class DataIndex;

    static QString getResourceNameById(const nx::Uuid& id);
    QString htmlData(const Column& column,const QnLegacyAuditRecord* data, int row, bool hovered) const;
    QString formatDateTime(int timestampSecs, bool showDate = true, bool showTime = true) const;
    static QString formatDateTime(const QDateTime& dateTime, bool showDate = true, bool showTime = true);
    static QString formatDuration(int durationSecs);
    QString eventDescriptionText(const QnLegacyAuditRecord* data) const;
    QVariant colorForType(Qn::LegacyAuditRecordType actionType) const;
    QString textData(const Column& column,const QnLegacyAuditRecord* action) const;
    bool skipDate(const QnLegacyAuditRecord *record, int row) const;
    static QString getResourcesString(const std::vector<nx::Uuid>& resources);
    static QnVirtualCameraResourceList getCameras(const std::vector<nx::Uuid>& resources);
    bool matches(const Column& column, const QnLegacyAuditRecord* data, const QString& keyword) const;
    QString descriptionTooltip(const QnLegacyAuditRecord *record) const;
    bool isDetailDataSupported(const QnLegacyAuditRecord *record) const;

protected:
    QScopedPointer<DataIndex> m_index;
    QList<Column> m_columns;
    int m_headerHeight;
    QVector<int> m_interleaveInfo;
};

#endif // QN_AUDIT_LOG_MODEL_H
