// When lib_context.cpp is compiled as part of nx_sdk library within the Server, this file is
// included in nx/sdk/helpers/lib_context.cpp to make the return value for nxSdkVersion().
//
// The value placed in this file is a kind of a stub, because the implementation of nxSdkVersion()
// located in the Server's nx_sdk library is called only for plugins which are build as part of the
// VMS (and thus are statically linked with nx_sdk).
//
// In the SDK package, a file with the same name is generated, containing the proper value which
// reflects the version of the VMS, in format "<major>.<minor>.<patch> <meta-version>". Note that
// the build number is intentionally left out, because various VMS builds (e.g. different
// customizations) must yield identical SDK packages, thus, nx-sdk-version must be the same.

"<server>"
