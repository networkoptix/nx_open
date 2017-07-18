#include "ioports_view_model.h"
#include <nx/utils/string.h>

namespace
{
    static const int kMaxIdLength = 20;
}

#include <client/client_globals.h>

QnIOPortsViewModel::QnIOPortsViewModel(QObject *parent) :
    base_type(parent)
{}

QnIOPortsViewModel::~QnIOPortsViewModel()
{}

int QnIOPortsViewModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return static_cast<int>(m_data.size());
    return 0;
}

int QnIOPortsViewModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QnIOPortsViewModel::Action QnIOPortsViewModel::actionFor(const QnIOPortData& value) const
{
    if (value.portType != Qn::PT_Output)
        return NoAction;

    return (value.autoResetTimeoutMs ? Impulse : ToggleState);
}

QString QnIOPortsViewModel::textData(const QModelIndex &index) const
{
    const QnIOPortData& value = m_data.at(index.row());

    switch(index.column())
    {
    case NumberColumn:
        return QString::number(index.row() + 1 /* 1-based */);
    case IdColumn:
        return nx::utils::elideString(value.id, kMaxIdLength);
    case TypeColumn:
        return portTypeToString(value.portType);
    case DefaultStateColumn:
        return stateToString(value.getDefaultState());
    case NameColumn:
        return value.getName();
    case ActionColumn:
        return actionToString(actionFor(value));
    case DurationColumn:
        return actionFor(value) == Impulse
            ? QString::number(value.autoResetTimeoutMs)
            : QString();
    default:
        return QString();
    }
}

QVariant QnIOPortsViewModel::editData(const QModelIndex &index) const
{
    const QnIOPortData& value = m_data.at(index.row());

    switch(index.column())
    {
    case NumberColumn:
        return index.row();
    case IdColumn:
        return value.id;
    case TypeColumn:
        return value.portType;
    case DefaultStateColumn:
        return value.getDefaultState();
    case NameColumn:
        return value.getName();
    case ActionColumn:
        return actionFor(value);
    case DurationColumn:
        if (actionFor(value) == Impulse)
            return value.autoResetTimeoutMs;
        return QVariant();
    default:
        return QVariant();
    }
}

QVariant QnIOPortsViewModel::data(const QModelIndex &index, int role) const
{
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();
    if (static_cast<size_t>(index.row()) >= m_data.size())
        return false;

    switch(role)
    {
    case Qn::IOPortDataRole:
        return QVariant::fromValue<QnIOPortData>(m_data.at(index.row()));
    case Qt::DisplayRole:
        return textData(index);
    case Qt::EditRole:
        return editData(index);
    case Qt::TextColorRole:
        break;
    case Qt::BackgroundRole:
        break;
    case Qn::DisabledRole:
        return isDisabledData(index);
    case Qt::FontRole:
        if (index.column() == IdColumn) {
            QFont boldFont;
            boldFont.setBold(true);
            return boldFont;
        }
        break;
    default:
        break;
    }
    return QVariant();
}

bool QnIOPortsViewModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;
    if (static_cast<size_t>(index.row()) >= m_data.size())
        return false;

    if (role != Qt::EditRole)
        return false;

    if (data(index, role) == value)
        return false;

    QnIOPortData& ioPort = m_data.at(index.row());
    switch(index.column())
    {
    case IdColumn:
    case NumberColumn:
        return false; // doesn't allowed

    case TypeColumn:
    {
        ioPort.portType = static_cast<Qn::IOPortType>(value.toInt());
        emit dataChanged(index, index.sibling(index.row(), ColumnCount - 1));
        return true;
    }

    case DefaultStateColumn:
        switch (ioPort.portType)
        {
        case Qn::PT_Input:
            ioPort.iDefaultState = static_cast<Qn::IODefaultState>(value.toInt());
            break;
        case Qn::PT_Output:
            ioPort.oDefaultState = static_cast<Qn::IODefaultState>(value.toInt());
            break;
        default:
            return false;
        }
        break;
    case NameColumn:
        switch (ioPort.portType)
        {
        case Qn::PT_Input:
            ioPort.inputName = value.toString();
            break;
        case Qn::PT_Output:
            ioPort.outputName = value.toString();
            break;
        default:
            return false;
        }
        break;

    case ActionColumn:
    {
        const auto newAction = static_cast<Action>(value.toInt());
        if (ioPort.portType != Qn::PT_Output || actionFor(ioPort) == newAction)
            return false;
        static const int kDefaultDurationMs = 1000;
        ioPort.autoResetTimeoutMs = (newAction == Impulse ? kDefaultDurationMs : 0);
        const auto durationIndex = index.sibling(index.row(), DurationColumn);
        emit dataChanged(durationIndex, durationIndex);
        break;
    }

    case DurationColumn:
    {
        if (ioPort.portType != Qn::PT_Output)
            return false;
        const auto oldAction = actionFor(ioPort);
        ioPort.autoResetTimeoutMs = value.toInt();
        if (actionFor(ioPort) == oldAction)
            break;
        const auto actionIndex = index.sibling(index.row(), ActionColumn);
        emit dataChanged(actionIndex, actionIndex);
        break;
    }

    default:
        return false;
    }

    emit dataChanged(index, index);
    return true;
}

