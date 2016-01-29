-- WebPage resource table
CREATE TABLE "vms_webpage" (
    resource_ptr_id integer NOT NULL UNIQUE PRIMARY KEY);

INSERT INTO "vms_resourcetype" (name) VALUES ('WebPage');
