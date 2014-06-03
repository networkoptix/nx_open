CREATE TABLE "vms_mserver_discovery" (
    "server_id" varchar(40) NOT NULL,
    "url" varchar(200) NOT NULL,
    "ignore" bool false,
    PRIMARY KEY("server_id", "url")
);
