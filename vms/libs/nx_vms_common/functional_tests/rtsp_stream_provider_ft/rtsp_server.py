#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import base64
import re
import socket
import struct
import threading
import time

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst


AXIS_MIKEY = (
    "AQAFAJy5yWQBAABpbxJIAAAAAAsA7h7wcVt3bEgKEBTiXX9BnS8aH1LdCb0NcUEBAAAAFQABAQEB"
    "EAIBAQMBCgcBAQgBAQoBAQAAACIAIAAeO741gC1/tzaOFlwe89iZTkduk6SfjkL+dWJJDI8aAA=="
)

AES256_CM_MIKEY = (
    "AQAFAEU/VsoBAAB2G73bAAAAAAsA7hyBwQEupS4KELHQy5v2FHdFI/iTwbm6whMBAAAAFQABAQEB"
    "IAIBAQMBCgcBAQgBAQoBAQAAADIAIAAuw6E7jJyyHO6EDhc9WDP3D2ct3PB1bIU433BkLYCw03WH"
    "KO639QYLmqb2hHUjCwA="
)

MIKEY_PARAM_ENCRYPTION_ALGORITHM = 0
MIKEY_PARAM_ENCRYPTION_KEY_LENGTH = 1
MIKEY_PARAM_AUTHENTICATION_ALGORITHM = 2
MIKEY_PARAM_AUTHENTICATION_KEY_LENGTH = 3
MIKEY_PARAM_AUTHENTICATION_TAG_LENGTH = 11
MIKEY_PARAM_AEAD_AUTHENTICATION_TAG_LENGTH = 20
MIKEY_AES_CM = 1
MIKEY_AES_GCM = 6
MIKEY_AUTH_NULL = 0
MIKEY_AUTH_HMAC_SHA1 = 1

GST_SRTP_CIPHER_AES_128_GCM = 3
GST_SRTP_CIPHER_AES_256_ICM = 2
GST_SRTP_AUTH_NULL = 0


def parse_mikey_payload(payload):
    try:
        data = base64.b64decode(payload, validate=True)
    except ValueError:
        return None

    if len(data) < 19:
        return None

    ssrc = struct.unpack_from(">I", data, 11)[0]
    next_payload = data[2]
    offset = 19
    while next_payload:
        if next_payload == 5:  # TIMESTAMP
            if offset + 2 > len(data):
                return None
            next_payload = data[offset]
            offset += 10 if data[offset + 1] in (0, 1) else 6
        elif next_payload == 11:  # RAND
            if offset + 2 > len(data):
                return None
            next_payload = data[offset]
            offset += 2 + data[offset + 1]
        elif next_payload == 10:  # SECURITY POLICY
            if offset + 5 > len(data):
                return None
            next_payload = data[offset]
            policy_size = struct.unpack_from(">H", data, offset + 3)[0]
            offset += 5 + policy_size
        elif next_payload == 1:  # KEMAC
            if offset + 4 > len(data):
                return None
            next_payload = data[offset]
            encrypted_size = struct.unpack_from(">H", data, offset + 2)[0]
            encrypted_start = offset + 4
            encrypted_end = encrypted_start + encrypted_size
            if encrypted_end >= len(data):
                return None
            encrypted = data[encrypted_start:encrypted_end]
            if len(encrypted) < 4:
                return None
            key_size = struct.unpack_from(">H", encrypted, 2)[0]
            key_end = 4 + key_size
            if key_end > len(encrypted):
                return None
            key = encrypted[4:key_end]
            mki = b""
            if encrypted[1] & 0x0F:
                if key_end >= len(encrypted):
                    return None
                mki_size = encrypted[key_end]
                mki = encrypted[key_end + 1:key_end + 1 + mki_size]
            if encrypted[key_end + (1 if encrypted[1] & 0x0F else 0):] \
                or data[encrypted_end] != 0:
                return None
            return key, mki, ssrc
        else:
            return None
    return None


