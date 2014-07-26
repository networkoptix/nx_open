CREATE TABLE "auth_group" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "name" varchar(80) NOT NULL UNIQUE
);
CREATE TABLE "auth_group_permissions" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "group_id" integer NOT NULL,
    "permission_id" integer NOT NULL REFERENCES "auth_permission" ("id"),
    UNIQUE ("group_id", "permission_id")
);
CREATE TABLE "auth_permission" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "name" varchar(50) NOT NULL,
    "content_type_id" integer NOT NULL,
    "codename" varchar(100) NOT NULL,
    UNIQUE ("content_type_id", "codename")
);
CREATE TABLE "auth_user" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "username" varchar(30) NOT NULL UNIQUE,
    "first_name" varchar(30),
    "last_name" varchar(30),
    "email" varchar(75),
    "password" varchar(128) NOT NULL,
    "is_staff" bool NOT NULL,
    "is_active" bool NOT NULL,
    "is_superuser" bool NOT NULL,
    "last_login" datetime NOT NULL,
    "date_joined" datetime NOT NULL
);
CREATE TABLE "auth_user_groups" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "user_id" integer NOT NULL,
    "group_id" integer NOT NULL REFERENCES "auth_group" ("id"),
    UNIQUE ("user_id", "group_id")
);
CREATE TABLE "auth_user_user_permissions" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "user_id" integer NOT NULL,
    "permission_id" integer NOT NULL REFERENCES "auth_permission" ("id"),
    UNIQUE ("user_id", "permission_id")
);
CREATE TABLE "django_admin_log" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "action_time" datetime NOT NULL,
    "user_id" integer NOT NULL REFERENCES "auth_user" ("id"),
    "content_type_id" integer REFERENCES "django_content_type" ("id"),
    "object_id" text,
    "object_repr" varchar(200) NOT NULL,
    "action_flag" smallint unsigned NOT NULL,
    "change_message" text NOT NULL
);
CREATE TABLE "django_content_type" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "name" varchar(100) NOT NULL,
    "app_label" varchar(100) NOT NULL,
    "model" varchar(100) NOT NULL,
    UNIQUE ("app_label", "model")
);
CREATE TABLE "django_session" (
    "session_key" varchar(40) NOT NULL PRIMARY KEY,
    "session_data" text NOT NULL,
    "expire_date" datetime NOT NULL
);
CREATE TABLE "django_site" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "domain" varchar(100) NOT NULL,
    "name" varchar(50) NOT NULL
);
CREATE TABLE "piston_consumer" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "name" varchar(255) NOT NULL,
    "description" text NOT NULL,
    "key" varchar(18) NOT NULL,
    "secret" varchar(32) NOT NULL,
    "status" varchar(16) NOT NULL,
    "user_id" integer REFERENCES "auth_user" ("id")
);
CREATE TABLE "piston_nonce" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "token_key" varchar(18) NOT NULL,
    "consumer_key" varchar(18) NOT NULL,
    "key" varchar(255) NOT NULL
);
CREATE TABLE "piston_token" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "key" varchar(18) NOT NULL,
    "secret" varchar(32) NOT NULL,
    "verifier" varchar(10) NOT NULL,
    "token_type" integer NOT NULL,
    "timestamp" integer NOT NULL,
    "is_approved" bool NOT NULL,
    "user_id" integer REFERENCES "auth_user" ("id"),
    "consumer_id" integer NOT NULL REFERENCES "piston_consumer" ("id"),
    "callback" varchar(255),
    "callback_confirmed" bool NOT NULL
);
CREATE TABLE "south_migrationhistory" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "app_name" varchar(255) NOT NULL,
    "migration" varchar(255) NOT NULL,
    "applied" datetime NOT NULL
);
CREATE TABLE "vms_businessrule" (
    "aggregation_period" integer, "action_params" varchar(16384) NOT NULL,
    "event_condition" varchar(16384) NOT NULL,
    "schedule" varchar(16384),
    "system" bool NOT NULL DEFAULT 0,
    "comments" varchar(16384),
    "disabled" bool NOT NULL DEFAULT 0,
    "action_type" smallint NOT NULL,
    "event_state" smallint NOT NULL DEFAULT 0,
    "id" integer PRIMARY KEY autoincrement,
    "event_type" smallint NOT NULL);

