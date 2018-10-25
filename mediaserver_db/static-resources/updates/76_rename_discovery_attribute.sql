UPDATE vms_kvpair 
  SET name = 'autoDiscoveryEnabled' 
  WHERE name = 'serverAutoDiscoveryEnabled';
  
UPDATE vms_kvpair 
  SET value = '' 
  WHERE name = 'disabledVendors' and value = 'all=partial';
 