def parse_client_mikey(value):
    match = re.search(r'data="([^"]+)"', value or "")
    if not match:
        return None

    try:
        data = base64.b64decode(match.group(1), validate=True)
    except ValueError:
        return None

    if len(data) < 19:
        return None

    ssrc = struct.unpack_from(">I", data, 11)[0]
    offset = 19
    next_payload = data[2]
    key = None
    mki = b""
    policy = {}

    while offset < len(data):
        if next_payload == 0:
            break
        if next_payload == 5:
            next_payload = data[offset]
            offset += 10
        elif next_payload == 11:
            if offset + 2 > len(data):
                return None
            next_payload = data[offset]
            offset += 2 + data[offset + 1]
        elif next_payload == 10:
            if offset + 5 > len(data):
                return None
            next_payload = data[offset]
            payload_size = struct.unpack_from(">H", data, offset + 3)[0]
            parameter_offset = offset + 5
            payload_end = parameter_offset + payload_size
            if payload_end > len(data):
                return None
            while parameter_offset < payload_end:
                if parameter_offset + 2 > payload_end:
                    return None
                parameter_type = data[parameter_offset]
                parameter_size = data[parameter_offset + 1]
                parameter_offset += 2
                parameter_end = parameter_offset + parameter_size
                if parameter_end > payload_end:
                    return None
                if parameter_size == 1:
                    policy[parameter_type] = data[parameter_offset]
                parameter_offset = parameter_end
            offset = payload_end
        elif next_payload == 1:
            if offset + 4 > len(data):
                return None
            next_payload = data[offset]
            offset += 1
            key_data_size = struct.unpack_from(">H", data, offset + 1)[0]
            key_data_start = offset + 3
            key_data_end = key_data_start + key_data_size
            if key_data_end > len(data) or key_data_start + 4 > key_data_end:
                return None
            key_size = struct.unpack_from(">H", data, key_data_start + 2)[0]
            key_start = key_data_start + 4
            key_end = key_start + key_size
            if key_end > key_data_end:
                return None
            key = data[key_start:key_end]
            if data[key_data_start + 1] & 0x0F:
                if key_end >= key_data_end:
                    return None
                mki_size = data[key_end]
                mki = data[key_end + 1:key_end + 1 + mki_size]
            if key_data_end >= len(data):
                return None
            next_payload = data[key_data_end]  # KEMAC MAC (disabled in this profile).
            offset = key_data_end + 1
        else:
            return None

    if key is None:
        return None
    return {
        "key": key,
        "mki": mki,
        "ssrc": ssrc,
        "policy": policy,
    }


def is_aes128_cm_mikey(parsed, with_mki):
    if parsed is None or bool(parsed["mki"]) != with_mki or len(parsed["key"]) != 30:
        return False
    policy = parsed["policy"]
    return (
        policy.get(MIKEY_PARAM_ENCRYPTION_ALGORITHM) == MIKEY_AES_CM
        and policy.get(MIKEY_PARAM_ENCRYPTION_KEY_LENGTH) == 16
        and policy.get(MIKEY_PARAM_AUTHENTICATION_ALGORITHM) == MIKEY_AUTH_HMAC_SHA1
        and policy.get(MIKEY_PARAM_AUTHENTICATION_KEY_LENGTH) == 20
        and policy.get(MIKEY_PARAM_AUTHENTICATION_TAG_LENGTH) == 10
        and MIKEY_PARAM_AEAD_AUTHENTICATION_TAG_LENGTH not in policy
    )


def is_aes128_gcm_mikey(parsed):
    if parsed is None or not parsed["mki"] or len(parsed["key"]) != 28:
        return False
    policy = parsed["policy"]
    return (
        policy.get(MIKEY_PARAM_ENCRYPTION_ALGORITHM) == MIKEY_AES_GCM
        and policy.get(MIKEY_PARAM_ENCRYPTION_KEY_LENGTH) == 16
        and policy.get(MIKEY_PARAM_AUTHENTICATION_ALGORITHM) == MIKEY_AUTH_NULL
        and policy.get(MIKEY_PARAM_AUTHENTICATION_KEY_LENGTH) == 0
        and policy.get(MIKEY_PARAM_AEAD_AUTHENTICATION_TAG_LENGTH) == 16
        and MIKEY_PARAM_AUTHENTICATION_TAG_LENGTH not in policy
    )


