/*
 * schema-svn.sql
 *
 * This file is part of svnindex, and describes the database schema
 * (for PostgreSQL).
 *
 * ====================================================================
 * Copyright (c) 2005-2007 Edmund Horner.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 * ====================================================================
 */


CREATE SCHEMA svn;

DROP TABLE svn.repository CASCADE;
DROP TABLE svn.revision CASCADE;
DROP TABLE svn.revprop CASCADE;
DROP TABLE svn.node CASCADE;
DROP TABLE svn.prop CASCADE;
DROP TABLE svn.content CASCADE;

CREATE TABLE svn.repository
(
    id SERIAL NOT NULL,
    uuid VARCHAR,
    name VARCHAR NOT NULL,
    url VARCHAR NOT NULL,
    indexed BOOLEAN NOT NULL,
    connect VARCHAR,
    tool VARCHAR,    
    
    CONSTRAINT repository_pk_id PRIMARY KEY (id),
    
    CONSTRAINT repository_uq_name UNIQUE (name),
    CONSTRAINT repository_uq_uuid UNIQUE (uuid)
) WITHOUT OIDS;

CREATE TABLE svn.revision
(
    repos_id INTEGER NOT NULL,
    rev INTEGER NOT NULL,
    svn_author VARCHAR,
    svn_date TIMESTAMP,
    svn_log VARCHAR,
    
    CONSTRAINT revision_pk_repos_rev PRIMARY KEY (repos_id,rev),
    
    CONSTRAINT revision_fk_repos_id FOREIGN KEY (repos_id)
        REFERENCES svn.repository (id)
        ON DELETE CASCADE
        ON UPDATE CASCADE,
    
    CONSTRAINT revision_ck_rev_ge_0 CHECK (rev >= 0)
) WITHOUT OIDS;

CREATE TABLE svn.revprop
(
    repos_id INTEGER NOT NULL,
    rev INTEGER NOT NULL,
    name VARCHAR NOT NULL,
    value VARCHAR NOT NULL,
    
    CONSTRAINT revprop_pk_repos_rev_name PRIMARY KEY (repos_id,rev,name),
    
    CONSTRAINT revprop_fk_repos_rev FOREIGN KEY (repos_id,rev)
        REFERENCES svn.revision (repos_id,rev)
        ON DELETE CASCADE
        ON UPDATE CASCADE
) WITHOUT OIDS;

CREATE TABLE svn.node
(
    id SERIAL NOT NULL,
    repos_id INTEGER NOT NULL,
    rev INTEGER NOT NULL,
    path VARCHAR NOT NULL,
    kind VARCHAR NOT NULL,
    action VARCHAR NOT NULL,
    name VARCHAR NOT NULL,
    parent_id INTEGER,
    predecessor_id INTEGER,
    copyfrom_rev INTEGER,
    copyfrom_path VARCHAR,
    props_id INTEGER,
    text_id INTEGER,
    size INTEGER,
    md5 VARCHAR(32),
    children INTEGER[],
    
    CONSTRAINT node_pk_id PRIMARY KEY (id),
    
    CONSTRAINT node_uq_repos_rev_path_action UNIQUE (repos_id,rev,path,action),
    
    CONSTRAINT node_fk_repos_rev FOREIGN KEY (repos_id,rev)
        REFERENCES svn.revision (repos_id,rev)
        ON DELETE CASCADE
        ON UPDATE CASCADE,
    
    CONSTRAINT node_ck_kind CHECK (kind IN ('file', 'dir', 'unknown')),
    CONSTRAINT node_ck_action CHECK (action IN ('add', 'change', 'delete', 'replace', 'unknown'))
) WITHOUT OIDS;

CREATE INDEX node_ix_name ON svn.node (name);
CREATE INDEX node_ix_props_id ON svn.node (props_id);
CREATE INDEX node_ix_text_id ON svn.node (text_id) WHERE text_id IS NOT NULL;
CREATE INDEX node_ix_md5 ON svn.node (md5) WHERE md5 IS NOT NULL;

CREATE TABLE svn.prop
(
    node_id INTEGER NOT NULL,
    name VARCHAR NOT NULL,
    value VARCHAR NOT NULL,
    
    CONSTRAINT prop_pk_node_name PRIMARY KEY (node_id,name),
    
    CONSTRAINT prop_fk_node_id FOREIGN KEY (node_id)
        REFERENCES svn.node (id)
        ON DELETE CASCADE
        ON UPDATE CASCADE
) WITHOUT OIDS;

