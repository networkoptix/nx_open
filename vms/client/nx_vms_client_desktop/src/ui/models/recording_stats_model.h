#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QVariant>

#include <api/model/recording_stats_reply.h>
#include <client/client_color_types.h>
#include <client_core/connection_context_aware.h>
#include <ui/customization/customized.h>
#include <ui/models/recording_stats_adapter.h>
#include <utils/common/id.h>

class QnSortedRecordingStatsModel: public QSortFilterProxyModel
{
public:
    typedef QSortFilterProxyModel base_type;
    QnSortedRecordingStatsModel(QObject* parent = nullptr) : base_type(parent) {}

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

class QnTotalRecordingStatsModel: public QSortFilterProxyModel
{
public:
    typedef QSortFilterProxyModel base_type;
    QnTotalRecordingStatsModel(QObject* parent = nullptr) : base_type(parent) {}

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};


class QnRecordingStatsModel : public Customized<QAbstractListModel>,
    public QnConnectionContextAware
{
    Q_OBJECT
    typedef Customized<QAbstractListModel> base_type;

public:

    enum DataRoles
    {
        RowKind = Qn::ItemDataRole::RoleCount,
        StatsData,
        ChartData
    };

    enum RowType
    {
        Normal,
        Foreign,
        Totals
    };
    Q_ENUM(RowType)

    enum Columns
    {
        CameraNameColumn,
        BytesColumn,
        DurationColumn,
        BitrateColumn,
        ColumnCount
    };

    explicit QnRecordingStatsModel(bool isForecastRole, QObject* parent = 0);
    virtual ~QnRecordingStatsModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void clear();
    void setModelData(const QnCameraStatsData& data);
    QnCameraStatsData modelData() const;

    void setHeaderTextBlocked(bool value);

signals:
    void colorsChanged();

private:
    QString displayData(const QModelIndex& index) const;
    qreal chartData(const QModelIndex& index) const;
    QString tooltipText(Columns column) const;

    QString formatBitrateString(qint64 bitrate) const;
    QString formatBytesString(qint64 bytes) const;
    QString formatDurationString(const QnCamRecordingStatsData& data) const;

    QnCameraStatsData m_data;

    QnRecordingStatsColors m_colors;
    bool m_isForecastRole;
    bool m_isHeaderTextBlocked = false;
};
