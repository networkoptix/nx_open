#include "ptz_tour_spots_model.h"

#include <common/common_globals.h>

#include <core/ptz/ptz_preset.h>

#include <ui/style/globals.h>

#include <utils/common/collection.h>
#include <utils/common/string.h>

namespace {
    static const qreal speedLowest  = 0.15;
    static const qreal speedLow     = 0.30;
    static const qreal speedNormal  = 0.5;
    static const qreal speedHigh    = 0.72;
    static const qreal speedHighest = 1.0;

    static const QList<qreal> allSpeedValues(QList<qreal>()
                                             << speedLowest
                                             << speedLow
                                             << speedNormal
                                             << speedHigh
                                             << speedHighest
                                             );

    static const int second = 1000;

    static const QList<quint64> allStayTimeValues(QList<quint64>()
                                                  << 0
                                                  << second * 1
                                                  << second * 2
                                                  << second * 5
                                                  << second * 10
                                                  << second * 15
                                                  << second * 20
                                                  << second * 30
                                                  << second * 45
                                                  << second * 60
                                                  );

    static const QnPtzTourSpot defaultSpot(QString(), second*5, speedNormal);
}

QnPtzTourSpotsModel::QnPtzTourSpotsModel(QObject *parent) :
    base_type(parent)
{
    connect(this, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_dataChanged(QModelIndex,QModelIndex)));
}

QnPtzTourSpotsModel::~QnPtzTourSpotsModel() {
}

QList<qreal> QnPtzTourSpotsModel::speedValues() {
    return allSpeedValues;
}

int QnPtzTourSpotsModel::stayTimeToIndex(quint64 time) {
    return allStayTimeValues.indexOf(time);
}

QList<quint64> QnPtzTourSpotsModel::stayTimeValues() {
    return allStayTimeValues;
}

int QnPtzTourSpotsModel::speedToIndex(qreal speed) {
    int index = -1;
    qreal minDiff = 1.0;
    for (int i = 0; i < allSpeedValues.size(); i++) {
        qreal diff = qAbs(allSpeedValues[i] - speed);
        if (diff < minDiff) {
            minDiff = diff;
            index = i;
        }
    }
    return index;
}

QString QnPtzTourSpotsModel::speedToString(qreal speed) {
    static QList<QString> names(QList<QString>() << tr("Lowest") << tr("Low") << tr("Normal") << tr("High") << tr("Highest"));
    Q_ASSERT(names.size() == allSpeedValues.size());

    int index = speedToIndex(speed);
    return index == -1 ? QString() : names[index];
}

QString QnPtzTourSpotsModel::stayTimeToString(quint64 time) {
    if (time == 0)
        return tr("Instant");
    return tr("%n seconds", "", time / second);
}

const QnPtzTourSpotList& QnPtzTourSpotsModel::spots() const {
    return m_spots;
}

void QnPtzTourSpotsModel::setSpots(const QnPtzTourSpotList &spots) {
    beginResetModel();
    m_spots = spots;
    endResetModel();
}

QnPtzPresetList QnPtzTourSpotsModel::sortedPresets() const {
    QnPtzPresetList result = m_presets;
    qSort(result.begin(), result.end(),  [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return naturalStringLess(l.name, r.name);
    });
    return result;
}

const QnPtzPresetList& QnPtzTourSpotsModel::presets() const {
    return m_presets;
}

void QnPtzTourSpotsModel::setPresets(const QnPtzPresetList &presets) {
    beginResetModel();
    m_presets = presets;
    endResetModel();
}

int QnPtzTourSpotsModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_spots.size();
    return 0;
}

int QnPtzTourSpotsModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return ColumnCount;
    return 0;
}

bool QnPtzTourSpotsModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) {
    if (destinationParent == sourceParent && (destinationChild == sourceRow || destinationChild == sourceRow + 1)) // see QAbstractItemModel docs
        return true;

    if (count < 0 || sourceRow < 0 || sourceRow + count > m_spots.size() || destinationChild < 0 || destinationChild + count > m_spots.size() + 1)
        return false;

    beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild);
    int offset = sourceRow < destinationChild ? -1 : 0; // QAbstractItemModel and QList have different opinion how to move elements
    for (int i = 0; i < count; i++)
        m_spots.move(sourceRow + i, destinationChild + i + offset);
    endMoveRows();
    emit spotsChanged(m_spots);
    return true;
}