CREATE TABLE svn.content
(
    node_id INTEGER NOT NULL,
    vector tsvector NOT NULL,
    
    CONSTRAINT content_pk_node_id PRIMARY KEY (node_id)
) WITHOUT OIDS;

CREATE INDEX content_ix_vector ON svn.content USING GIST (vector);

ALTER TABLE svn.node ADD CONSTRAINT node_fk_text_id FOREIGN KEY (text_id)
    REFERENCES svn.content (node_id)
    ON UPDATE CASCADE;



GRANT USAGE ON SCHEMA svn TO localuser;

GRANT SELECT,INSERT,UPDATE,DELETE ON
    svn.repository, svn.revision, svn.revprop,
    svn.node, svn.prop, svn.content,
    pg_ts_cfg, pg_ts_cfgmap, pg_ts_dict, pg_ts_parser,
    svn.repository_id_seq, svn.node_id_seq
TO localuser;



-- get_repository_id(uuid)
-- Return the id of a repository, creating a new record if necessary.
CREATE OR REPLACE FUNCTION svn.get_repository_id(TEXT) RETURNS INTEGER
LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
    rid INTEGER;
BEGIN
    SELECT INTO rid id FROM svn.repository WHERE uuid = $1;
    
    IF rid IS NULL THEN
        INSERT INTO svn.repository (uuid) VALUES ($1);
        SELECT INTO rid id FROM svn.repository WHERE uuid = $1;
    END IF;
    
    RETURN rid;
END;
$$;

-- create_revision(repos_id, rev)
-- Create a revision with the given attributes and return its id (rev).
CREATE OR REPLACE FUNCTION svn.create_revision(INTEGER, INTEGER) RETURNS INTEGER
LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
BEGIN
    INSERT INTO svn.revision (repos_id, rev) VALUES ($1, $2);
    
    IF $2 = 0 THEN
        INSERT INTO svn.node (repos_id, rev, path, kind, action, name) VALUES ($1, 0, '', 'dir', 'add', '');
        UPDATE svn.node SET props_id = id WHERE repos_id = $1 AND rev = 0 and path = '';
    ELSE
        IF NOT EXISTS (SELECT * FROM svn.revision WHERE repos_id = $1 AND rev = $2-1) THEN
            RAISE EXCEPTION 'previous revision (r%) not indexed yet!', $2-1;
        END IF;
    END IF;
    
    RETURN $2;
END;
$$;

-- get_parent_id(repos_id, rev, path)
-- Create a node's parent if necessary.
CREATE OR REPLACE FUNCTION svn.get_parent_id(INTEGER, INTEGER, TEXT) RETURNS INTEGER
LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
    pid INTEGER;
    parent_path TEXT := '';
BEGIN
    IF $3 = '' THEN
        RETURN NULL;
    END IF;
    
    IF $3 LIKE '%/%' THEN
        SELECT INTO parent_path regexp_replace($3, '^(.+)/[^/]+$', E'\\1');
    END IF;
    
    SELECT INTO pid id FROM svn.node WHERE repos_id = $1 AND rev = $2 AND path = parent_path ORDER BY action DESC;
    
    IF pid IS NULL THEN
        SELECT INTO pid svn.create_node($1, $2, parent_path, 'dir', 'change', NULL, NULL);
    END IF;
    
    RETURN pid;
END;
$$;

--- get_children(id)
-- Return a table of this node's children.
CREATE OR REPLACE FUNCTION svn.get_children(INTEGER) RETURNS SETOF INTEGER
LANGUAGE 'plpgsql' STABLE STRICT AS
$$
DECLARE
    i INTEGER;
    len INTEGER;
    children INTEGER[];
BEGIN
    SELECT INTO children n.children FROM svn.node AS n WHERE id = $1;
    
    IF children IS NULL THEN
        RETURN;
    END IF;
    
    len := array_upper(children, 1);
    i := 1;
    
    LOOP
        IF i > len THEN
            EXIT;
        END IF;
    
        RETURN NEXT children[i];
        
        i := i + 1;
    END LOOP;
    
    RETURN;
END;
$$;

-- get_predecessor_id(repos_id, rev, path)
-- Find a predecessor for a node.
-- This is done by walking through the tree from the root directory of
-- the latest revision (implemented by finding the predecessor of the
-- parent, and looking for a matching child).
CREATE OR REPLACE FUNCTION svn.get_predecessor_id(INTEGER, INTEGER, TEXT) RETURNS INTEGER
LANGUAGE 'plpgsql' STABLE STRICT AS
$$
DECLARE
    pid INTEGER;
    node_name TEXT;
    parent_path TEXT := '';
    old_settings TEXT[];
    rv INTEGER;