class GstreamerStream:
    def __init__(self, client, video_file, rtp_socket=None, rtp_endpoint=None):
        self.client = client
        self.video_file = video_file
        self.rtp_socket = rtp_socket
        self.rtp_endpoint = rtp_endpoint
        self.pipeline = None
        self.send_lock = threading.Lock()
        # 1-based index of the RTP packet to resend verbatim right after it is sent, to make the
        # client's SRTP anti-replay window reject the duplicate. None disables this behavior.
        self.replay_packet_index = None
        self._sent_packet_count = 0

    def _send_packet(self, packet):
        if self.rtp_socket is not None:
            self.rtp_socket.sendto(packet, self.rtp_endpoint)
        else:
            frame = b"$" + bytes((0,)) + struct.pack(">H", len(packet)) + packet
            self.client.sendall(frame)

    def _on_rtp_buffer(self, sink):
        sample = sink.emit("pull-sample")
        if sample:
            buffer = sample.get_buffer()
            packet = buffer.extract_dup(0, buffer.get_size())
            with self.send_lock:
                try:
                    self._send_packet(packet)
                    self._sent_packet_count += 1
                    if self._sent_packet_count == self.replay_packet_index:
                        self._send_packet(packet)
                except OSError:
                    return Gst.FlowReturn.EOS
        return Gst.FlowReturn.OK

    def start(self, key, mki, ssrc, use_gcm):
        Gst.init(None)
        frame_pacing = "identity sleep-time=200000 ! " if self.rtp_socket else ""
        pipeline = (
            f"filesrc location={self.video_file} ! matroskademux ! h264parse config-interval=-1 ! "
            f"{frame_pacing}"
            "rtph264pay name=pay pt=96 config-interval=1 ! ")
        if key is not None:
            pipeline += "srtpenc name=encryptor ! "
        pipeline += "appsink name=rtp_sink emit-signals=true sync=false"
        self.pipeline = Gst.parse_launch(pipeline)

        self.pipeline.get_by_name("pay").set_property("ssrc", ssrc)
        if key is not None:
            encryptor = self.pipeline.get_by_name("encryptor")
            if use_gcm:
                encryptor.set_property("rtp-cipher", GST_SRTP_CIPHER_AES_128_GCM)
                encryptor.set_property("rtcp-cipher", GST_SRTP_CIPHER_AES_128_GCM)
                encryptor.set_property("rtp-auth", GST_SRTP_AUTH_NULL)
                encryptor.set_property("rtcp-auth", GST_SRTP_AUTH_NULL)
            elif len(key) == 46:
                encryptor.set_property("rtp-cipher", GST_SRTP_CIPHER_AES_256_ICM)
                encryptor.set_property("rtcp-cipher", GST_SRTP_CIPHER_AES_256_ICM)
            encryptor.set_property("key", Gst.Buffer.new_wrapped(key))
            if mki:
                encryptor.set_property("mki", Gst.Buffer.new_wrapped(mki))
        self.pipeline.get_by_name("rtp_sink").connect("new-sample", self._on_rtp_buffer)
        self.pipeline.set_state(Gst.State.PLAYING)

        bus = self.pipeline.get_bus()
        while True:
            message = bus.timed_pop_filtered(
                Gst.SECOND,
                Gst.MessageType.ERROR | Gst.MessageType.EOS)
            if message is None:
                continue
            if message.type == Gst.MessageType.ERROR:
                error, debug = message.parse_error()
                raise RuntimeError(f"GStreamer error: {error}; {debug}")
            break

    def stop(self):
        if self.pipeline is not None:
            self.pipeline.set_state(Gst.State.NULL)
            self.pipeline = None


def create_udp_socket_pair():
    for _attempt in range(100):
        rtp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        rtp_socket.bind(("127.0.0.1", 0))
        rtp_port = rtp_socket.getsockname()[1]
        if rtp_port == 65535:
            rtp_socket.close()
            continue

        rtcp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            rtcp_socket.bind(("127.0.0.1", rtp_port + 1))
            return rtp_socket, rtcp_socket
        except OSError:
            rtp_socket.close()
            rtcp_socket.close()

    raise RuntimeError("Failed to allocate an RTP/RTCP UDP socket pair")


