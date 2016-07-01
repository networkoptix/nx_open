
DROP TABLE transaction_log;

CREATE TABLE "transaction_log" (
                peer_guid   	BLOB(16) NOT NULL,
                db_guid     	BLOB(16) NOT NULL,
                sequence    	INTEGER NOT NULL,
                timestamp   	INTEGER NOT NULL,
                tran_guid   	BLOB(16) NOT NULL,
                tran_data   	BLOB  NOT NULL);

CREATE UNIQUE INDEX idx_transaction_key   ON transaction_log(peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_hash  ON transaction_log(tran_guid);
CREATE INDEX idx_transaction_time  ON transaction_log(timestamp);