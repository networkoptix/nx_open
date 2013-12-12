#include "ptz_tour_model.h"

#include <common/common_globals.h>

#include <core/ptz/ptz_preset.h>

namespace {
    static const qreal speedLowest  = 0.1;
    static const qreal speedLow     = 0.325;
    static const qreal speedNormal  = 0.55;
    static const qreal speedHigh    = 0.775;
    static const qreal speedHighest = 1.0;

    static const QList<qreal> allSpeedValues(QList<qreal>() << speedLowest << speedLow << speedNormal << speedHigh << speedHighest);
}

QnPtzTourModel::QnPtzTourModel(QObject *parent) :
    base_type(parent)
{
    connect(this, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_dataChanged(QModelIndex,QModelIndex)));
}

QnPtzTourModel::~QnPtzTourModel() {
}

QList<qreal> QnPtzTourModel::speedValues() {
    return allSpeedValues;
}

QString QnPtzTourModel::speedToString(qreal speed) {
    static QList<QString> names(QList<QString>() << tr("Lowest") << tr("Low") << tr("Normal") << tr("High") << tr("Highest"));
    Q_ASSERT(names.size() == allSpeedValues.size());

    qreal value = qBound(speedLowest, speed, speedHighest);
    for (int i = 1; i < allSpeedValues.size() - 1; ++i) {
        if (allSpeedValues[i] < value)
            continue;

        qreal prev = value - allSpeedValues[i - 1];
        qreal cur = allSpeedValues[i] - value;
        if (prev < cur)
            return names[i - 1];
        return names[i];
    }

    //should never come here
    DEBUG_CODE(Q_ASSERT(false);)
    return names[allSpeedValues.indexOf(speedNormal)];
}

const QnPtzTour& QnPtzTourModel::tour() const {
    return m_tour;
}

void QnPtzTourModel::setTour(const QnPtzTour &tour) {
    beginResetModel();
    m_tour = tour;
    endResetModel();
}

const QString QnPtzTourModel::tourName() const {
    return m_tour.name;
}

void QnPtzTourModel::setTourName(const QString &name) {
    m_tour.name = name;
    emit tourChanged(m_tour);
}

const QnPtzPresetList& QnPtzTourModel::presets() const {
    return m_presets;
}

void QnPtzTourModel::setPresets(const QnPtzPresetList &presets) {
    beginResetModel();
    m_presets = presets;
    endResetModel();
}

int QnPtzTourModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_tour.spots.size();
    return 0;
}

int QnPtzTourModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return ColumnCount;
    return 0;
}

bool QnPtzTourModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || count < 1 || row + count > m_tour.spots.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_tour.spots.erase(m_tour.spots.begin() + row, m_tour.spots.begin() + row + count);
    endRemoveRows();
    return true;
}

bool QnPtzTourModel::insertRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || row > m_tour.spots.size() || count < 1)
        return false;

    beginInsertRows(parent, row, row + count - 1);
    for(int i = 0; i < count; i++)
        m_tour.spots.insert(row, QnPtzTourSpot());
    endInsertRows();
    return true;
}

Qt::ItemFlags QnPtzTourModel::flags(const QModelIndex &index) const {
    return base_type::flags(index) | Qt::ItemIsEditable; //TODO: #GDM PTZ drag'n'drop?
}

QVariant QnPtzTourModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnPtzTourSpot &spot = m_tour.spots[index.row()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        switch (index.column()) {
        case NameColumn:
            foreach (const QnPtzPreset &preset, m_presets) {
                if (preset.id == spot.presetId)
                    return preset.name;
            }
            return tr("<Invalid>");
        case TimeColumn:
            return spot.stayTime;
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

QVariant QnPtzTourModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < ColumnCount) {
        switch (section) {
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

bool QnPtzTourModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(role != Qt::EditRole)
        return false;

    if(!index.isValid())
        return false;

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return false;

    QnPtzTourSpot &spot = m_tour.spots[index.row()];

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

void QnPtzTourModel::at_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
    emit tourChanged(m_tour);
}