def parse_client_ports(transport):
    match = re.search(r"(?:^|;)\s*client_port=(\d+)(?:-(\d+))?", transport)
    if not match:
        return None
    rtp_port = int(match.group(1))
    return rtp_port, int(match.group(2)) if match.group(2) else rtp_port + 1


def receiver_report_ssrcs(data):
    result = []
    offset = 0
    while offset + 4 <= len(data):
        packet_size = (struct.unpack_from(">H", data, offset + 2)[0] + 1) * 4
        if packet_size < 4 or offset + packet_size > len(data):
            return []
        if data[offset + 1] == 201 and packet_size >= 8:
            result.append(struct.unpack_from(">I", data, offset + 4)[0])
        offset += packet_size
    return result


class SrtcpSession:
    def __init__(
        self,
        server_key,
        client_key,
        server_ssrc,
        client_ssrc,
        rtcp_socket,
        client_endpoint,
    ):
        self.server_ssrc = server_ssrc
        self.client_ssrc = client_ssrc
        self.rtcp_socket = rtcp_socket
        self.client_endpoint = client_endpoint
        self.stop_event = threading.Event()
        self.report_received = threading.Event()
        self.error = None
        self.threads = []
        self.encoder = self._create_encoder(server_key)
        self.decoder = self._create_decoder(client_key)

    @staticmethod
    def _create_encoder(key):
        pipeline = Gst.parse_launch(
            "srtpenc name=encryptor "
            "appsrc name=source is-live=true format=time caps=application/x-rtcp "
            "! encryptor.rtcp_sink_0 "
            "encryptor.rtcp_src_0 ! appsink name=sink sync=false async=false")
        pipeline.get_by_name("encryptor").set_property("key", Gst.Buffer.new_wrapped(key))
        pipeline.set_state(Gst.State.PLAYING)
        return pipeline

    @staticmethod
    def _create_decoder(key):
        crypto_caps = Gst.Caps.from_string(
            "application/x-srtp, "
            f"srtp-key=(buffer){key.hex()}, "
            "srtp-cipher=(string)aes-128-icm, "
            "srtp-auth=(string)hmac-sha1-80, "
            "srtcp-cipher=(string)aes-128-icm, "
            "srtcp-auth=(string)hmac-sha1-80")
        pipeline = Gst.parse_launch(
            "srtpdec name=decryptor "
            "appsrc name=source is-live=true format=time caps=application/x-srtcp "
            "! decryptor.rtcp_sink "
            "decryptor.rtcp_src ! appsink name=sink sync=false async=false")
        pipeline.get_by_name("decryptor").connect(
            "request-key", lambda _decryptor, _ssrc: crypto_caps)
        pipeline.set_state(Gst.State.PLAYING)
        return pipeline

    @staticmethod
    def _transform(pipeline, data, timeout):
        buffer = Gst.Buffer.new_wrapped(data)
        if pipeline.get_by_name("source").emit("push-buffer", buffer) != Gst.FlowReturn.OK:
            raise RuntimeError("Failed to push an SRTCP buffer into GStreamer")
        sample = pipeline.get_by_name("sink").emit("try-pull-sample", timeout)
        if sample is None:
            return None
        output = sample.get_buffer()
        return output.extract_dup(0, output.get_size())

    def _sender_report(self):
        unix_nanoseconds = time.time_ns()
        ntp_seconds = unix_nanoseconds // 1_000_000_000 + 2_208_988_800
        ntp_fraction = ((unix_nanoseconds % 1_000_000_000) << 32) // 1_000_000_000
        return struct.pack(
            ">BBHIIIIII",
            0x80,
            200,
            6,
            self.server_ssrc,
            ntp_seconds,
            ntp_fraction,
            0,
            0,
            0)

    def _send_reports(self):
        try:
            while not self.stop_event.is_set() and not self.report_received.is_set():
                packet = self._transform(self.encoder, self._sender_report(), Gst.SECOND)
                if packet is None:
                    raise RuntimeError("GStreamer did not encrypt an SRTCP Sender Report")
                self.rtcp_socket.sendto(packet, self.client_endpoint)
                self.report_received.wait(0.5)
        except Exception as error:  # pylint: disable=broad-exception-caught
            self.error = error
            self.stop_event.set()

    def _receive_reports(self):
        self.rtcp_socket.settimeout(0.2)
        try:
            while not self.stop_event.is_set() and not self.report_received.is_set():
                try:
                    packet, _sender = self.rtcp_socket.recvfrom(2048)
                except TimeoutError:
                    continue
                if len(packet) < 8:
                    continue
                plain_packet = self._transform(self.decoder, packet, 100 * Gst.MSECOND)
                if plain_packet is None:
                    continue
                report_ssrcs = receiver_report_ssrcs(plain_packet)
                if self.client_ssrc in report_ssrcs:
                    print(
                        f"Received SRTCP Receiver Report with SSRC {self.client_ssrc:08x}",
                        flush=True)
                    self.report_received.set()
                elif report_ssrcs:
                    actual = ", ".join(f"{ssrc:08x}" for ssrc in report_ssrcs)
                    raise RuntimeError(
                        f"Unexpected SRTCP Receiver Report SSRC: {actual}; "
                        f"expected {self.client_ssrc:08x}")
        except Exception as error:  # pylint: disable=broad-exception-caught
            self.error = error
            self.stop_event.set()

    def start(self):
        self.threads = [
            threading.Thread(target=self._send_reports, daemon=True),
            threading.Thread(target=self._receive_reports, daemon=True),
        ]
        for thread in self.threads:
            thread.start()

    def stop(self):
        self.stop_event.set()
        for thread in self.threads:
            thread.join()
        self.encoder.set_state(Gst.State.NULL)
        self.decoder.set_state(Gst.State.NULL)

    def validate(self):
        if self.error:
            raise RuntimeError("SRTCP exchange failed") from self.error
        if not self.report_received.is_set():
            raise RuntimeError("Server did not receive an SRTCP Receiver Report")


