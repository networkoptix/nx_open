#include <nx/fusion/model_functions.h>
#include "update_information.h"

namespace nx {
namespace update {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::update, Component,
    (nx::update::Component::client, "client")
    (nx::update::Component::server, "server"))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Package, (eq)(json), Package_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Information, (eq)(json), Information_Fields)

Information updateInformation(const QString& /*publicationKey*/, InformationError* /*error*/)
{
    return Information();
}

Information updateInformationFromZipFile(const QString& /*zipFileName*/, InformationError* /*error*/)
{
    return Information();
}

} // namespace update
} // namespace nx

