#ifndef IOPORTS_VIEW_MODEL_H
#define IOPORTS_VIEW_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <utils/common/id.h>
#include "api/model/api_ioport_data.h"
#include "api/model/recording_stats_reply.h"
#include "core/resource/resource_fwd.h"
#include <ui/customization/customized.h>
#include "client/client_color_types.h"

#include <ui/models/abstract_item_model.h>


class QnSortedRecordingStatsModel: public QSortFilterProxyModel
{
public:
    typedef QSortFilterProxyModel base_type;
    QnSortedRecordingStatsModel(QObject *parent = 0) : base_type(parent) {}
protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

class QnRecordingStatsModel : public Customized<QnAbstractItemModel>
{
    Q_OBJECT
    Q_PROPERTY(QnRecordingStatsColors colors READ colors WRITE setColors)

    typedef Customized<QnAbstractItemModel> base_type;
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

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
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
    QString displayData(const QModelIndex &index) const;
    QString footerDisplayData(const QModelIndex &index) const;
    QnResourcePtr getResource(const QModelIndex &index) const;
    qreal chartData(const QModelIndex &index, bool isForecast) const;
    QString tooltipText(Columns column) const;
    QVariant footerData(const QModelIndex &index, int role) const;
    void setModelDataInternal(const QnRecordingStatsReply& data, QnRecordingStatsReply& result);
private:

    QnRecordingStatsReply m_data;
    QnRecordingStatsReply m_forecastData;
    QnRecordingStatsColors m_colors;
    qint64 m_bitrateSum;
};



#endif // IOPORTS_VIEW_MODEL_H
