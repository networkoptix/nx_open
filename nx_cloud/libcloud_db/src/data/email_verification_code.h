/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#ifndef EMAIL_VERIFICATION_CODE_H
#define EMAIL_VERIFICATION_CODE_H

#include <string>

#include <QtCore/QUrlQuery>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/fusion/fusion_fwd.h>


namespace nx {
namespace cdb {
namespace data {


class EmailVerificationCode
:
    public stree::AbstractResourceReader
{
public:
    std::string code;

    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery( const QUrlQuery& urlQuery, EmailVerificationCode* const data );

#define EmailVerificationCode_Fields (code)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (EmailVerificationCode),
    (json)(sql_record) )


}   //data
}   //cdb
}   //nx

#endif  //EMAIL_VERIFICATION_CODE_H
