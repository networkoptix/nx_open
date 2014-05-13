update vms_resource set guid = id
where guid = "";

DELETE FROM vms_businessrule_action_resources where resource_id in (SELECT id from vms_resource where disabled > 0);
DELETE FROM vms_businessrule_event_resources where resource_id in (SELECT id from vms_resource where disabled > 0);
DELETE FROM vms_scheduletask WHERE source_id in (SELECT id from vms_resource where disabled > 0);
DELETE FROM vms_camera WHERE resource_ptr_id in (SELECT id from vms_resource where disabled > 0);
DELETE FROM vms_kvpair WHERE resource_id in (SELECT id from vms_resource where disabled > 0);
DELETE FROM vms_resource WHERE disabled > 0;

ALTER TABLE "vms_resource" RENAME TO vms_resource_tmp;

CREATE TABLE "vms_resource" (id INTEGER PRIMARY KEY AUTOINCREMENT,
                             guid BLOB(16) NULL UNIQUE,
			     parent_guid BLOB(16),
                             status SMALLINT NOT NULL, 
                             name VARCHAR(200) NOT NULL, 
			     url VARCHAR(200), 
			     xtype_guid BLOB(16));
INSERT INTO "vms_resource" (id, status,name,url) SELECT id, status,name,url FROM vms_resource_tmp;

ALTER TABLE "vms_businessrule" ADD guid BLOB(16);
ALTER TABLE "vms_resourcetype" ADD guid BLOB(16);

ALTER TABLE "vms_businessrule_action_resources" RENAME TO vms_businessrule_action_resources_tmp;
CREATE TABLE "vms_businessrule_action_resources" (businessrule_guid BLOB(16) NOT NULL, resource_guid BLOB(16) NOT NULL, PRIMARY KEY(businessrule_guid, resource_guid));

ALTER TABLE "vms_businessrule_event_resources" RENAME TO vms_businessrule_event_resources_tmp;
CREATE TABLE "vms_businessrule_event_resources" (businessrule_guid BLOB(16) NOT NULL, resource_guid BLOB(16) NOT NULL, PRIMARY KEY(businessrule_guid, resource_guid));


CREATE TABLE "transaction_log" (peer_guid   BLOB(16) NOT NULL,
			        sequence    INTEGER NOT NULL,
				timestamp   INTEGER NOT NULL,
			        tran_guid   BLOB(16) NOT NULL,
			        tran_data   BLOB  NOT NULL);

CREATE UNIQUE INDEX idx_transaction_key   ON transaction_log(peer_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_hash  ON transaction_log(tran_guid);
CREATE INDEX idx_transaction_time  ON transaction_log(timestamp);

CREATE TABLE "vms_storedFiles" (path VARCHAR PRIMARY KEY, data BLOB);
CREATE UNIQUE INDEX idx_kvpair_name ON vms_kvpair (resource_id, name);
ALTER TABLE vms_kvpair ADD isResTypeParam BOOL;
