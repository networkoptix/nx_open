ALTER TABLE storage_bookmark_tag RENAME TO bookmark_tags;
ALTER TABLE storage_bookmark RENAME TO bookmarks;

CREATE TABLE bookmark_tag_counts (
    tag     TEXT NOT NULL PRIMARY KEY,
    count   INTEGER NOT NULL
);

CREATE TRIGGER increment_bookmark_tag_counter AFTER INSERT ON bookmark_tags
BEGIN
    INSERT OR IGNORE INTO bookmark_tag_counts (tag, count) VALUES (NEW.name, 0);
    UPDATE bookmark_tag_counts SET count = count + 1 WHERE tag = NEW.name;
END;


CREATE TRIGGER decrement_bookmark_tag_counter AFTER DELETE ON bookmark_tags
BEGIN
    UPDATE bookmark_tag_counts SET count = count - 1 WHERE tag = OLD.name;
    DELETE FROM bookmark_tag_counts WHERE tag = OLD.name AND count <= 0;
END;
