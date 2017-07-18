/**********************************************************
* Oct 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_DATA_FILTER_H
#define NX_CDB_DATA_FILTER_H

#include <QtCore/QUrlQuery>

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>


namespace nx {
namespace cdb {
namespace data {

class DataFilter
:
    public nx::utils::stree::AbstractIteratableContainer,
    public nx::utils::stree::AbstractResourceReader,
    public nx::utils::stree::AbstractResourceWriter
{
public:
    //!Implementation of \a nx::utils::stree::AbstractIteratableContainer::begin
    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const override;
    //!Implementation of \a nx::utils::stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
    //!Implementation of \a nx::utils::stree::AbstractResourceWriter::put
    virtual void put(int resID, const QVariant& value) override;

    //!Empty filter means data should not be filtered
    bool empty() const;

    nx::utils::stree::ResourceContainer& resources();
    const nx::utils::stree::ResourceContainer& resources() const;

private:
    nx::utils::stree::ResourceContainer m_rc;
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, DataFilter* const dataFilter);
bool deserialize(QnJsonContext*, const QJsonValue&, DataFilter*);

}   //data
}   //cdb
}   //nx

#endif  //NX_CDB_DATA_FILTER_H
