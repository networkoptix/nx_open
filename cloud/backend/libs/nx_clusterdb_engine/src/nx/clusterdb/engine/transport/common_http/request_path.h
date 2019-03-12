#pragma once

namespace nx::clusterdb::engine::transport {

static constexpr char kEstablishEc2TransactionConnectionPath[] = "/events/ConnectingStage1";
static constexpr char kdeprecatedEstablishEc2P2pTransactionConnectionPath[] = "/transactionBus";
static constexpr char kEstablishEc2P2pTransactionConnectionPath[] = "/transactionBus/websocket";
static constexpr char kPushEc2TransactionPath[] = "/forward_events/{sequence}";
// TODO: #ak Refactor this constant out.
static constexpr char kPushEc2TransactionPathWithoutSequence[] = "/forward_events";

} // namespace nx::clusterdb::engine::transport
