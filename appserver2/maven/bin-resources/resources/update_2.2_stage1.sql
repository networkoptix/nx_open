update vms_resource set guid = id
where guid = "";

ALTER TABLE "vms_resource" RENAME TO vms_resource_tmp;

CREATE TABLE "vms_resource" (id INTEGER PRIMARY KEY AUTOINCREMENT,
                             guid BLOB(16) NULL UNIQUE,
			     parent_guid BLOB(16),
                             status SMALLINT NOT NULL, 
			     disabled BOOL NOT NULL DEFAULT 0, 
                             name VARCHAR(200) NOT NULL, 
			     url VARCHAR(200), 
			     xtype_guid BLOB(16));
INSERT INTO "vms_resource" (id, status,disabled,name,url) SELECT id, status,disabled,name,url FROM vms_resource_tmp;

ALTER TABLE "vms_businessrule" ADD guid BLOB(16);
ALTER TABLE "vms_resourcetype" ADD guid BLOB(16);

ALTER TABLE "vms_businessrule_action_resources" RENAME TO vms_businessrule_action_resources_tmp;
CREATE TABLE "vms_businessrule_action_resources" (businessrule_guid BLOB(16) NOT NULL, resource_guid BLOB(16) NOT NULL, PRIMARY KEY(businessrule_guid, resource_guid));

ALTER TABLE "vms_businessrule_event_resources" RENAME TO vms_businessrule_event_resources_tmp;
CREATE TABLE "vms_businessrule_event_resources" (businessrule_guid BLOB(16) NOT NULL, resource_guid BLOB(16) NOT NULL, PRIMARY KEY(businessrule_guid, resource_guid));


CREATE TABLE "transaction_log" (peer_guid   BLOB(16) NOT NULL,
			        sequence    INTEGER NOT NULL,
			        tran_guid   BLOB(16) NOT NULL,
			        tran_data   BLOB  NOT NULL);

CREATE UNIQUE INDEX idx_transaction_key   ON transaction_log(peer_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_hash  ON transaction_log(peer_guid, tran_guid);
