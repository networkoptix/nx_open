/**********************************************************
* Aug 26, 2016
* a.kolesnikov
***********************************************************/

#pragma once


namespace ec2 {
struct ApiUserData;
struct ApiIdData;
}   // namespace ec2

namespace nx {

namespace api {
class SystemSharing;
}   // namespace api

namespace cdb {
namespace ec2 {

void convert(const api::SystemSharing& from, ::ec2::ApiUserData* const to);
void convert(const ::ec2::ApiUserData& from, api::SystemSharing* const to);

void convert(const api::SystemSharing& from, ::ec2::ApiIdData* const to);

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
