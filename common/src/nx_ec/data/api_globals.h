#ifndef QN_API_GLOBALS_H
#define QN_API_GLOBALS_H

/** 
 * \file
 * 
 * This is an abstraction layer header that makes it possible to use ec
 * api data types in other projects that do not depend on Qt. 
 * 
 * This can be done by simply defining <tt>QN_API_GLOBALS_HEADER</tt> to a name 
 * of the header file that defines all the types used in api data declarations.
 * If this macro is not defined, then default types from Qt will be used.
 */

#ifndef QN_NO_QT
#   include <QtCore/QString>
#   include <QtCore/QByteArray>
#endif

#ifndef QN_NO_BASE
#   include <utils/common/id.h>
#   include <utils/common/email.h>
#   include <utils/common/latin1_array.h>
#   include <api/model/email_attachment.h>
#   include <core/resource/resource_fwd.h>
#   include <business/business_fwd.h>
#   include "utils/common/latin1_array.h"
#endif

#   include "api_fwd.h"

#endif // QN_API_GLOBALS_H
