#include "abstract_connection.h"

#include <QtCore/QUrlQuery>

#include <cstring> /* For std::strstr. */

#include <api/network_proxy_factory.h>
#include <core/resource/resource.h>
#include <nx/fusion/serialization/lexical_enum.h>

#include <common/common_module.h>

Q_GLOBAL_STATIC(QnEnumLexicalSerializer<int>, qn_abstractConnection_emptySerializer);

