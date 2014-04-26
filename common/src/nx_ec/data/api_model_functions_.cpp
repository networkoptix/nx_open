#include "api_fwd.h"

#include <boost/preprocessor/seq/to_tuple.hpp>

#include <utils/common/model_functions.h>

#include "api_business_rule_data.h"
#include "api_camera_data.h"
#include "api_camera_server_item_data.h"
#include "api_connection_data.h"
#include "api_data.h"
#include "api_email_data.h"
#include "api_full_info_data.h"
#include "api_layout_data.h"
#include "api_license_data.h"
#include "api_lock_data.h"
#include "api_media_server_data.h"
#include "api_resource_data.h"
#include "api_resource_type_data.h"
#include "api_runtime_data.h"
#include "api_server_alive_data.h"
#include "api_server_info_data.h"
#include "api_stored_file_data.h"
#include "api_user_data.h"
#include "api_videowall_data.h"

/* Some fields are not meant to be bound or fetched. */
template<class T, class Allocator>
inline void serialize_field(const std::vector<T, Allocator> &, const QString &, QSqlQuery *) { return; }
template<class Key, class T, class Predicate, class Allocator>
inline void serialize_field(const std::map<Key, T, Predicate, Allocator> &, const QString &, QSqlQuery *) { return; }
template<class T>
inline void serialize_field(const QList<T> &, const QString &, QSqlQuery *) { return; }
inline void serialize_field(const ec2::ApiServerInfoData &, const QString &, QSqlQuery *) { return; }

template<class T, class Allocator>
inline void deserialize_field(const QSqlRecord &, int, std::vector<T, Allocator> *) { return; }
template<class Key, class T, class Predicate, class Allocator>
inline void deserialize_field(const QSqlRecord &, int, std::map<Key, T, Predicate, Allocator> *) { return; }
template<class T>
inline void deserialize_field(const QSqlRecord &, int, QList<T> *) { return; }
inline void deserialize_field(const QSqlRecord &, int, ec2::ApiServerInfoData *) { return; }


namespace ec2 {

#define QN_EC2_ADAPT_API_DATA_CLASSES(CLASSES)                                  \
    BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(CLASSES), QN_EC2_ADAPT_API_DATA_CLASSES_STEP_I, BOOST_PP_SEQ_TO_TUPLE(CLASSES))

#define QN_EC2_ADAPT_API_DATA_CLASSES_STEP_I(Z, I, CLASSES)                     \
    QN_EC2_ADAPT_API_DATA_CLASSES_STEP_II(BOOST_PP_TUPLE_ELEM(I, CLASSES))

#define QN_EC2_ADAPT_API_DATA_CLASSES_STEP_II(CLASS)                            \
    QN_FUSION_ADAPT_STRUCT(CLASS, BOOST_PP_CAT(CLASS, _Fields))                 \
    QN_FUSION_DEFINE_FUNCTIONS(CLASS, (binary)(json)(sql))
    
    QN_EC2_ADAPT_API_DATA_CLASSES(QN_EC2_API_DATA_CLASSES)

} // namespace ec2

