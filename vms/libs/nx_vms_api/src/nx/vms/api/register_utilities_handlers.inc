// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

void registerUtilitiesHandlers(
    MediaServerProcess* mediaServerProcess,
    nx::vms::network::AbstractServerConnector* serverConnector,
    int maxTcpRequestSize)
{
    const auto serverModule = mediaServerProcess->serverModule();

    /**%apidoc GET /{deviceId}
     * Opens RTSP video stream from Device. It is not exactly an API function but rather a URL
     * format. Example:
     * <code>rtsp://&lt;server_ip&gt;:&lt;port&gt;/12AB42FD5912?pos=1235631&amp;resolution=240p</code>
     * <br/>
     * ATTENTION: By default all Users are created with disabled Basic/Digest authentication, so
     * only session-based authentication which requires SSL/TLS connections to be used (see
     * General information about VMS Server API for details). Most of the 3rd party players and
     * libraries (vlc, ffmpeg, live555, etc.) support only standard RTSP, which can be used only
     * for the Users with enabled Basic/Digest authentication.
     * %caption RTSP streaming
     * %ingroup Utilities
     * %param:uuid deviceId Device id (can be obtained from "id", "physicalId" or "logicalId" field
     *     via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain Devices).
     * %param[opt]:string pos By default, Server opens Device at the live position. This parameter
     *     defines position in an archive (as a string containing time in milliseconds since epoch,
     *     or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     *     the format is auto-detected). Position can be passed via standard RTSP attribute as well.
     * %param[opt]:string resolution Turn on video transcoding to the specified resolution.
     *     Resolution string either may contain width and height (for instance 320x240) or height
     *     only (for instance 240p).
     * %param[opt]:enum rotation Rotate item, in degrees. If the parameter is absent, video will be
     *     rotated to the default value defined in Device settings dialog. Parameter take place if
     *     video transcoding is activated (parameter `codec` and/or parameter `resolution` are
     *     presented).
     *     %value 0
     *     %value 90
     *     %value 180
     *     %value 270
     * %param[opt]:enum codec Defines video codec used for transcoding. Default value is H263p.
     *     %value H263p H263 plus format
     *     %value H264 H264 format
     *     %value mpeg2video
     *     %value mjpeg Motion JPEG
     *     %value mpeg4 MPEG-4 part 2
     *     %value libvpx VP8 video codec
     * %param[opt]:enum acodec Defines audio codec used for transcoding.
     *     %value aac AAC format
     *     %value mp2 Mpeg Layer 2 format
     * %param[opt]:integer stream Open high quality stream if parameter is 0 or low quality stream
     *     if parameter is 1. By default Server auto-detects preferred stream index based on
     *     destination resolution.
     * %param[opt]:string speed Playback speed. Only values in range [1..32] are supported. Default
     *     value is 1. The special value `max` means streaming without delay.
     * %param[opt]:string multiple_payload_types Generate SDP with two payload types (high and low
     *     qualities). Payload type of RTP packets will be changed in according to transmitted
     *     stream.
     * %param[opt]:option onvif_replay Insert ONVIF header extension that contains absolute
     *     timestamp (See: ONVIF Streaming Specification Ver. 17.06, 6.3 RTP header extension).
     * %param[opt]:option disable_fast_channel_zapping Disable fast channel zapping, the stream
     *     will wait before starting, until the next key frame from Device is received.
     * %param[opt]:option enable_analytics_objects Add an RTSP channel that contains Analytics
     *     Objects data in the JSON format.
     * %return RTSP video/audio/metadata streams.
     * NOTE for analytics streaming: if Device has analytics plugin then RTSP server creates
     * addition track with type 'text'. This track contains analytics objects in the JSON format described below.
     * RTP packet has 'marker' bit in case of it includes the end of a Json object.
     * %struct ObjectMetadataPacket
     */
    registerTcpHandler<QnRtspConnectionProcessor>(
        mediaServerProcess,
        /*protocol*/ "RTSP",
        /*path*/ "*",
        serverModule);

    /**%apidoc GET /api/http_audio
     * Open websocket audio stream to Device. Example:
     * <code>
     * wss://&lt;server_ip&gt;:&lt;port&gt;/api/http_audio?camera_id=2a4717bb-1d3e-4878-a28b-af4eaedbfb89&format=f32le&sample_rate=44100&channels=1
     * </code>
     * %caption Audio backchannel streaming
     * %ingroup Utilities
     * %param:uuid camera_id Device id (can be obtained from "id", "physicalId" or "logicalId" field
     *     via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain Devices).
     * %param[opt]:enum format Audio sample format.
     *     %value u8
     *     %value s16be
     *     %value s16le
     *     %value s32be
     *     %value s32le
     *     %value f32be
     *     %value f32le
     *     %value f64be
     *     %value f64le
     * %param[opt]:integer sample_rate Audio sample rate.
     * %param[opt]:enum channels Audio channel count. 1 for mono, 2 for stereo.
     *     %value 1
     *     %value 2
     * %return Audio stream in the requested format.
     */
    registerTcpHandler<http_audio::AudioRequestProcessor>(
        mediaServerProcess,
        /*protocol*/ "HTTP",
        /*path*/ "api/http_audio",
        serverModule);

    /**%apidoc GET /proxy/{protocol}/{serverId}/{apiRequest}
     * <p>
     * Proxy methods allow to send any request to any Server using the existing public connection.
     * It is useful to login on a Server that is hidden from the requestor via executing
     * `POST /proxy/https-insecure/{hiddenServerEndpoint}/rest/v{1-}/login/sessions` to the proxy
     * Server that has access to the hidden Server, this request requires Administrator permission
     * on the proxy Server.
     * </p>
     * <p>
     * Can also be used to get video streams. Example:
     * <code>
     * rtsp://&lt;server_ip&gt;:&lt;port&gt;/proxy/rtsp/{72934575-ceb7-54bb-23a0-84b81cf1d3f1}/12AB42FD5912?pos=1235631&resolution=240p
     * </code>
     * Example:
     * <code>
     * https://&lt;server_ip&gt;:&lt;port&gt;/proxy/https/{72934575-ceb7-54bb-23a0-84b81cf1d3f1}/showLog
     * </code>
     * </p>
     * %caption Proxying
     * %ingroup Utilities
     * %param:string protocol
     * %param:uuid serverId
     * %param:string apiRequest
     * %return Result of the target API request.
     */
    registerTcpHandler<nx::vms::server::network::ProxyConnectionProcessor>(
        mediaServerProcess,
        /*protocol*/ "*",
        /*path*/ "proxy",
        /*ProxyConnectionProcessor/reverseConnectionManager*/ serverConnector,
        serverModule->certificateVerifier(),
        [authenticator = mediaServerProcess->universalTcpListener()->authenticator()](
            auto&&... args)
        {
            return authenticator->watch(std::forward<decltype(args)>(args)...);
        },
        serverModule->auditManager(),
        maxTcpRequestSize);
}