CREATE TABLE "vms_businessrule_action_resources" (
    "id" integer NOT NULL PRIMARY KEY,
    "businessrule_id" integer NOT NULL,
    "resource_id" integer NOT NULL);

CREATE TABLE "vms_businessrule_event_resources" (
    "id" integer NOT NULL PRIMARY KEY,
    "businessrule_id" integer NOT NULL,
    "resource_id" integer NOT NULL);

CREATE TABLE "vms_camera" (
    "audio_enabled" bool NOT NULL DEFAULT 0,
    "control_disabled" bool NOT NULL DEFAULT 0,
    "firmware" varchar(200) NOT NULL DEFAULT '',
    "vendor" varchar(200) NOT NULL DEFAULT '',
    "manually_added" bool NOT NULL DEFAULT 0,
    "resource_ptr_id" integer PRIMARY KEY autoincrement,
    "region" varchar(1024) NOT NULL,
    "schedule_disabled" bool NOT NULL DEFAULT 0,
    "motion_type" smallint NOT NULL DEFAULT 0,
    "group_name" varchar(200) NOT NULL DEFAULT '',
    "group_id" varchar(200) NOT NULL DEFAULT '',
    "mac" varchar(200) NOT NULL,
    "model" varchar(200) NOT NULL DEFAULT '',
    "secondary_quality" smallint NOT NULL DEFAULT 1,
    "status_flags" integer NOT NULL DEFAULT 0,
    "physical_id" varchar(200) NOT NULL DEFAULT '',
    "password" varchar(200) NOT NULL,
    "login" varchar(200) NOT NULL,
    "dewarping_params" varchar(200));

CREATE TABLE "vms_cameraserveritem" (
    "server_guid" varchar(40) NOT NULL,
    "timestamp" integer NOT NULL,
    "physical_id" varchar(200) NOT NULL,
    "id" integer PRIMARY KEY autoincrement);

CREATE TABLE "vms_kvpair" (
    "resource_id" integer NOT NULL DEFAULT 1,
    "id" integer PRIMARY KEY autoincrement,
    "value" varchar(200) NOT NULL,
    "name" varchar(200) NOT NULL);


CREATE TABLE "vms_layout" (
    "user_can_edit" bool NOT NULL DEFAULT 0,
    "cell_spacing_height" real NOT NULL DEFAULT -1.0,
    "locked" bool NOT NULL DEFAULT 0,
    "cell_aspect_ratio" real NOT NULL DEFAULT -1.0,
    "user_id" integer NOT NULL,
    "background_width" integer NOT NULL DEFAULT 1,
    "background_image_filename" varchar(1024),
    "background_height" integer NOT NULL DEFAULT 1,
    "cell_spacing_width" real NOT NULL DEFAULT -1.0, 
    "background_opacity" real NOT NULL,
    "resource_ptr_id" integer PRIMARY KEY autoincrement );


