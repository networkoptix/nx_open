ALTER TABLE auth_user RENAME TO auth_user_tmp;

CREATE TABLE "auth_user" (
    "id" integer NOT NULL PRIMARY KEY autoincrement,
    "username" varchar(30) NOT NULL,
    "first_name" varchar(30),
    "last_name" varchar(30),
    "email" varchar(75),
    "password" varchar(128) NOT NULL,
    "is_staff" bool NULL,
    "is_active" bool NULL,
    "is_superuser" bool NOT NULL,
    "last_login" datetime NULL,
    "date_joined" datetime NULL
);

INSERT INTO auth_user SELECT * from auth_user_tmp;
DROP TABLE auth_user_tmp;
