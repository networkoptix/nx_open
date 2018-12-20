Abstract.
nx_sql library provides a way to execute SQL queries asynchronously over DB connection pool.

Features.
- connection pool.
  - unused connections are closed by timeout
  - broken connections are restored automatically to keep connection count as required
- asynchronous query execution. Query result is delivered to completion handler
- lookup / modify data query prioritization over other queries.
- optional and controlled aggregation of multiple queries inside a single transaction (e.g., speeds up inserting to SQLITE a lot)
