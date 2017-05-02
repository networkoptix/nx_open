CREATE TABLE local_resource_properties
(
	id INTEGER NOT NULL PRIMARY KEY,
    resource_id BLOB(16) NOT NULL,
	property_name TEXT NOT NULL,
	property_value TEXT
);