BEGIN
    IF $3 = '' THEN
        RETURN (SELECT id FROM svn.node WHERE repos_id = $1 AND rev <= $2 AND path = '' ORDER BY rev DESC LIMIT 1);
    END IF;
    
    SELECT INTO node_name regexp_replace($3, '^.+/([^/]+)$', E'\\1');
    
    IF $3 LIKE '%/%' THEN
        SELECT INTO parent_path regexp_replace($3, '^(.+)/[^/]+$', E'\\1');
    END IF;
    
    SELECT INTO pid svn.get_predecessor_id($1, $2, parent_path);
    
    SELECT INTO old_settings ARRAY[set_config('enable_bitmapscan', 'off', TRUE),
                                   set_config('enable_hashjoin', 'off', TRUE),
                                   set_config('enable_mergejoin', 'off', TRUE)];

    
    SELECT INTO rv n.id
        FROM svn.node AS n JOIN svn.get_children(pid) AS c(id) ON c.id = n.id
        WHERE n.name = node_name;

    PERFORM set_config('enable_bitmapscan', old_settings[1], TRUE),
            set_config('enable_hashjoin', old_settings[2], TRUE),
            set_config('enable_mergejoin', old_settings[3], TRUE);
    

    RETURN rv;
END;
$$;



-- get_node_history(repos_id, rev, path, stop_on_copy)
-- Get a node's entire history, traversing copies as necessary.  Returns a
-- set of node ids in reverse revision order.
CREATE OR REPLACE FUNCTION svn.get_node_history(INTEGER, INTEGER, TEXT, BOOLEAN) RETURNS SETOF INTEGER
LANGUAGE 'plpgsql' STABLE STRICT AS
$$
DECLARE
    rev INTEGER;
    path TEXT;
    len INTEGER;
    i INTEGER;
    rec RECORD;
    parent_path TEXT;
BEGIN
    rev := $2;
    path := $3;
    
    FOR rec IN 
        SELECT
            r.rev,
            r.svn_author AS author,
            r.svn_date AS time,
            r.svn_log AS log,
            n.id,
            n.size,
            n.copyfrom_rev,
            n.copyfrom_path
        FROM
            svn.node AS n
            JOIN svn.revision AS r ON (n.repos_id = r.repos_id AND n.rev = r.rev)
        WHERE
            n.repos_id = $1
            AND n.path = path
            AND r.rev <= rev
    LOOP
        RETURN NEXT rec.id;
    END LOOP;
    
    IF NOT $4 AND NOT FOUND THEN
        parent_path := regexp_replace($3, '^(.+)/[^/]+$', E'\\1');
        
        FOR rec IN
            SELECT get_node_history AS id FROM svn.get_node_history($1, rev, parent_path, $4)
        LOOP
            RETURN NEXT rec.id;
        END LOOP;
        
        SELECT INTO rev, path
            n.rev,
            n.path
        FROM
            svn.node AS n
        WHERE
            n.id = svn.get_predecessor_id($1, rev, path);
    ELSE
        rev := rec.copyfrom_rev;
        path = rec.copyfrom_path;
    END IF;
    
    IF NOT $4 AND rev IS NOT NULL THEN
        FOR rec IN
            SELECT get_node_history AS id FROM svn.get_node_history($1, rev, path, $4)
        LOOP
            RETURN NEXT rec.id;
        END LOOP;
    END IF;
    
    RETURN;
END;
$$;



-- create_node(repos_id, rev, path, kind, action, copyfrom_rev, copyfrom_path)
-- Create a node with the given attributes and return its id.
CREATE OR REPLACE FUNCTION svn.create_node(INTEGER, INTEGER, TEXT, TEXT, TEXT, INTEGER, TEXT) RETURNS INTEGER
LANGUAGE 'plpgsql' VOLATILE CALLED ON NULL INPUT AS
$$
DECLARE
    nid INTEGER;
    pid INTEGER;
    pred_id INTEGER;
    node_name TEXT;
