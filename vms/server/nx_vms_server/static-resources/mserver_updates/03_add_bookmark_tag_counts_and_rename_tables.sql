ALTER TABLE storage_bookmark_tag RENAME TO bookmark_tags;
ALTER TABLE storage_bookmark RENAME TO bookmarks;

CREATE TABLE bookmark_tag_counts (
    tag     TEXT NOT NULL PRIMARY KEY,
    count   INTEGER NOT NULL
);
