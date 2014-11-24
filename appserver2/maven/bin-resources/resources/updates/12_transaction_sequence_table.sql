CREATE TABLE "transaction_sequence" (
                peer_guid   BLOB(16) NOT NULL,
                db_guid     BLOB(16) NOT NULL,
                sequence    INTEGER NOT NULL);

CREATE UNIQUE INDEX idx_tran_sequence_key   ON transaction_sequence(peer_guid, db_guid);
