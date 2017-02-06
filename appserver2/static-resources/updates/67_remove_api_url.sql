
ALTER TABLE "vms_server" RENAME TO "vms_server_tmp";

CREATE TABLE "vms_server" (
    auth_key            varchar(1024),
    version             varchar(1024),
    net_addr_list       varchar(1024),
    resource_ptr_id     integer,
    panic_mode          smallint NOT NULL,
    flags               number,
    system_info         varchar(1024),
    system_name         varchar(1024),
    PRIMARY KEY(resource_ptr_id)
);


INSERT INTO vms_server
  SELECT
    auth_key,
    version,
    net_addr_list,
    resource_ptr_id,
    panic_mode,
    flags,
    system_info,
    system_name
  FROM vms_server_tmp;

DROP TABLE vms_server_tmp;