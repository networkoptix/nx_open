INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','6','2','10020','9' );
INSERT INTO "vms_businessrule_action_resources" ( "businessrule_id","resource_id" ) VALUES ( '10020','1' );

INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','7','2','10021','10' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','6','2','10022','10' );
INSERT INTO "vms_businessrule_action_resources" ( "businessrule_id","resource_id" ) VALUES ( '10022','1' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'0',NULL,'0','8','2','10023','10' );

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
                             name VARCHAR(200) NOT NULL, 
                 	     url VARCHAR(200), 
                 	    xtype_guid BLOB(16));
INSERT INTO "vms_resource" (id, name,url) SELECT id, name,url FROM vms_resource_tmp;

CREATE TABLE vms_resource_status (
	guid guid BLOB(16) NOT NULL UNIQUE, 
        status SMALLINT NOT NULL
);

ALTER TABLE "vms_layoutitem" RENAME TO vms_layoutitem_tmp;
CREATE TABLE "vms_layoutitem" (
    "zoom_bottom" real NOT NULL DEFAULT 0,
    "right" real NOT NULL,
    "uuid" varchar(40) NOT NULL,
    "zoom_left" real NOT NULL DEFAULT 0,
    "resource_guid" BLOB(16) NOT NULL,
    "zoom_right" real NOT NULL DEFAULT 0,
    "top" real NOT NULL,
    "layout_id" integer NOT NULL,
    "bottom" real NOT NULL,
    "zoom_top" real NOT NULL DEFAULT 0,
    "zoom_target_uuid" varchar(40),
    "flags" integer NOT NULL,
    "contrast_params" varchar(200),
    "rotation" real NOT NULL,
    "id" integer PRIMARY KEY autoincrement,
    "dewarping_params" varchar(200) NULL,
    "left" real NOT NULL);
INSERT INTO vms_layoutitem SELECT * from vms_layoutitem_tmp;
                                                            

ALTER TABLE "vms_layout" RENAME TO "vms_layout_tmp";

-- Removing "user_id" field from the old database
CREATE TABLE "vms_layout" (
    "resource_ptr_id"           INTEGER PRIMARY KEY AUTOINCREMENT,
    "cell_aspect_ratio"         REAL NOT NULL DEFAULT -1.0,
    "cell_spacing_height"       REAL NOT NULL DEFAULT -1.0,
    "cell_spacing_width"        REAL NOT NULL DEFAULT -1.0,
    "user_can_edit"             BOOL NOT NULL DEFAULT 0,
    "locked"                    BOOL NOT NULL DEFAULT 0,
    "background_width"          INTEGER NOT NULL DEFAULT 1,
    "background_image_filename" TEXT NULL,
    "background_height"         INTEGER NOT NULL DEFAULT 1,
    "background_opacity"        REAL NOT NULL    
    );
    
INSERT INTO vms_layout (
    resource_ptr_id, cell_aspect_ratio, cell_spacing_height, cell_spacing_width, user_can_edit, locked,
    background_width, background_image_filename, background_height, background_opacity
    ) 
    SELECT 
    resource_ptr_id, cell_aspect_ratio, cell_spacing_height, cell_spacing_width, user_can_edit, locked,
    background_width, background_image_filename, background_height, background_opacity 
    FROM vms_layout_tmp;    
    
DROP TABLE vms_layout_tmp;

ALTER TABLE "vms_businessrule" ADD guid BLOB(16);
ALTER TABLE "vms_resourcetype" ADD guid BLOB(16);

ALTER TABLE "vms_businessrule_action_resources" RENAME TO vms_businessrule_action_resources_tmp;
CREATE TABLE "vms_businessrule_action_resources" (businessrule_guid BLOB(16) NOT NULL, resource_guid BLOB(16) NOT NULL, PRIMARY KEY(businessrule_guid, resource_guid));

ALTER TABLE "vms_businessrule_event_resources" RENAME TO vms_businessrule_event_resources_tmp;
CREATE TABLE "vms_businessrule_event_resources" (businessrule_guid BLOB(16) NOT NULL, resource_guid BLOB(16) NOT NULL, PRIMARY KEY(businessrule_guid, resource_guid));


CREATE TABLE "transaction_log" (
                peer_guid   BLOB(16) NOT NULL,
                db_guid     BLOB(16) NOT NULL,
                sequence    INTEGER NOT NULL,
                timestamp   INTEGER NOT NULL,
                tran_guid   BLOB(16) NOT NULL,
                tran_data   BLOB  NOT NULL);

CREATE UNIQUE INDEX idx_transaction_key   ON transaction_log(peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_hash  ON transaction_log(tran_guid);
CREATE INDEX idx_transaction_time  ON transaction_log(timestamp);

CREATE TABLE "vms_storedFiles" (path VARCHAR PRIMARY KEY, data BLOB);


INSERT INTO "vms_propertytype" ("resource_type_id",name,type,min,max,step,"values","ui_values","default_value",netHelper,"group","sub_group",description,ui,readonly ) VALUES ('624','DeviceID','1',NULL,NULL,NULL,'','','','','','','','0','0' );
