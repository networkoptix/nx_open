#ifndef QN_API_TYPES_I_H
#define QN_API_TYPES_I_H

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

#ifdef QN_API_GLOBALS_HEADER
#   include QN_API_GLOBALS_HEADER
#else

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <api/model/email_attachment.h>

#include <utils/common/id.h>
#include <business/business_fwd.h>
#include <core/resource/media_resource.h>
#include <utils/common/email.h>

#include "api_fwd.h"

#endif // QN_API_TYPES_HEADER

#endif // QN_API_TYPES_I_H