bool QnPtzTourSpotsModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || count < 1 || row + count > m_spots.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_spots.erase(m_spots.begin() + row, m_spots.begin() + row + count);
    endRemoveRows();
    emit spotsChanged(m_spots);
    return true;
}

bool QnPtzTourSpotsModel::insertRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || row > m_spots.size() || count < 1)
        return false;

    QnPtzTourSpot sampleSpot(defaultSpot);
    if (!m_presets.isEmpty()) {

        int index = -1;
        if (!m_spots.isEmpty()) {
            QString lastId = m_spots.last().presetId;
            index = qnIndexOf(m_presets, [&](const QnPtzPreset &preset) { return lastId == preset.id; });
        }
        index++;

        if (index >= m_presets.size())
            index = 0;

        sampleSpot.presetId = m_presets[index].id;

    }

    beginInsertRows(parent, row, row + count - 1);
    for(int i = 0; i < count; i++)
        m_spots.insert(row, sampleSpot);
    endInsertRows();
    emit spotsChanged(m_spots);
    return true;
}

Qt::ItemFlags QnPtzTourSpotsModel::flags(const QModelIndex &index) const {
    switch (index.column()) {
    case NumberColumn:
        return base_type::flags(index);
    default:
        return base_type::flags(index) | Qt::ItemIsEditable; //TODO: #GDM #PTZ drag'n'drop?
    }
}

QVariant QnPtzTourSpotsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnPtzTourSpot &spot = m_spots[index.row()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        switch (index.column()) {
        case NumberColumn:
            return index.row() + 1;
        case NameColumn:
            foreach (const QnPtzPreset &preset, m_presets) {
                if (preset.id == spot.presetId)
                    return preset.name;
            }
            return tr("<Invalid>");
        case TimeColumn:
            return stayTimeToString(spot.stayTime);
        case SpeedColumn:
            return speedToString(spot.speed);
        default:
            break;
        }
        return QVariant();

    case Qt::EditRole:
        switch (index.column()) {
        case NameColumn:
            return spot.presetId;
        case TimeColumn:
            return spot.stayTime;
        case SpeedColumn:
            return spot.speed;
        default:
            break;
        }
        return QVariant();

    case Qt::BackgroundRole:
        if (!isPresetValid(spot.presetId)) {
            switch (index.column()) {
            case NameColumn:
                return QBrush(qnGlobals->businessRuleInvalidColumnBackgroundColor());
            case TimeColumn:
            case SpeedColumn:
                return QBrush(qnGlobals->businessRuleInvalidBackgroundColor());
            default:
                break;
            }
        }
        break;

    case Qn::PtzTourSpotRole:
        return QVariant::fromValue<QnPtzTourSpot>(spot);

    case Qn::ValidRole:
        foreach (const QnPtzPreset &preset, m_presets) {
            if (preset.id == spot.presetId)
                return true;
        }
        return false;
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzTourSpotsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < ColumnCount) {
        switch (section) {
        case NumberColumn:
            return QString();
        case NameColumn:
            return tr("Position");
        case TimeColumn:
            return tr("Stay Time");
        case SpeedColumn:
            return tr("Speed");
        default:
            break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

bool QnPtzTourSpotsModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(role != Qt::EditRole)
        return false;

    if(!index.isValid())
        return false;

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return false;

    QnPtzTourSpot &spot = m_spots[index.row()];

    switch(index.column()) {
    case NameColumn: {
        QString presetId = value.toString();
        if(presetId.isEmpty())
            return false;

        if(spot.presetId == presetId)
            return true;

        spot.presetId = presetId;
        emit dataChanged(index, index);
        return true;
    }
    case TimeColumn: {
        bool ok = false;
        quint64 time = value.toULongLong(&ok);
        if(!ok)
            return false;

        spot.stayTime = time;
        emit dataChanged(index, index);
        return true;
    }
    case SpeedColumn: {
        bool ok = false;
        qreal speed = value.toReal(&ok);
        if(!ok)
            return false;

        spot.speed = speed;
        emit dataChanged(index, index);
        return true;
    }
    default:
        return base_type::setData(index, value, role);
    }

}

bool QnPtzTourSpotsModel::isPresetValid(const QString &presetId) const {
    foreach (const QnPtzPreset &preset, m_presets) {
        if (preset.id == presetId)
            return true;
    }
    return false;
}

void QnPtzTourSpotsModel::at_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
    emit spotsChanged(m_spots);
}
