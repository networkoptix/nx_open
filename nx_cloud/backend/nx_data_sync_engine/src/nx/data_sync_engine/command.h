#pragma once

#include <transaction/transaction.h>
#include <transaction/transaction_transport_header.h>

namespace nx {
namespace data_sync_engine {

using CommandHeader = ::ec2::QnAbstractTransaction;

template<typename Data>
using Command = ::ec2::QnTransaction<Data>;

using CommandTransportHeader = ::ec2::QnTransactionTransportHeader;

} // namespace data_sync_engine
} // namespace nx
