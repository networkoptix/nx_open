// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/metrics/base_metrics_storage.h>

namespace nx {
namespace metrics {

struct Storage: ParameterSet
{
    struct TcpConnections: ParameterSet
    {
        NX_METRICS_ADD(std::atomic_int, outgoing,
            "Total amount of opened outgoing TCP connections with any type");
        NX_METRICS_ADD(std::atomic_int, total,
            "Total amount of opened incoming TCP connections with any type");
        NX_METRICS_ADD(std::atomic_int, rtsp,
            "Amount of opened RTSP connections");
        NX_METRICS_ADD(std::atomic_int, hls,
            "Amount of opened HLS connections");
        NX_METRICS_ADD(std::atomic_int, progressiveDownloading,
            "Amount of opened progressive downloading connections");
        NX_METRICS_ADD(std::atomic_int, p2p,
            "Amount of opened p2p connections");
        NX_METRICS_ADD(std::atomic<qint64>, totalBytesSent, "Total tcp bytes sent");
    };
    NX_METRICS_ADD(TcpConnections, tcpConnections, "Opened TCP connections");

    NX_METRICS_ADD(std::atomic_int, transcoders,
        "Amount of video transcoding threads (same as amount of encoders)");
    NX_METRICS_ADD(std::atomic_int, progressiveDownloadingTranscoders,
        "Amount of opened progressive downloading connections with transcoding");
    NX_METRICS_ADD(std::atomic_int, decoders, "Amount of video decoders");
    NX_METRICS_ADD(std::atomic<qint64>, encodedPixels, "Amount of encoded video pixels");
    NX_METRICS_ADD(std::atomic<qint64>, decodedPixels, "Amount of decoded video pixels");
    NX_METRICS_ADD(std::atomic_int, offlineStatus,
        "How many times resources have switched to the offline state");
    NX_METRICS_ADD(std::atomic<qint64>, ruleActions, "The number if executed rules actions");
    NX_METRICS_ADD(std::atomic<qint64>, thumbnails, "Amount of requested thumbnails");
    NX_METRICS_ADD(std::atomic<qint64>, apiCalls, "Amount of requested API calls");
    NX_METRICS_ADD(std::atomic<qint64>, primaryStreams, "Amount of primary streams");
    NX_METRICS_ADD(std::atomic<qint64>, secondaryStreams, "Amount of secondary streams");

    struct Transactions: ParameterSet
    {
        NX_METRICS_ADD(std::atomic_int, errors,
            "Amount of transactions that can't be written to DB due to SQL error");
        NX_METRICS_ADD(std::atomic_int, success,
            "Total amount of transactions successfully written.");
        NX_METRICS_ADD(std::atomic_int, local,
            "Total amount of local transactions written. Local transactions are written "
            "to the DB but not synchronized to another servers. 'Local' always <= 'success'");
        NX_METRICS_ADD(std::atomic<qint64>, logSize, "Total size of transaction log in bytes");
    };
    NX_METRICS_ADD(Transactions, transactions, "Database transactions statistics");

    struct P2pStatisticsData: ParameterSet
    {
        using DataSentByMessageType = QMap<QString, qint64>;
        NX_METRICS_ADD(DataSentByMessageType, dataSentByMessageType, "Amount of sent data in bytes by p2p message type");
    };
    NX_METRICS_ADD(P2pStatisticsData, p2pCounters, "P2p statistics");
};

} // namespace metrics
} // namespace nx
