#include "fake_resource_list_model.h"

#include <QtGui/QFontMetrics>

#include <utils/math/math.h>

namespace {

const auto kNameFont =
    []() -> QFont
    {
        QFont font;
        font.setBold(true);
        return font;
    }();

} //namespace

QnFakeResourceListModel::QnFakeResourceListModel(
    const QnFakeResourceList& fakeResources,
    QObject *parent)
    :
    m_resources(fakeResources)
{
}

QnFakeResourceListModel::~QnFakeResourceListModel()
{
}

QnFakeResourceList QnFakeResourceListModel::resources() const
{
    return m_resources;
}

int QnFakeResourceListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        NX_EXPECT(false, "Wrong parent");
        return 0;
    }

    return m_resources.size();
}

QVariant QnFakeResourceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return QVariant();

    const auto& data = m_resources[row];
    switch(index.column())
    {
        case Columns::iconColumn:
            if (role == Qt::DecorationRole)
                return data.icon;
            break;

        case Columns::nameColumn:
            if (role == Qt::DisplayRole)
                return data.name;
            if (role == Qt::FontRole)
                return kNameFont;
            break;

        case Columns::addressColumn:
            if (role == Qt::DisplayRole)
                return data.address;
            if (role == Qt::TextColorRole)
                return QPalette().windowText();
            break;

        default:
            break;
    }
    return QVariant();
}

int QnFakeResourceListModel::columnCount(const QModelIndex &parent) const
{
    return Columns::count;
}
