
ALTER TABLE "vms_server" RENAME TO "vms_server_tmp";

CREATE TABLE "vms_server" (
    resource_ptr_id     integer NOT NULL,
    auth_key            varchar(1024),
    version             varchar(1024),
    net_addr_list       varchar(1024),
    flags               number,
    system_info         varchar(1024),
    PRIMARY KEY(resource_ptr_id)
);


INSERT INTO vms_server
  SELECT
    resource_ptr_id,
    auth_key,
    version,
    net_addr_list,
    flags,
    system_info
  FROM vms_server_tmp;

DROP TABLE vms_server_tmp;
