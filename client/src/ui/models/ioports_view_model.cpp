#include "ioports_view_model.h"

#include <QtCore/QFileInfo>

#include <client/client_settings.h>

#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include "utils/serialization/lexical.h"


QnIOPortsViewModel::QnIOPortsViewModel(QObject *parent) :
    base_type(parent)
    //QnWorkbenchContextAware(parent)
{
}

QnIOPortsViewModel::~QnIOPortsViewModel() {

}

QModelIndex QnIOPortsViewModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column, (void*)0) : QModelIndex();
}

QModelIndex QnIOPortsViewModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
    return QModelIndex();
}

int QnIOPortsViewModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return (int) m_data.size();
}

int QnIOPortsViewModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ColumnCount;
}

QString QnIOPortsViewModel::textData(const QModelIndex &index) const
{
    const QnIOPortData& value = m_data.at(index.row());

    switch(index.column())
    {
    case IdColumn:
        return value.id;
    case TypeColumn:
        return QnLexical::serialized(value.portType);
    case DefaultStateColumn:
        return value.portType == Qn::PT_Output ? QnLexical::serialized<Qn::IODefaultState>(value.oDefaultState) : QnLexical::serialized<Qn::IODefaultState>(value.iDefaultState);
    case NameColumn:
        return value.portType == Qn::PT_Output ? value.outputName : value.inputName;
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
        switch (value.portType) 
        {
        case Qn::PT_Input:
            return value.iDefaultState;
        case Qn::PT_Output:
            return value.oDefaultState;
        default:
            return QString();
        }
    case NameColumn:
        switch (value.portType) 
        {
        case Qn::PT_Input:
            return value.inputName;
        case Qn::PT_Output:
            return value.outputName;
        default:
            return QString();
        }
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
    if (index.row() >= m_data.size())
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
    if (index.row() >= m_data.size())
        return false;

    if (role != Qt::EditRole)
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
            ioPort.iDefaultState = (Qn::IODefaultState) value.toInt();
            break;
        case Qn::PT_Output:
            ioPort.oDefaultState = (Qn::IODefaultState) value.toInt();
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
    Qt::ItemFlags flags = base_type::flags(index);
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
