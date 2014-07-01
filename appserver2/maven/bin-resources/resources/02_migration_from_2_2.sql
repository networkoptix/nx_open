update vms_resource set guid = '99cbc715-539b-4bfe-856f-799b45b69b1e'
 where id = (select id from auth_user where username = 'admin');

INSERT INTO vms_kvpair (resource_id, value, name)
 select (select id from auth_user where username = 'admin'), s.value, s.name
 from vms_setting s;
