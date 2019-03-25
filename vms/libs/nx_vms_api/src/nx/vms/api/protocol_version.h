#pragma once

namespace nx::vms::api {

/**
 * Protocol version defines compatibility between different VMS parts such as the desktop client,
 * the VMS server and the cloud. Usually it should be increased when some api structures are changed
 * and logical or network-level compatibility is lost.
 */
NX_VMS_API int protocolVersion();

} // namespace nx::vms::api
