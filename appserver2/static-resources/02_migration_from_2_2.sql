update vms_resource set guid = '99cbc715-539b-4bfe-856f-799b45b69b1e'
 where id = (select id from auth_user where username = 'admin');

update vms_setting set name = "smtpHost" where name = "EMAIL_HOST";
update vms_setting set name = "smtpPort" where name = "EMAIL_PORT";
update vms_setting set name = "smtpUser" where name = "EMAIL_HOST_USER";
update vms_setting set name = "smptPassword" where name = "EMAIL_HOST_PASSWORD";
update vms_setting set name = "smtpSimple" where name = "EMAIL_SIMPLE";
update vms_setting set name = "smtpTimeout" where name = "EMAIL_TIMEOUT";
update vms_setting set name = "emailFrom" where name = "EMAIL_FROM";
update vms_setting set name = "emailSignature" where name = "EMAIL_SIGNATURE";
insert into vms_setting (name, value) values("smtpConnectionType", case when exists(select 1 from vms_setting where name = "EMAIL_USE_TLS" and value="true") then 2
else case when exists(select 1 from vms_setting where name = "EMAIL_USE_SSL" and value = "true") then 1 else 0 end end);

delete from vms_setting where name = "EMAIL_USE_TLS";
delete from vms_setting where name = "EMAIL_USE_SSL";

INSERT INTO vms_kvpair (resource_id, value, name)
 select (select id from auth_user where username = 'admin'), s.value, s.name
 from vms_setting s;

ALTER TABLE auth_user RENAME TO auth_user_tmp;
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
INSERT INTO auth_user (id, username, first_name, last_name, email, password, is_staff, is_active, is_superuser, last_login, date_joined)
     SELECT vms_userprofile.resource_ptr_id,
            username, first_name, last_name, email, password, is_staff, is_active, is_superuser, last_login, date_joined
       FROM auth_user_tmp
       JOIN vms_userprofile on user_id = auth_user_tmp.id;
DROP TABLE auth_user_tmp;

ALTER TABLE vms_userprofile RENAME TO vms_userprofile_tmp;
CREATE TABLE "vms_userprofile" (
    "user_id" integer NOT NULL UNIQUE,
    "digest" varchar(128) NULL,
    "resource_ptr_id" integer PRIMARY KEY autoincrement,
    "rights" integer unsigned NOT NULL DEFAULT 0);
INSERT INTO vms_userprofile (user_id, digest, resource_ptr_id, rights)
     SELECT resource_ptr_id, digest, resource_ptr_id, rights
       FROM vms_userprofile_tmp;
DROP TABLE vms_userprofile_tmp;

update vms_cameraserveritem set timestamp = timestamp * 1000;

DELETE FROM vms_property 
    WHERE id NOT IN (
        SELECT MAX(id) 
            FROM vms_property p 
            GROUP BY resource_id, property_type_id);

INSERT INTO vms_kvpair (resource_id, name, value)
     SELECT p.resource_id, pt.name, p.value
       FROM vms_property p
       JOIN vms_propertyType pt on pt.id = p.property_type_id;