def header(request, name):
    prefix = name.lower() + ":"
    for line in request.split("\r\n"):
        if line.lower().startswith(prefix):
            return line[len(prefix):].strip()
    return ""


def response(code, reason, cseq, extra="", body=b""):
    headers = [f"RTSP/1.0 {code} {reason}", f"CSeq: {cseq}"]
    if extra:
        headers.extend(extra.rstrip("\r\n").split("\r\n"))
    if body:
        headers.extend(["Content-Type: application/sdp", f"Content-Length: {len(body)}"])
    return ("\r\n".join(headers) + "\r\n\r\n").encode() + body


def read_request(client):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = client.recv(4096)
        if not chunk:
            return None
        data += chunk
    end = data.index(b"\r\n\r\n") + 4
    request = data[:end].decode("latin1")
    length = int(header(request, "Content-Length") or 0)
    while len(data) - end < length:
        chunk = client.recv(4096)
        if not chunk:
            return None
        data += chunk
    return request


def serve(client, mode, video_file):
    client.settimeout(30)
    stream = None
    udp_sockets = None
    srtcp_session = None
    srtcp_started = False
    session = "12345678"
    if mode == "savp_axis_unparseable":
        server_mikey = "unsupported"
    elif mode == "savp_media_level_aes256_cm":
        server_mikey = AES256_CM_MIKEY
    else:
        server_mikey = AXIS_MIKEY
    server_context = parse_mikey_payload(server_mikey)
    setup_attempt = 0
    play_attempt = 0
    teardown_session = None

    def validate_scenario():
        if mode == "avp_second_track_transport_rejected":
            if setup_attempt != 2:
                raise RuntimeError(
                    f"Expected two SETUP requests, received {setup_attempt}")
            # The session created by the first track SETUP must be released.
            if teardown_session != session:
                raise RuntimeError(
                    f"Expected TEARDOWN of session {session}, received {teardown_session}")
        if mode == "avp_setup_service_unavailable":
            if setup_attempt != 1:
                raise RuntimeError(
                    f"Expected one SETUP request, received {setup_attempt}")
            # No session has been created, so there is nothing to tear down.
            if teardown_session is not None:
                raise RuntimeError("Unexpected TEARDOWN without a session")
        if mode == "avp_play_rejected":
            if setup_attempt != 1 or play_attempt != 1:
                raise RuntimeError(
                    f"Expected one SETUP and one PLAY request, received {setup_attempt} "
                    f"and {play_attempt}")
            # The session created by SETUP must be released after the rejected PLAY.
            if teardown_session != session:
                raise RuntimeError(
                    f"Expected TEARDOWN of session {session}, received {teardown_session}")

    try:
        while True:
            request = read_request(client)
            if request is None:
                validate_scenario()
                return
            lines = request.split("\r\n")
            method, uri, _ = lines[0].split(" ", 2)
            cseq = header(request, "CSeq")

            if method == "OPTIONS":
                client.sendall(response(
                    200,
                    "OK",
                    cseq,
                    "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN"))
            elif method == "DESCRIBE":
                if mode == "avp_second_track_transport_rejected":
                    body = (
                        "v=0\r\n"
                        "o=- 0 0 IN IP4 127.0.0.1\r\n"
                        "s=Transport selection functional test\r\n"
                        "t=0 0\r\n"
                        f"a=control:{uri}/\r\n"
                        "m=video 0 RTP/AVP 96\r\n"
                        "c=IN IP4 0.0.0.0\r\n"
                        "a=recvonly\r\n"
                        "a=control:trackID=1\r\n"
                        "a=rtpmap:96 H264/90000\r\n"
                        "m=video 0 RTP/AVP 97\r\n"
                        "c=IN IP4 0.0.0.0\r\n"
                        "a=recvonly\r\n"
                        "a=control:trackID=2\r\n"
                        "a=rtpmap:97 H264/90000\r\n"
                    ).encode()
                    client.sendall(response(200, "OK", cseq, f"Content-Base: {uri}\r\n", body))
                    continue
                if mode in ("avp_setup_service_unavailable", "avp_play_rejected"):
                    body = (
                        "v=0\r\n"
                        "o=- 0 0 IN IP4 127.0.0.1\r\n"
                        "s=Stream opening failure functional test\r\n"
                        "t=0 0\r\n"
                        f"a=control:{uri}/\r\n"
                        "m=video 0 RTP/AVP 96\r\n"
                        "c=IN IP4 0.0.0.0\r\n"
                        "a=recvonly\r\n"
                        "a=control:trackID=1\r\n"
                        "a=rtpmap:96 H264/90000\r\n"
                    ).encode()
                    client.sendall(response(200, "OK", cseq, f"Content-Base: {uri}\r\n", body))
                    continue
                stream_name = "AES-256-CM" if mode == "savp_media_level_aes256_cm" else "Axis"
                session_key_management = (
                    "" if mode == "savp_media_level_aes256_cm"
                    else f"a=key-mgmt:mikey {server_mikey}\r\n")
                media_key_management = (
                    f"a=key-mgmt:mikey {server_mikey}\r\n"
                    if mode == "savp_media_level_aes256_cm" else "")
                body = (
                    f"v=0\r\n"
                    f"o=- 0 0 IN IP4 127.0.0.1\r\n"
                    f"s={stream_name} functional test\r\n"
                    "t=0 0\r\n"
                    f"a=control:{uri}/\r\n"
                    f"{session_key_management}"
                    f"m=video 0 RTP/SAVP 96\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=recvonly\r\n"
                    "a=control:trackID=1\r\n"
                    f"{media_key_management}"
                    "a=rtpmap:96 H264/90000\r\n"
                    "a=fmtp:96 packetization-mode=1;profile-level-id=640028;"
                    "sprop-parameter-sets="
                    "Z2QAKK2EBUViuKxUdCAqKxXFYqOhAVFYrisVHQgKisVxWKjoQFRWK4rFR0ICorFcVio6ECSF"
                    "ITk8nyfk/k/J8nm5s00IEkKQnJ5Pk/J/J+T5PNzZprQFAW0qQAAAAwCAAAAPGBAAD0JAAAiVQ"
                    "ve+F4RCNQAAAAE=,aO48sA==\r\n"
                ).encode()
                client.sendall(response(200, "OK", cseq, f"Content-Base: {uri}\r\n", body))
            elif method == "SETUP":
                use_gcm = False
                if mode == "avp_setup_service_unavailable":
                    # The cameras from VMS-63226 reject SETUP this way.
                    setup_attempt += 1
                    client.sendall(response(503, "Service Unavailable", cseq))
                    continue
                if mode == "avp_play_rejected":
                    setup_attempt += 1
                    client.sendall(response(
                        200,
                        "OK",
                        cseq,
                        f"Session: {session};timeout=60\r\n"
                        "Transport: RTP/AVP/TCP;unicast;interleaved=0-1"))
                    continue
                if mode == "avp_second_track_transport_rejected":
                    setup_attempt += 1
                    transport = header(request, "Transport")
                    if setup_attempt == 1:
                        if "trackID=1" not in uri or "RTP/AVP/TCP" not in transport:
                            raise RuntimeError("Expected TCP SETUP for the first track")
                        client.sendall(response(
                            200,
                            "OK",
                            cseq,
                            f"Session: {session};timeout=60\r\n"
                            "Transport: RTP/AVP/TCP;unicast;interleaved=0-1"))
                        continue
                    if setup_attempt == 2:
                        if ("trackID=2" not in uri
                            or "RTP/AVP/TCP" not in transport
                            or header(request, "Session") != session):
                            raise RuntimeError("Expected TCP SETUP for the second track")
                        client.sendall(response(461, "Unsupported Transport", cseq))
                        continue
                    raise RuntimeError("Unexpected transport fallback after the first track")
                elif mode == "savp":
                    parsed = parse_client_mikey(header(request, "KeyMgmt"))
                    client_ports = parse_client_ports(header(request, "Transport"))
                    if (server_context is None
                        or not is_aes128_cm_mikey(parsed, with_mki=False)
                        or client_ports is None):
                        client.sendall(response(463, "Key management failure", cseq))
                        continue
                    server_key, server_mki, media_ssrc = server_context
                    if server_mki:
                        raise RuntimeError("Expected standard server MIKEY without MKI")
                    udp_sockets = create_udp_socket_pair()
                    server_rtp_port = udp_sockets[0].getsockname()[1]
                    server_rtcp_port = udp_sockets[1].getsockname()[1]
                    client_address = client.getpeername()[0]
                    stream = GstreamerStream(
                        client,
                        video_file,
                        udp_sockets[0],
                        (client_address, client_ports[0]))
                    stream.start_args = (server_key, server_mki, media_ssrc, False)
                    srtcp_session = SrtcpSession(
                        server_key,
                        parsed["key"],
                        media_ssrc,
                        parsed["ssrc"],
                        udp_sockets[1],
                        (client_address, client_ports[1]))
                    transport = (
                        f"Session: {session};timeout=60\r\n"
                        f"Transport: RTP/SAVP;unicast;"
                        f"client_port={client_ports[0]}-{client_ports[1]};"
                        f"server_port={server_rtp_port}-{server_rtcp_port};"
                        f"ssrc={media_ssrc:08x};mode=\"PLAY\"")
                    client.sendall(response(200, "OK", cseq, transport))
                    continue
                elif mode == "savp_media_level_aes256_cm":
                    parsed = parse_client_mikey(header(request, "KeyMgmt"))
                    if (server_context is None
                        or not is_aes128_cm_mikey(parsed, with_mki=False)):
                        client.sendall(response(463, "Key management failure", cseq))
                        continue
                    key, mki, media_ssrc = server_context
                else:
                    parsed = parse_client_mikey(header(request, "KeyMgmt"))
                    setup_attempt += 1
                    if mode == "savp_axis_gcm" and setup_attempt == 1:
                        if not is_aes128_cm_mikey(parsed, with_mki=False):
                            raise RuntimeError("Expected standard AES-CM MIKEY on first SETUP")
                        client.sendall(response(463, "Key management failure", cseq))
                        continue
                    if mode == "savp_axis_gcm" and setup_attempt == 2:
                        if not is_aes128_cm_mikey(parsed, with_mki=True):
                            raise RuntimeError(
                                "Expected client-managed AES-CM MIKEY on second SETUP")
                        client.sendall(response(463, "Key management failure", cseq))
                        continue
                    if mode == "savp_axis_gcm":
                        if setup_attempt != 3 or not is_aes128_gcm_mikey(parsed):
                            client.sendall(response(463, "Key management failure", cseq))
                            continue
                        use_gcm = True
                    elif mode in ("savp_axis", "savp_axis_replayed_packet"):
                        if setup_attempt == 1:
                            if not is_aes128_cm_mikey(parsed, with_mki=False):
                                raise RuntimeError(
                                    "Expected standard AES-CM MIKEY on first SETUP")
                            client.sendall(response(463, "Key management failure", cseq))
                            continue
                        if setup_attempt != 2 or not is_aes128_cm_mikey(
                            parsed, with_mki=True
                        ):
                            raise RuntimeError(
                                "Expected client-managed AES-CM MIKEY on second SETUP")
                    elif setup_attempt != 1 or not is_aes128_cm_mikey(parsed, with_mki=True):
                        raise RuntimeError("Expected client-managed AES-CM MIKEY on first SETUP")
                    key = parsed["key"]
                    mki = parsed["mki"]
                    media_ssrc = 0x12345678
                stream = GstreamerStream(client, video_file)
                if mode == "savp_axis_replayed_packet":
                    stream.replay_packet_index = 5
                stream.start_args = (key, mki, media_ssrc, use_gcm)
                transport = (
                    f"Session: {session};timeout=60\r\n"
                    f"Transport: RTP/SAVP/TCP;unicast;interleaved=0-1;"
                    f"ssrc={media_ssrc:08x};mode=\"PLAY\"")
                client.sendall(response(
                    200, "OK", cseq, transport))
            elif method == "PLAY":
                if mode == "avp_play_rejected":
                    play_attempt += 1
                    client.sendall(response(503, "Service Unavailable", cseq))
                    continue
                client.sendall(response(
                    200, "OK", cseq,
                    f"Session: {session}\r\nRTP-Info: url={uri};seq=0;rtptime=0"))
                if srtcp_session is not None:
                    srtcp_session.start()
                    srtcp_started = True
                if stream is not None:
                    stream.start(*stream.start_args)
            elif method == "TEARDOWN":
                teardown_session = header(request, "Session")
                try:
                    client.sendall(response(200, "OK", cseq, f"Session: {session}"))
                except OSError:
                    pass
                validate_scenario()
                return
            else:
                client.sendall(response(501, "Not Implemented", cseq))
    finally:
        if stream is not None:
            stream.stop()
        if srtcp_session is not None:
            srtcp_session.stop()
        if udp_sockets is not None:
            for udp_socket in udp_sockets:
                udp_socket.close()
        if srtcp_started:
            srtcp_session.validate()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--ready-file", required=True)
    parser.add_argument(
        "--mode",
        choices=(
            "savp_axis",
            "savp_axis_gcm",
            "savp_axis_unparseable",
            "savp_axis_replayed_packet",
            "savp_media_level_aes256_cm",
            "savp",
            "avp_second_track_transport_rejected",
            "avp_setup_service_unavailable",
            "avp_play_rejected"),
        default="savp_axis")
    parser.add_argument("--rtp-transport", choices=("tcp", "udp"), required=True)
    parser.add_argument("--video-file", required=True)
    args = parser.parse_args()

    if (args.mode == "savp") != (args.rtp_transport == "udp"):
        parser.error("savp mode requires UDP; other modes require TCP")

    Gst.init(None)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", args.port))
    server.listen(1)
    with open(args.ready_file, "w", encoding="ascii") as ready:
        ready.write("ready\n")
    client, _ = server.accept()
    with client:
        serve(client, args.mode, args.video_file)
    server.close()


if __name__ == "__main__":
    main()
