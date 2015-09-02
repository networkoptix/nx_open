#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <utils/common/id.h>
#include "api/model/recording_stats_reply.h"
#include "core/resource/resource_fwd.h"
#include <ui/customization/customized.h>
#include "client/client_color_types.h"

class QnSortedRecordingStatsModel: public QSortFilterProxyModel
{
public:
    typedef QSortFilterProxyModel base_type;
    QnSortedRecordingStatsModel(QObject *parent = 0) : base_type(parent) {}
protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

struct QnFooterData: public QnCamRecordingStatsData {
    QnFooterData() : QnCamRecordingStatsData(), bitrateSum(0) {}

    qint64 bitrateSum;
};

class QnRecordingStatsModel : public Customized<QAbstractListModel>
{
    Q_OBJECT
    Q_PROPERTY(QnRecordingStatsColors colors READ colors WRITE setColors)

    typedef Customized<QAbstractListModel> base_type;
public:

    enum Columns {
        CameraNameColumn,
        BytesColumn,
        DurationColumn,
        BitrateColumn,
        ColumnCount
    };


    explicit QnRecordingStatsModel(QObject *parent = 0);
    virtual ~QnRecordingStatsModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void clear();
    void setModelData(const QnRecordingStatsReply& data);
    QnRecordingStatsReply modelData() const;
    void setForecastData(const QnRecordingStatsReply& data);

    QnRecordingStatsColors colors() const;
    void setColors(const QnRecordingStatsColors &colors);
signals:
    void colorsChanged();
private:
    const QnRecordingStatsReply& internalModelData() const;
    const QnFooterData& internalFooterData() const;

    QString displayData(const QModelIndex &index) const;
    QString footerDisplayData(const QModelIndex &index) const;
    QnResourcePtr getResource(const QModelIndex &index) const;
    qreal chartData(const QModelIndex &index, bool isForecast) const;
    QString tooltipText(Columns column) const;
    QVariant footerData(const QModelIndex &index, int role) const;

    QnFooterData calculateFooter(const QnRecordingStatsReply& data) const;

    QString formatBitrateString(qint64 bitrate) const;
    QString formatBytesString(qint64 bytes) const;
    QString formatDurationString(const QnCamRecordingStatsData &data) const;
private:

    QnRecordingStatsReply m_data;
    QnFooterData m_footer;

    QnRecordingStatsReply m_forecastData;
    QnFooterData m_forecastFooter;

    QnRecordingStatsColors m_colors;
    
};
