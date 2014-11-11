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

update auth_user set id = (select resource_ptr_id from vms_userprofile where user_id = auth_user.id);
update vms_userprofile set user_id = resource_ptr_id;

update vms_cameraserveritem set timestamp = timestamp * 1000;

INSERT INTO vms_kvpair (resource_id, name, value)
     SELECT p.resource_id, pt.name, p.value
       FROM vms_property p
       JOIN vms_propertyType pt on pt.id = p.property_type_id;