BEGIN
    -- First find its parent (creating it if necessary).
    SELECT INTO pid svn.get_parent_id($1, $2, $3);

    SELECT INTO nid nextval('svn.node_id_seq');
    
    SELECT INTO node_name regexp_replace($3, '^.+/([^/]+)$', E'\\1');
    
    -- Find a predecessor, if possible.
    IF $6 IS NOT NULL THEN
        SELECT INTO pred_id svn.get_predecessor_id($1, $6, $7);
    ELSE
        SELECT INTO pred_id svn.get_predecessor_id($1, $2 - 1, $3);
    END IF;

    -- Create the new node!
    INSERT INTO svn.node (id, repos_id, rev, path, kind, action, name, parent_id, predecessor_id)
        VALUES (nid, $1, $2, $3, $4, $5, node_name, pid, pred_id);
    
    -- If there is a parent, then update its list of children.
    IF pid IS NOT NULL THEN
        UPDATE svn.node SET children = children - COALESCE(ARRAY(
            SELECT n.id FROM svn.node AS n JOIN svn.get_children(pid) AS c(id) ON c.id = n.id
            WHERE n.name = node_name),'{}') WHERE id = pid;
        
        IF $5 IN ('add','change','replace') THEN
            UPDATE svn.node SET children = uniq(sort(COALESCE(children,'{}') + nid)) WHERE id = pid;
        END IF;
    END IF;
    
    -- If there is a precessor, then copy its contents (initially).
    IF pred_id IS NOT NULL THEN
        UPDATE svn.node SET props_id = (SELECT props_id FROM svn.node WHERE id = pred_id) WHERE id = nid;
        
        IF $4 = 'dir' THEN
            UPDATE svn.node SET children = (SELECT children FROM svn.node WHERE id = pred_id) WHERE id = nid;
        ELSE
            UPDATE svn.node SET text_id = (SELECT text_id FROM svn.node WHERE id = pred_id) WHERE id = nid;
            UPDATE svn.node SET size = (SELECT size FROM svn.node WHERE id = pred_id) WHERE id = nid;
            UPDATE svn.node SET md5 = (SELECT md5 FROM svn.node WHERE id = pred_id) WHERE id = nid;
        END IF;
    END IF;
    
    -- If it's a copy update it's copyfrom attributes.
    IF $6 IS NOT NULL THEN
        UPDATE svn.node SET copyfrom_rev = $6, copyfrom_path = $7 WHERE id = nid;
    END IF;
    
    RETURN nid;
END;
$$;

-- get_property(node_id, name)
-- Get the value of a node's property (NULL if the property isn't set).
CREATE OR REPLACE FUNCTION svn.get_property(INTEGER, TEXT) RETURNS TEXT
LANGUAGE 'sql' STABLE STRICT AS
$$
SELECT p.value FROM svn.prop AS p JOIN svn.node AS n ON p.node_id = n.props_id WHERE n.id = $1 AND p.name = $2;
$$;

-- set_content(node_id, md5, content)
-- Set the text_id attribute of this node and create a new content tuple
-- if necessary.
CREATE OR REPLACE FUNCTION svn.set_content(INTEGER, TEXT, TEXT) RETURNS BOOLEAN
LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
    cid INTEGER;
    v tsvector;
BEGIN
    -- First make sure it has svn:mime-type set to text.
    IF svn.get_property($1, 'svn:mime-type') IS NOT NULL AND svn.get_property($1, 'svn:mime-type') NOT LIKE 'text/%' THEN
        RETURN FALSE;
    END IF;
    
    -- Make sure it's not an empty vector.
    SELECT INTO v strip(to_tsvector($3));
    IF LENGTH(v) = 0 THEN
        RETURN FALSE;
    END IF;
    
    -- Try to find an existing usable content tuple.
    SELECT INTO cid text_id FROM svn.node
        JOIN svn.content ON text_id = node_id
        WHERE md5 IS NOT NULL AND md5 = $2 AND id != $1;
    
    -- If there wan't one, then make one.
    IF cid IS NULL THEN
        INSERT INTO svn.content (node_id, vector)
            VALUES ($1, v);
        SELECT INTO cid $1;
    END IF;
    
    -- Update the text_id attribute.
    UPDATE svn.node SET text_id = cid WHERE id = $1;
    
    RETURN TRUE;
END;
$$;

CREATE OR REPLACE RULE revprop_author AS
ON INSERT TO svn.revprop WHERE new.name = 'svn:author'
DO ALSO UPDATE svn.revision SET svn_author = new.value
WHERE repos_id = new.repos_id AND rev = new.rev;

CREATE OR REPLACE RULE revprop_date AS
ON INSERT TO svn.revprop WHERE new.name = 'svn:date'
DO ALSO UPDATE svn.revision
    SET svn_date = CAST(to_timestamp(new.value::TEXT, 'YYYY-MM-DD HH24:MI:SS.US') AS TIMESTAMP)
WHERE repos_id = new.repos_id AND rev = new.rev;

CREATE OR REPLACE RULE revprop_log AS
ON INSERT TO svn.revprop WHERE new.name = 'svn:log'
DO ALSO UPDATE svn.revision SET svn_log = new.value
WHERE repos_id = new.repos_id AND rev = new.rev;
