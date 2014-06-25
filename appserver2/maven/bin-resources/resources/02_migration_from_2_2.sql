update vms_resource set guid = '99cbc715-539b-4bfe-856f-799b45b69b1e'
where id = (select id from auth_user where username = 'admin');

