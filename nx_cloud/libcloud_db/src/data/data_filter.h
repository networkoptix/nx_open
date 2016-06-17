/**********************************************************
* Oct 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_DATA_FILTER_H
#define NX_CDB_DATA_FILTER_H

#include <QtCore/QUrlQuery>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>


namespace nx {
namespace cdb {
namespace data {

class DataFilter
:
    public stree::AbstractIteratableContainer,
    public stree::AbstractResourceReader
{
    friend bool loadFromUrlQuery(const QUrlQuery& urlQuery, DataFilter* const dataFilter);

public:
    //!Implementation of \a stree::AbstractIteratableContainer::begin
    virtual std::unique_ptr<stree::AbstractConstIterator> begin() const override;
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

    //!Empty filter means data should not be filtered
    bool empty() const;

    /** TODO #ak do something with this class. 
        This variable is required by QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES
    */
    int dummy;

    stree::ResourceContainer& resources();
    const stree::ResourceContainer& resources() const;

private:
    stree::ResourceContainer m_rc;
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, DataFilter* const dataFilter);

#define DataFilter_Fields (dummy)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (DataFilter),
    (json))

}   //data
}   //cdb
}   //nx

#endif  //NX_CDB_DATA_FILTER_H