CREATE TABLE "vms_layoutitem" (
    "zoom_bottom" real NOT NULL DEFAULT 0,
    "right" real NOT NULL,
    "uuid" varchar(40) NOT NULL,
    "zoom_left" real NOT NULL DEFAULT 0,
    "resource_id" integer NOT NULL,
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


CREATE TABLE "vms_license" (
    "name" varchar(100) NOT NULL,
    "camera_count" integer NOT NULL,
    "raw_license" varchar(2048) NOT NULL DEFAULT '',
    "key" varchar(32) NOT NULL,
    "signature" varchar(1024) NOT NULL,
    "id" integer PRIMARY KEY autoincrement);


CREATE TABLE "vms_localresource" (
    "resource_ptr_id" integer NOT NULL PRIMARY KEY);


CREATE TABLE "vms_manufacture" (
    "id" integer NOT NULL PRIMARY KEY,
    "name" varchar(200) NOT NULL UNIQUE);


CREATE TABLE "vms_property" (
    "id" integer NOT NULL PRIMARY KEY,
    "resource_id" integer NOT NULL,
    "property_type_id" integer NOT NULL,
    "value" varchar(200) NOT NULL);


CREATE TABLE "vms_propertytype" (
    "id" integer NOT NULL PRIMARY KEY,
    "resource_type_id" integer NOT NULL,
    "name" varchar(200) NOT NULL,
    "type" smallint NOT NULL,
    "min" integer NULL,
    "max" integer NULL,
    "step" integer NULL,
    "values" varchar(200) NULL,
    "ui_values" varchar(200) NULL,
    "default_value" varchar(200) NULL,
    "netHelper" varchar(200) NULL,
    "group" varchar(200) NULL,
    "sub_group" varchar(200) NULL,
    "description" varchar(200) NULL,
    "ui" bool NULL,
    "readonly" bool NULL);


CREATE TABLE "vms_resource" (
    "status" smallint NOT NULL,
    "disabled" bool NOT NULL DEFAULT 0,
    "name" varchar(200) NOT NULL,
    "url" varchar(200),
    "xtype_id" integer NOT NULL,
    "parent_id" integer,
    "guid" varchar(40),
    "id" integer PRIMARY KEY autoincrement);


CREATE TABLE "vms_resourcetype" (
    "id" integer NOT NULL PRIMARY KEY,
    "name" varchar(200) NOT NULL,
    "description" varchar(200) NULL,
    "manufacture_id" integer NULL);


CREATE TABLE "vms_resourcetype_parents" (
    "id" integer NOT NULL PRIMARY KEY,
    "from_resourcetype_id" integer NOT NULL,
    "to_resourcetype_id" integer NOT NULL);


CREATE TABLE "vms_scheduletask" (
    "id" integer NOT NULL PRIMARY KEY,
    "source_id" integer NOT NULL,
    "start_time" integer NOT NULL,
    "end_time" integer NOT NULL,
    "do_record_audio" bool NOT NULL,
    "record_type" smallint NOT NULL,
    "day_of_week" smallint NOT NULL,
    "before_threshold" integer NOT NULL,
    "after_threshold" integer NOT NULL,
    "stream_quality" smallint NOT NULL,
    "fps" integer NOT NULL);


CREATE TABLE "vms_server" (
    "api_url" varchar(200) NOT NULL,
    "auth_key" varchar(1024),
    "streaming_url" varchar(200),
    "version" varchar(1024),
    "net_addr_list" varchar(1024),
    "reserve" bool NOT NULL DEFAULT 0,
    "resource_ptr_id" integer PRIMARY KEY autoincrement,
    "panic_mode" smallint NOT NULL);


CREATE TABLE "vms_setting" (
    "id" integer NOT NULL PRIMARY KEY,
    "name" varchar(200) NOT NULL UNIQUE,
    "value" varchar(200) NOT NULL);

CREATE TABLE "vms_storage" (
    "space_limit" integer NOT NULL,
    "used_for_writing" bool NOT NULL DEFAULT 1,
    "resource_ptr_id" integer PRIMARY KEY autoincrement);

CREATE TABLE "vms_userprofile" (
    "user_id" integer NOT NULL UNIQUE,
    "digest" varchar(128) NULL,
    "resource_ptr_id" integer PRIMARY KEY autoincrement,
    "rights" integer unsigned NOT NULL DEFAULT 0);
    
CREATE INDEX "auth_group_permissions_1e014c8f" ON "auth_group_permissions" ("permission_id");
CREATE INDEX "auth_group_permissions_425ae3c4" ON "auth_group_permissions" ("group_id");
CREATE INDEX "auth_permission_1bb8f392" ON "auth_permission" ("content_type_id");
CREATE INDEX "auth_user_groups_403f60f" ON "auth_user_groups" ("user_id");
CREATE INDEX "auth_user_groups_425ae3c4" ON "auth_user_groups" ("group_id");
CREATE INDEX "auth_user_user_permissions_1e014c8f" ON "auth_user_user_permissions" ("permission_id");
CREATE INDEX "auth_user_user_permissions_403f60f" ON "auth_user_user_permissions" ("user_id");
CREATE INDEX "django_admin_log_1bb8f392" ON "django_admin_log" ("content_type_id");
CREATE INDEX "django_admin_log_403f60f" ON "django_admin_log" ("user_id");
CREATE INDEX "django_session_3da3d3d8" ON "django_session" ("expire_date");
CREATE INDEX "piston_consumer_403f60f" ON "piston_consumer" ("user_id");
CREATE INDEX "piston_token_403f60f" ON "piston_token" ("user_id");
CREATE INDEX "piston_token_6565fc20" ON "piston_token" ("consumer_id");
CREATE INDEX "vms_businessrule_action_resources_31b73438" ON "vms_businessrule_action_resources" ("resource_id");
CREATE INDEX "vms_businessrule_action_resources_48bdae6c" ON "vms_businessrule_action_resources" ("businessrule_id");
CREATE UNIQUE INDEX "vms_businessrule_action_resources_businessrule_id__resource_id" ON "vms_businessrule_action_resources"("businessrule_id", "resource_id");
CREATE INDEX "vms_businessrule_event_resources_31b73438" ON "vms_businessrule_event_resources" ("resource_id");
CREATE INDEX "vms_businessrule_event_resources_48bdae6c" ON "vms_businessrule_event_resources" ("businessrule_id");
CREATE UNIQUE INDEX "vms_businessrule_event_resources_businessrule_id__resource_id" ON "vms_businessrule_event_resources"("businessrule_id", "resource_id");
CREATE INDEX "vms_kvpair_31b73438" ON "vms_kvpair" ("resource_id");
CREATE INDEX "vms_property_31b73438" ON "vms_property" ("resource_id");
CREATE INDEX "vms_property_5e9d1ca3" ON "vms_property" ("property_type_id");
CREATE INDEX "vms_propertytype_7d447b32" ON "vms_propertytype" ("resource_type_id");
CREATE INDEX "vms_resourcetype_1e476c7c" ON "vms_resourcetype" ("manufacture_id");
CREATE INDEX "vms_resourcetype_52094d6e" ON "vms_resourcetype" ("name");
CREATE INDEX "vms_resourcetype_parents_61c908f2" ON "vms_resourcetype_parents" ("from_resourcetype_id");
CREATE INDEX "vms_resourcetype_parents_6ad72b9f" ON "vms_resourcetype_parents" ("to_resourcetype_id");
CREATE UNIQUE INDEX "vms_resourcetype_parents_from_resourcetype_id__to_resourcetype_id" ON "vms_resourcetype_parents"("from_resourcetype_id", "to_resourcetype_id");
CREATE INDEX "vms_scheduletask_7607617b" ON "vms_scheduletask" ("source_id");
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '1','Can add permission','1','add_permission' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '2','Can change permission','1','change_permission' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '3','Can delete permission','1','delete_permission' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '4','Can add group','2','add_group' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '5','Can change group','2','change_group' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '6','Can delete group','2','delete_group' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '7','Can add user','3','add_user' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '8','Can change user','3','change_user' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '9','Can delete user','3','delete_user' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '10','Can add content type','4','add_contenttype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '11','Can change content type','4','change_contenttype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '12','Can delete content type','4','delete_contenttype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '13','Can add session','5','add_session' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '14','Can change session','5','change_session' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '15','Can delete session','5','delete_session' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '16','Can add site','6','add_site' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '17','Can change site','6','change_site' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '18','Can delete site','6','delete_site' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '19','Can add migration history','7','add_migrationhistory' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '20','Can change migration history','7','change_migrationhistory' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '21','Can delete migration history','7','delete_migrationhistory' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '22','Can add log entry','8','add_logentry' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '23','Can change log entry','8','change_logentry' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '24','Can delete log entry','8','delete_logentry' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '25','Can add nonce','9','add_nonce' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '26','Can change nonce','9','change_nonce' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '27','Can delete nonce','9','delete_nonce' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '28','Can add consumer','10','add_consumer' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '29','Can change consumer','10','change_consumer' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '30','Can delete consumer','10','delete_consumer' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '31','Can add token','11','add_token' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '32','Can change token','11','change_token' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '33','Can delete token','11','delete_token' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '34','Can add manufacture','12','add_manufacture' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '35','Can change manufacture','12','change_manufacture' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '36','Can delete manufacture','12','delete_manufacture' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '37','Can add resource type','13','add_resourcetype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '38','Can change resource type','13','change_resourcetype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '39','Can delete resource type','13','delete_resourcetype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '40','Can add property type','14','add_propertytype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '41','Can change property type','14','change_propertytype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '42','Can delete property type','14','delete_propertytype' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '43','Can add resource','15','add_resource' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '44','Can change resource','15','change_resource' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '45','Can delete resource','15','delete_resource' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '46','Can add property','16','add_property' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '47','Can change property','16','change_property' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '48','Can delete property','16','delete_property' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '49','Can add local resource','17','add_localresource' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '50','Can change local resource','17','change_localresource' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '51','Can delete local resource','17','delete_localresource' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '52','Can add user profile','18','add_userprofile' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '53','Can change user profile','18','change_userprofile' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '54','Can delete user profile','18','delete_userprofile' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '55','Can add camera','19','add_camera' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '56','Can change camera','19','change_camera' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '57','Can delete camera','19','delete_camera' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '58','Can add server','20','add_server' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '59','Can change server','20','change_server' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '60','Can delete server','20','delete_server' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '61','Can add storage','21','add_storage' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '62','Can change storage','21','change_storage' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '63','Can delete storage','21','delete_storage' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '64','Can add schedule task','22','add_scheduletask' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '65','Can change schedule task','22','change_scheduletask' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '66','Can delete schedule task','22','delete_scheduletask' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '67','Can add layout','23','add_layout' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '68','Can change layout','23','change_layout' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '69','Can delete layout','23','delete_layout' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '70','Can add layout item','24','add_layoutitem' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '71','Can change layout item','24','change_layoutitem' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '72','Can delete layout item','24','delete_layoutitem' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '73','Can add license','25','add_license' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '74','Can change license','25','change_license' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '75','Can delete license','25','delete_license' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '76','Can add camera server item','26','add_cameraserveritem' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '77','Can change camera server item','26','change_cameraserveritem' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '78','Can delete camera server item','26','delete_cameraserveritem' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '79','Can add business rule','27','add_businessrule' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '80','Can change business rule','27','change_businessrule' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '81','Can delete business rule','27','delete_businessrule' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '82','Can add kv pair','28','add_kvpair' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '83','Can change kv pair','28','change_kvpair' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '84','Can delete kv pair','28','delete_kvpair' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '85','Can add setting','29','add_setting' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '86','Can change setting','29','change_setting' );
INSERT INTO "auth_permission" ( id,name,"content_type_id",codename ) VALUES ( '87','Can delete setting','29','delete_setting' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '1','permission','auth','permission' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '2','group','auth','group' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '3','user','auth','user' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '4','content type','contenttypes','contenttype' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '5','session','sessions','session' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '6','site','sites','site' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '7','migration history','south','migrationhistory' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '8','log entry','admin','logentry' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '9','nonce','piston','nonce' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '10','consumer','piston','consumer' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '11','token','piston','token' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '12','manufacture','vms','manufacture' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '13','resource type','vms','resourcetype' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '14','property type','vms','propertytype' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '15','resource','vms','resource' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '16','property','vms','property' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '17','local resource','vms','localresource' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '18','user profile','vms','userprofile' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '19','camera','vms','camera' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '20','server','vms','server' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '21','storage','vms','storage' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '22','schedule task','vms','scheduletask' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '23','layout','vms','layout' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '24','layout item','vms','layoutitem' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '25','license','vms','license' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '26','camera server item','vms','cameraserveritem' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '27','business rule','vms','businessrule' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '28','kv pair','vms','kvpair' );
INSERT INTO "django_content_type" ( id,name,"app_label",model ) VALUES ( '29','setting','vms','setting' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{ "userGroup" : 0 }','{  }',NULL,'0',NULL,'0','7','2','1','3' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'0',NULL,'0','7','2','2','4' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'0',NULL,'0','7','2','3','5' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'0',NULL,'0','7','2','4','6' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'0',NULL,'0','7','2','5','7' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'0',NULL,'0','7','2','6','8' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','7','3' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','8','4' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','9','5' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','10','6' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','11','7' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','12','8' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','6','2','13','3' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','6','2','14','4' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','6','2','15','5' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','6','2','16','6' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','6','2','17','7' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '30','{  }','{  }',NULL,'1',NULL,'0','6','2','18','8' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '0','{  }','{  }',NULL,'1',NULL,'0','6','2','19','9' );
INSERT INTO "vms_businessrule" ( "aggregation_period","action_params","event_condition",schedule,system,comments,disabled,"action_type","event_state",id,"event_type" ) VALUES ( '21600','{  }','{  }',NULL,'0',NULL,'0','5','2','20','9' );

INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '1','7','1' );
INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '2','8','1' );
INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '3','9','1' );
INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '4','10','1' );
INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '5','11','1' );
INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '6','12','1' );
INSERT INTO "vms_businessrule_action_resources" ( id,"businessrule_id","resource_id" ) VALUES ( '7','20','1' );


INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '1','ArecontVision' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '2','Axis' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '3','ACTI' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '4','VIRTUAL_CAMERA' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '5','Stardot' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '6','VMAX' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '7','Dlink' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '8','NetworkOptixDroid' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '9','IqEye' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '10','ISD' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '11','NetworkOptix' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '12','OnvifDevice' );
INSERT INTO "vms_manufacture" ( id,name ) VALUES ( '13','THIRD_PARTY' );
INSERT INTO "vms_resource" ( status,disabled,name,url,"xtype_id","parent_id",guid,id ) VALUES ( '2','0','admin',NULL,'1',NULL,'99cbc715-539b-4bfe-856f-799b45b69b1e','1' );
INSERT INTO "auth_user" ( id,username,"first_name","last_name",email,password,"is_staff","is_active","is_superuser","last_login","date_joined" ) VALUES ( '1','admin',NULL,NULL,NULL,'md5$6XxRPmS7F5t2$cdeb4a7179c7fa31e46a9da1d666f611','1','1','1','2014-02-17 21:16:19.743000','2014-02-17 21:16:19.743000' );
INSERT INTO "vms_userprofile" ( "user_id",digest,"resource_ptr_id",rights ) VALUES ( '1','0bb0dc4ced8619406c29cb3130df127d','1','255' );

INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '1','User',NULL,NULL );
INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '2','Server',NULL,NULL );
INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '3','Layout',NULL,NULL );
INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '4','Storage',NULL,NULL );
INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '5','Local',NULL,NULL );
INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '6','Camera',NULL,NULL );

INSERT INTO "vms_propertytype" ( id,"resource_type_id",name,type,min,max,step,"values","ui_values","default_value",netHelper,"group","sub_group",description,ui,readonly ) VALUES ( '17238','6','mediaStreams','1',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'0','0' );
