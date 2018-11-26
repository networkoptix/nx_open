# Distributed embedded key/value database.

Each node stores copy of the whole DB, so every data access operation is processed locally.
Even after loosing connection to every other node, DB stays fully operational.
After restoring connection data will synchronized and merged, if needed.

Based on nx_clusterdb_engine.
Can use any backend supported by nx_clusterdb_engine (e.g., MySQL, SQLite).
