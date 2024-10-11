// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// TODO: #akolesnikov Most (if not all) of these strings are not relevant for nx_network and should be moved
// to vms.
namespace Qn {

static constexpr char GUID_HEADER_NAME[] = "X-guid";
static constexpr char PROXY_SENDER_HEADER_NAME[] = "Nx-Proxy-Sender";
static constexpr char SERVER_GUID_HEADER_NAME[] = "X-server-guid";
static constexpr char CAMERA_GUID_HEADER_NAME[] = "X-camera-guid";
static constexpr char PROXY_TTL_HEADER_NAME[] = "X-proxy-ttl";
static constexpr char CUSTOM_USERNAME_HEADER_NAME[] = "X-Nx-User-Name";
static constexpr char CUSTOM_CHANGE_REALM_HEADER_NAME[] = "X-Nx-Allow-Update-Realm";
static constexpr char AUTH_SESSION_HEADER_NAME[] = "X-Auth-Session";
static constexpr char USER_HOST_HEADER_NAME[] = "X-User-Host";
static constexpr char USER_AGENT_HEADER_NAME[] = "User-Agent";
static constexpr char REALM_HEADER_NAME[] = "X-Nx-Realm";
static constexpr char AUTH_RESULT_HEADER_NAME[] = "X-Auth-Result";
static constexpr char HA1_DIGEST_HEADER_NAME[] = "X-Nx-Digest";
static constexpr char CRYPT_SHA512_HASH_HEADER_NAME[] = "X-Nx-Crypt-Sha512";
static constexpr char RTSP_DATA_FILTER_HEADER_NAME[] = "x-data-filter";
static constexpr char RTSP_DATA_CHUNKS_FILTER_HEADER_NAME[] = "x-chunks-filter";
static constexpr char WEB_RESOURCE_ID_HEADER_NAME[] = "Nx-Web-Resource-Id";

// Flag is used to preserve compatibility with old servers.
static constexpr char RTSP_DATA_SEND_MOTION_HEADER_NAME[] = "x-send-motion";

/** Guid of peer which initiated request. */
static constexpr char PEER_GUID_HEADER_NAME[] = "X-Nx-Peer-Guid";

static constexpr char API_RESULT_CODE_HEADER_NAME[] = "X-Nx-Result-Code";
static constexpr char RTT_MS_HEADER_NAME[] = "X-Nx-rtt-ms";

static constexpr char EC2_SYSTEM_ID_HEADER_NAME[] = "X-Nx-Ec-SYSTEM-Id";
static constexpr char EC2_CONNECTION_STATE_HEADER_NAME[] = "X-Nx-EC-CONNECTION-STATE";
static constexpr char EC2_CONNECTION_DIRECTION_HEADER_NAME[] = "X-Nx-Connection-Direction";
static constexpr char EC2_CONNECTION_GUID_HEADER_NAME[] = "X-Nx-Connection-Guid";
static constexpr char EC2_PING_ENABLED_HEADER_NAME[] = "X-Nx-Ping-Enabled";
static constexpr char EC2_CONNECTION_TIMEOUT_HEADER_NAME[] = "X-Nx-Connection-Timeout";
static constexpr char EC2_GUID_HEADER_NAME[] = "X-guid";
static constexpr char EC2_CONNECT_STAGE_1[] = "Nx-connect-stage1";
static constexpr char EC2_PEER_DATA[] = "Nx-PeerData";
static constexpr char EC2_SERVER_GUID_HEADER_NAME[] = "X-server-guid";
static constexpr char EC2_RUNTIME_GUID_HEADER_NAME[] = "X-runtime-guid";
static constexpr char EC2_DB_GUID_HEADER_NAME[] = "Nx-db-guid";
static constexpr char EC2_SYSTEM_IDENTITY_HEADER_NAME[] = "X-system-identity-time";
static constexpr char EC2_INTERNAL_RTP_FORMAT[] = "X-FFMPEG-RTP";

/** Name of HTTP header holding ec2 proto version. */
static constexpr char EC2_PROTO_VERSION_HEADER_NAME[] = "X-Nx-EC-PROTO-VERSION";

static constexpr char EC2_CLOUD_HOST_HEADER_NAME[] = "X-Nx-EC-CLOUD-HOST";
static constexpr char EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME[] = "X-Nx-base64-encoding-required";
static constexpr char EC2_MEDIA_ROLE[] = "X-Media-Role";

static constexpr char DESKTOP_CAMERA_NO_VIDEO_HEADER_NAME[] = "X-no-video";

static constexpr char URL_QUERY_AUTH_KEY_NAME[] = "auth";

/** Url query item used in api/iomonitor and deprecated api/image. */
static constexpr char PHYSICAL_ID_URL_QUERY_ITEM[] = "cameraId";

static constexpr char FRAME_TIMESTAMP_US_HEADER_NAME[] = "Frame-Timestamp";
static constexpr char FRAME_ENCODED_BY_PLUGIN[] = "Frame-From-Plugin";

static constexpr char kAcceptLanguageHeader[] = "Accept-Language";

static constexpr char kTicketUrlQueryName[] = "_ticket";

static constexpr char VIDEOWALL_INSTANCE_HEADER_NAME[] = "videoWallInstanceGuid";

} // namespace Qn
