"""Transaction log utilities

Classes and utilities to work with transaction log JSON structure returned by server
"""

import json
from functools import total_ordering


@total_ordering
class Timestamp(object):

    def __init__(self, sequence, ticks):
        self.sequence = sequence
        self.ticks = ticks

    def __eq__(self, other):
        return ((self.sequence, self.ticks) ==
                (other.sequence, other.ticks))

    def __lt__(self, other):
        return ((self.sequence, self.ticks) <
                (other.sequence, other.ticks))

    def __str__(self):
        return "%s.%s" % (self.sequence, self.ticks)

    def __repr__(self):
        return '%s' % self


@total_ordering
class Transaction(object):

    @classmethod
    def from_dict(cls, d):
        return Transaction(
            command=d['tran']['command'],
            author=d['tran']['historyAttributes']['author'],
            peer_id=d['tran']['peerID'],
            db_id=d['tran']['persistentInfo']['dbID'],
            sequence=d['tran']['persistentInfo']['sequence'],
            timestamp=Timestamp(
                sequence=d['tran']['persistentInfo']['timestamp']['sequence'],
                ticks=d['tran']['persistentInfo']['timestamp']['ticks']),
            transaction_type=d['tran']['transactionType'],
            transaction_guid=d['tranGuid'])

    def __init__(self, command, author, peer_id, db_id, sequence, timestamp,
                 transaction_type, transaction_guid):
        self.command = command
        self.author = author
        self.peer_id = peer_id
        self.db_id = db_id
        self.sequence = sequence
        self.timestamp = timestamp
        self.transaction_type = transaction_type
        self.transaction_guid = transaction_guid

    def __hash__(self):
        return hash((self.peer_id, self.db_id, self.sequence))

    def __eq__(self, other):
        return ((self.peer_id, self.db_id, self.sequence) ==
                (other.peer_id, other.db_id, other.sequence))

    def __lt__(self, other):
        return ((self.peer_id, self.db_id, self.sequence) <
                (other.peer_id, other.db_id, other.sequence))

    def __str__(self):
        return ('Transaction(peer_id=%s db_id=%s sequence=%s timestamp=%s command=%s)' %
                    (self.peer_id, self.db_id, self.sequence, self.timestamp, self.command))

    def __repr__(self):
        return '%s' % self

    # trying to keep same structure as from_json, but with only those fields we compare by
    def to_dict(self):
        return dict(
            tran=dict(
                peerID=self.peer_id,
                persistentInfo=dict(
                    dbID=self.db_id,
                    sequence=self.sequence)))


class TransactionJsonEncoder(json.JSONEncoder):

    def default(self, obj):
        if isinstance(obj, Transaction):
            return obj.to_dict()
        return json.JSONEncoder.default(self, obj)


def transactions_from_json(json_data):
    return sorted(map(Transaction.from_dict, json_data))
