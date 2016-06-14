/**********************************************************
* Jun 14, 2016
* akolesnikov
***********************************************************/

#pragma once


namespace nx {
namespace hpm {
namespace api {

enum class CloudConnectVersion
{
    /** Used when cloudConnectionVersion attribute is missing in message */
    initial = 1,
    /** #CLOUD-398 */
    tryingEveryAddressOfPeer
};

constexpr const CloudConnectVersion kDefaultCloudConnectVersion =
    CloudConnectVersion::initial;
constexpr const CloudConnectVersion kCurrentCloudConnectVersion = 
    CloudConnectVersion::tryingEveryAddressOfPeer;

}   //namespace api
}   //namespace hpm
}   //namespace nx
