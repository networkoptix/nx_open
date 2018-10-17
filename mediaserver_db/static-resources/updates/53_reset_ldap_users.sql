UPDATE vms_userprofile SET crypt_sha512_hash = '', digest = '' WHERE is_ldap = 1;
UPDATE auth_user SET password = '' WHERE id in 
    (SELECT user_id from vms_userprofile WHERE is_ldap = 1);
