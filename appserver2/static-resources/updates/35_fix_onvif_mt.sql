DELETE FROM vms_kvpair WHERE name = 'supportedMotion' and coalesce(value,'') = '';
