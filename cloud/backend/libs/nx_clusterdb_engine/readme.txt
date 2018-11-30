# Cluster DB engine.

## Synchronizes application-defined "commands" in a cluster of nodes.

Each node contains the whole copy of DB, so every data retrieval operation is processed locally.
Every data modification is recorded locally and reported to other nodes ASAP.
If current node cannot be connected to others at the moment it is still fully operational.
So, user does not see the difference between online/offline nodes.
Data modifications made in offline node are distributed when connection to other node(s) is restored.


## Uses SQL DB as a persistent storage.

Currently MySQL and SQLite are supported. But, due to "simple" SQL syntax requirements, 
other RDBMS types can be added easily.
Different nodes can have a different RDBMS type.


## Supports many DB instances in a single node.

E.g., let us have multiple user DBs out there. Each DB has the same schema but different data.
Each user's DB is served by a node (or interconnected group of nodes). At the same time, there can 
be a "cloud node" that is connected to every user node. Cloud node contains data of all user DBs. 
But, at the same time, restricts user's access only to appropriate data.


## Current usages
- By cloud_db to synchronize data with mediaservers.
- By nx_clusterdb_map to provide distributed map.
