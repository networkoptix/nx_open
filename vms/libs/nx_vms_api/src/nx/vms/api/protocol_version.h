#pragma once

namespace nx::vms::api {

/**
 * Protocol version defines compatibility between different VMS parts such as the desktop client,
 * the VMS server and the cloud. Usually it should be increased when some api structures are changed
 * and logical or network-level compatibility is lost.
 */
NX_VMS_API int protocolVersion();

/**
 * Servers with no proto version support have this version.
 *
 * THIS VALUE MUST NOT BE CHANGED!
 */
static constexpr int kInitialProtocolVersion = 1000;

/**
 * The value of this variable overrides the compiled-in value and makes protocolVersion() return
 * this. This is for testing reasons and should be set as early as possible in the program.
 */
NX_VMS_API extern int protocolVersionOverride;

} // namespace nx::vms::api
