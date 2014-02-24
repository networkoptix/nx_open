INSERT INTO "vms_businessrule_action_resources" (businessrule_guid,resource_guid) 
  SELECT br.guid, r.guid
  FROM vms_businessrule_action_resources_tmp t
  JOIN vms_businessrule br on br.id = t.businessrule_id
  JOIN vms_resource r on r.id = t.resource_id;

INSERT INTO "vms_businessrule_event_resources" (businessrule_guid,resource_guid) 
  SELECT br.guid, r.guid
  FROM vms_businessrule_event_resources_tmp t
  JOIN vms_businessrule br on br.id = t.businessrule_id
  JOIN vms_resource r on r.id = t.resource_id;

drop table vms_resource_tmp;
drop table vms_businessrule_action_resources_tmp;
drop table vms_businessrule_event_resources_tmp;

CREATE UNIQUE INDEX idx_businessrule_guid ON vms_businessrule(guid);
CREATE UNIQUE INDEX idx_resourcetype_guid ON vms_resourcetype(guid);

ALTER TABLE "vms_server" ADD flags number;

CREATE TABLE "vms_transaction_log" (id              INTEGER PRIMARY KEY AUTOINCREMENT,
                             	   peer_guid        BLOB(16) NOT NULL,
				   transaction_guid BLOB(16) NOT NULL,
			           transaction_data BLOB  NOT NULL);

CREATE  INDEX idx_transaction_guid      ON vms_transaction_log(transaction_guid);
CREATE  INDEX idx_transaction_peer_guid ON vms_transaction_log(peer_guid);
