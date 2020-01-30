DELETE FROM vms_kvpair WHERE name = 'licenseServer' AND coalesce(value, '') = 'http://licensing.vmsproxy.com';
    