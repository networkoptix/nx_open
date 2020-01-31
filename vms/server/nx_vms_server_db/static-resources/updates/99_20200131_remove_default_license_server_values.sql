DELETE FROM vms_kvpair WHERE name = 'licenseServer'
    AND (value = 'http://licensing.vmsproxy.com' OR value = 'https://licensing.vmsproxy.com');