QVariant QnIOPortsViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < ColumnCount) {
        switch(section) {
        case NumberColumn:       return lit("#");
        case IdColumn:           return tr("Id");
        case TypeColumn:         return tr("Type");
        case DefaultStateColumn: return tr("Default state");
        case NameColumn:         return tr("Name");
        case ActionColumn:       return tr("On click");
        case DurationColumn:     return tr("Duration");
        default:
            break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

Qt::ItemFlags QnIOPortsViewModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = base_type::flags(index);
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return flags;
    const QnIOPortData& value = m_data.at(index.row());

    switch (index.column())
    {
        case NumberColumn:
        case IdColumn:
            break;
        case TypeColumn:
            if (value.supportedPortTypes != value.portType)
                flags |= Qt::ItemIsEditable;
            break;
        case DefaultStateColumn:
        case NameColumn:
            if (value.portType != Qn::PT_Disabled)
                flags |= Qt::ItemIsEditable;
            break;
        case ActionColumn:
            if (value.portType == Qn::PT_Output)
                flags |= Qt::ItemIsEditable;
            break;
        case DurationColumn:
            if (actionFor(value) == Impulse)
                flags |= Qt::ItemIsEditable;
            break;
    }
    return flags;
}

bool QnIOPortsViewModel::isDisabledData(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;
    const QnIOPortData& value = m_data.at(index.row());

    switch (index.column())
    {
    case NumberColumn:
    case IdColumn:
    case TypeColumn:
    case DefaultStateColumn:
    case NameColumn:
        return value.portType == Qn::PT_Disabled;
    case ActionColumn:
    case DurationColumn:
        return value.portType != Qn::PT_Output;
    }
    return false;
}

void QnIOPortsViewModel::clear() {
    beginResetModel();
    m_data.clear();
    endResetModel();
}

void QnIOPortsViewModel::setModelData(const QnIOPortDataList& data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}

QnIOPortDataList QnIOPortsViewModel::modelData() const
{
    return m_data;
}

QString QnIOPortsViewModel::portTypeToString(Qn::IOPortType portType)
{
    switch (portType)
    {
        case Qn::PT_Unknown:
            return tr("Unknown", "IO Port Type");

        case Qn::PT_Disabled:
            return tr("Disabled", "IO Port Type");

        case Qn::PT_Input:
            return tr("Input", "IO Port Type");

        case Qn::PT_Output:
            return tr("Output", "IO Port Type");
    }

    NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
    return tr("Invalid", "IO Port Type");
}

QString QnIOPortsViewModel::stateToString(Qn::IODefaultState state)
{
    switch (state)
    {
        case Qn::IO_OpenCircuit:
            return tr("Open circuit", "IO Port State");

        case Qn::IO_GroundedCircuit:
            return tr("Grounded circuit", "IO Port State");
    }

    NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
    return tr("Invalid state", "IO Port State");
}

QString QnIOPortsViewModel::actionToString(Action action)
{
    switch (action)
    {
        case ToggleState:
            return tr("Toggle state", "IO Output Port Action");

        case Impulse:
           return tr("Impulse", "IO Output Port Action");
    }

    return QString();
}

