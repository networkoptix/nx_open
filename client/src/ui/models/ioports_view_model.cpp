#include "ioports_view_model.h"

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

QString QnIOPortsViewModel::textData(const QModelIndex &index) const
{
    const QnIOPortData& value = m_data.at(index.row());

    switch(index.column())
    {
    case IdColumn:
        return value.id;
    case TypeColumn:
        return portTypeToString(value.portType);
    case DefaultStateColumn:
        return stateToString(value.getDefaultState());
    case NameColumn:
        return value.getName();
    case AutoResetColumn:
        return QString::number(value.autoResetTimeoutMs);
    default:
        return QString();
    }
}

QVariant QnIOPortsViewModel::editData(const QModelIndex &index) const
{
    const QnIOPortData& value = m_data.at(index.row());

    switch(index.column())
    {
    case IdColumn:
        return value.id;
    case TypeColumn:
        return value.portType;
    case DefaultStateColumn:
        return value.getDefaultState();
    case NameColumn:
        return value.getName();
    case AutoResetColumn:
        return value.autoResetTimeoutMs;
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


    switch(role) {
    case Qn::IOPortDataRole:
        return QVariant::fromValue<QnIOPortData>(m_data.at(index.row()));
    case Qt::DisplayRole:
        return QVariant(textData(index));
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
        return false; // doesn't allowed
    case TypeColumn:
        ioPort.portType= (Qn::IOPortType) value.toInt();
        break;
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
    case AutoResetColumn:
        ioPort.autoResetTimeoutMs = value.toInt();
        break;
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
        case IdColumn:           return tr("#");
        case TypeColumn:         return tr("Type");
        case DefaultStateColumn: return tr("Default state");
        case NameColumn:         return tr("Name");
        case AutoResetColumn:    return tr("Pulse time(ms)");
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
        case IdColumn:
            break;
        case TypeColumn:
            flags |= Qt::ItemIsEditable;
            break;
        case DefaultStateColumn:
        case NameColumn:
            if (value.portType != Qn::PT_Disabled)
                flags |= Qt::ItemIsEditable;
            break;
        case AutoResetColumn:
            if (value.portType == Qn::PT_Output)
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
    case IdColumn:
    case TypeColumn:
    case DefaultStateColumn:
    case NameColumn:
        return value.portType == Qn::PT_Disabled;
    case AutoResetColumn:
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

QString QnIOPortsViewModel::portTypeToString(Qn::IOPortType portType) const {
    const auto disambiguation = "IO Port Type";

    switch (portType) {
    case Qn::PT_Unknown:
        return tr("Unknown", disambiguation);
    case Qn::PT_Disabled:
        return tr("Disabled", disambiguation);
    case Qn::PT_Input:
        return tr("Input", disambiguation);
    case Qn::PT_Output:
        return tr("Output", disambiguation);
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }
    return tr("Invalid", disambiguation);

}

QString QnIOPortsViewModel::stateToString(Qn::IODefaultState state) const {
    const auto disambiguation = "IO Port State";

    switch (state) {
    case Qn::IO_OpenCircuit:
        return tr("Open Circuit", disambiguation);
    case Qn::IO_GroundedCircuit:
        return tr("Grounded circuit", disambiguation);
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }
    return tr("Invalid state", disambiguation);
}
