/* index.c --- index filesystem revisions.
 *
 * Written by Edmund Horner (with source adapted from libsvn_repos/dump.c).
 *
 * ====================================================================
 * Copyright (c) 2000-2004 CollabNet.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://subversion.tigris.org/.
 * ====================================================================
 */


#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_md5.h"
#include "svn_props.h"

#include "svnindex.h"

#include <libpq-fe.h>

#define ARE_VALID_COPY_ARGS(p,r) ((p && SVN_IS_VALID_REVNUM (r)) ? 1 : 0)



/*----------------------------------------------------------------------*/

static svn_error_t *
write_prop_records (apr_hash_t *hash,
                    PGconn *conn,
                    int node_id,
                    apr_pool_t *pool)
{
  apr_hash_index_t *this;      /* current hash entry */

  for (this = apr_hash_first (pool, hash); this; this = apr_hash_next (this))
    {
      const void *key;
      void *val;
      apr_ssize_t keylen;
      svn_string_t *value;

      /* Get this key and val. */
      apr_hash_this (this, &key, &keylen, &val);
      value = val;

      SVN_ERR(db_write_prop(conn, node_id, key, value->data, pool));
    }

  return SVN_NO_ERROR;
}



/*----------------------------------------------------------------------*/

/** An editor that indexes node-data to a PostgreSQL database. **/

struct edit_baton
{
  /* The path which implicitly prepends all full paths coming into
     this editor.  This will almost always be "" or "/".  */
  const char *path;

  /* Send feedback here, if non-NULL */
  svn_stream_t *feedback_stream;

  PGconn *conn;
  int repos_id;

  svn_boolean_t verbose;

  /* The fs revision root, so we can read the contents of paths. */
  svn_fs_root_t *fs_root;
  svn_revnum_t current_rev;

  /* reusable buffer for writing file contents */
  char buffer[SVN_STREAM_CHUNK_SIZE];
  apr_size_t bufsize;
};

struct dir_baton
{
  struct edit_baton *edit_baton;
  struct dir_baton *parent_dir_baton;

  /* is this directory a new addition to this revision? */
  svn_boolean_t added;
  
  /* has this directory been written to the output stream? */
  svn_boolean_t written_out;

  /* the absolute path to this directory */
  const char *path;

  /* the comparison path and revision of this directory.  if both of
     these are valid, use them as a source against which to compare
     the directory instead of the default comparison source of PATH in
     the previous revision. */
  const char *cmp_path;
  svn_revnum_t cmp_rev;

  /* hash of paths that need to be deleted, though some -might- be
     replaced.  maps const char * paths to this dir_baton.  (they're
     full paths, because that's what the editor driver gives us.  but
     really, they're all within this directory.) */
  apr_hash_t *deleted_entries;

  /* pool to be used for deleting the hash items */
  apr_pool_t *pool;
};


/* Make a directory baton to represent the directory was path
   (relative to EDIT_BATON's path) is PATH.  

   CMP_PATH/CMP_REV are the path/revision against which this directory
   should be compared for changes.  If either is omitted (NULL for the
   path, SVN_INVALID_REVNUM for the rev), just compare this directory
   PATH against itself in the previous revision.
   
   PARENT_DIR_BATON is the directory baton of this directory's parent,
   or NULL if this is the top-level directory of the edit.  ADDED
   indicated if this directory is newly added in this revision.
   Perform all allocations in POOL.  */
static struct dir_baton *
make_dir_baton (const char *path,
                const char *cmp_path,
                svn_revnum_t cmp_rev,
                void *edit_baton,
                void *parent_dir_baton,
                svn_boolean_t added,
                apr_pool_t *pool)
{
  struct edit_baton *eb = edit_baton;
  struct dir_baton *pb = parent_dir_baton;
  struct dir_baton *new_db = apr_pcalloc (pool, sizeof (*new_db));
  const char *full_path;

  /* A path relative to nothing?  I don't think so. */
  if (path && (! pb))
    abort();

  /* Construct the full path of this node. */
  if (pb)
    full_path = svn_path_join (eb->path, path, pool);
  else
    full_path = apr_pstrdup (pool, eb->path);

  /* Remove leading slashes from copyfrom paths. */
  if (cmp_path)
    cmp_path = ((*cmp_path == '/') ? cmp_path + 1 : cmp_path);

  new_db->edit_baton = eb;
  new_db->parent_dir_baton = pb;
  new_db->path = full_path;
  new_db->cmp_path = cmp_path ? apr_pstrdup (pool, cmp_path) : NULL;
  new_db->cmp_rev = cmp_rev;
  new_db->added = added;
  new_db->written_out = FALSE;
  new_db->deleted_entries = apr_hash_make (pool);
  new_db->pool = pool;
  
  return new_db;
}


/* This helper is the main "meat" of the editor -- it does all the
   work of writing a node record.
   
   Write out a node record for PATH of type KIND under EB->FS_ROOT.
   ACTION describes what is happening to the node (see enum svn_node_action).
   Write record to writable EB->STREAM, using EB->BUFFER to write in chunks.

   If the node was itself copied, IS_COPY is TRUE and the
   path/revision of the copy source are in CMP_PATH/CMP_REV.  If
   IS_COPY is FALSE, yet CMP_PATH/CMP_REV are valid, this node is part
   of a copied subtree.
  */
static svn_error_t *
index_node (struct edit_baton *eb,
           const char *path,    /* an absolute path. */
           svn_node_kind_t kind,
           enum svn_node_action action,
           svn_boolean_t is_copy,
           const char *cmp_path,
           svn_revnum_t cmp_rev,
           apr_pool_t *pool)
{
  svn_boolean_t must_dump_text = FALSE, must_dump_props = FALSE;
  const char *compare_path = path;
  svn_revnum_t compare_rev = eb->current_rev - 1;
  svn_fs_root_t *compare_root = NULL;

  int node_id;

  const char *kind_str = "unknown";
  const char *action_str = "unknown";
  const char *copyfrom_rev_str = NULL;
  const char *copyfrom_path_str = NULL;
  const char *props_id_str = NULL;
  const char *text_id_str = NULL;
  const char *size_str = NULL;
  const char *md5_str = NULL;

  /* Remove leading slashes from copyfrom paths. */
  if (cmp_path)
    cmp_path = ((*cmp_path == '/') ? cmp_path + 1 : cmp_path);

  /* Validate the comparison path/rev. */
  if (ARE_VALID_COPY_ARGS (cmp_path, cmp_rev))
    {
      compare_path = cmp_path;
      compare_rev = cmp_rev;
    }

  if (kind == svn_node_file)
    {
      kind_str = "file";
    }
  else if (kind == svn_node_dir)
    {
      kind_str = "dir";
    }

  if (action == svn_node_action_change)
    {
      action_str = "change";

      if (eb->verbose)
        {
          SVN_ERR (svn_stream_printf (eb->feedback_stream, pool,
                                      _("    * editing path : %s ..."),
                                      path));
        }

      /* either the text or props changed, or possibly both. */
      SVN_ERR (svn_fs_revision_root (&compare_root, 
                                     svn_fs_root_fs (eb->fs_root),
                                     compare_rev, pool));
      
      SVN_ERR (svn_fs_props_changed (&must_dump_props,
                                     compare_root, compare_path,
                                     eb->fs_root, path, pool));
      if (kind == svn_node_file)
        SVN_ERR (svn_fs_contents_changed (&must_dump_text,
                                          compare_root, compare_path,
                                          eb->fs_root, path, pool));
    }
  else if (action == svn_node_action_replace)
    {
      if (eb->verbose)
        {
          SVN_ERR (svn_stream_printf (eb->feedback_stream, pool,
                                      _("    * replacing path : %s ..."),
                                      path));
        }

      if (! is_copy)
        {
          /* a simple delete+add, implied by a single 'replace' action. */
          action_str = "change";

          /* definitely need to dump all content for a replace. */
          if (kind == svn_node_file)
            must_dump_text = TRUE;
          must_dump_props = TRUE;
        }
      else
        {
          /* more complex:  delete original, then add-with-history.  */

          /* N.B. Unlike the original dump.c, recusion now will perform
          a change BEFORE this change.  We must therefore do the delete
          in the recusion, and the add-with-history here.

          /* recurse:  print an additional delete record. */
          SVN_ERR (index_node (eb, path, kind, svn_node_action_delete,
                               FALSE, NULL, NULL, pool));

          action_str = "add";

          copyfrom_rev_str = apr_psprintf (pool, "%ld", cmp_rev);
          copyfrom_path_str = cmp_path;

          SVN_ERR (svn_fs_revision_root (&compare_root, 
                                         svn_fs_root_fs (eb->fs_root),
                                         compare_rev, pool));

          /* Need to decide if the copied node had any extra textual or
             property mods as well.  */
          SVN_ERR (svn_fs_props_changed (&must_dump_props,
                                         compare_root, compare_path,
                                         eb->fs_root, path, pool));
          if (kind == svn_node_file)
            SVN_ERR (svn_fs_contents_changed (&must_dump_text,
                                              compare_root, compare_path,
                                              eb->fs_root, path, pool));
          
          /* ### someday write a node-copyfrom-source-checksum. */
        }
    }
  else if (action == svn_node_action_delete)
    {
      action_str = "delete";

      if (eb->verbose)
        {
          SVN_ERR (svn_stream_printf (eb->feedback_stream, pool,
                                      _("    * deleting path : %s ..."),
                                      path));
        }

      /* we can leave this routine quietly now, don't need to dump
         any content. */
      must_dump_text = FALSE;
      must_dump_props = FALSE;
    }
  else if (action == svn_node_action_add)
    {
      action_str = "add";

      if (eb->verbose)
        {
          SVN_ERR (svn_stream_printf (eb->feedback_stream, pool,
                                      _("    * adding path : %s ..."),
                                      path));
        }

      if (! is_copy)
        {
          /* Dump all contents for a simple 'add'. */
          if (kind == svn_node_file)
            must_dump_text = TRUE;
          must_dump_props = TRUE;
        }
      else
        {
          copyfrom_rev_str = apr_psprintf (pool, "%ld", cmp_rev);
          copyfrom_path_str = cmp_path;

          SVN_ERR (svn_fs_revision_root (&compare_root, 
                                         svn_fs_root_fs (eb->fs_root),
                                         compare_rev, pool));

          /* Need to decide if the copied node had any extra textual or
             property mods as well.  */
          SVN_ERR (svn_fs_props_changed (&must_dump_props,
                                         compare_root, compare_path,
                                         eb->fs_root, path, pool));
          if (kind == svn_node_file)
            SVN_ERR (svn_fs_contents_changed (&must_dump_text,
                                              compare_root, compare_path,
                                              eb->fs_root, path, pool));
          
          /* ### someday write a node-copyfrom-source-checksum. */
        }
    }

  /*** Start prepping content to dump... ***/

  /* If we are supposed to dump text, write out a text length header
     here, and a md5 checksum (if available.) */
  if (must_dump_text && (kind == svn_node_file))
    {
      unsigned char md5_digest[APR_MD5_DIGESTSIZE];
      const char *hex_digest;
      svn_filesize_t textlen;

      /* Just fetch the length of the file. */
      SVN_ERR (svn_fs_file_length (&textlen, eb->fs_root, path, pool));

      SVN_ERR (svn_fs_file_md5_checksum (md5_digest, eb->fs_root, path, pool));
      hex_digest = svn_md5_digest_to_cstring (md5_digest, pool);

      size_str = apr_psprintf (pool, "%ld", textlen);
      md5_str = hex_digest;
    }

    SVN_ERR(db_create_node(&node_id,
                         eb->conn,
                         eb->repos_id,
                         eb->current_rev,
                         kind_str,
                         action_str,
                         path,
                         copyfrom_rev_str,
                         copyfrom_path_str,
                         pool));

  if (size_str)
    {
      SVN_ERR(db_set_node_size(eb->conn, path, node_id, size_str, md5_str, pool));
    }

  /* Store props into the svn_prop table if necessary. */
  if (must_dump_props)
    {
      apr_hash_t *prophash;

      SVN_ERR (svn_fs_node_proplist (&prophash, eb->fs_root, path, pool));

      /* Store the props for this node in the database. */
      SVN_ERR(write_prop_records(prophash, eb->conn, node_id, pool));

      SVN_ERR(db_set_node_props(eb->conn, path, node_id, node_id, pool));
    }


  /* Dump text content */
  if (must_dump_text && (kind == svn_node_file))
    {
      int lines_changed;
      svn_string_t *bufstr;

      SVN_ERR(get_lines_changed(&lines_changed, &bufstr, eb->fs_root, path, compare_root, compare_path, pool));

      if (bufstr)
        SVN_ERR(db_set_node_content(eb->conn, path, node_id, md5_str, bufstr->data, eb->feedback_stream, pool));
    }
  

  /* If verbosity was requested, print the path. */
  if (eb->verbose)
    {
      SVN_ERR (svn_stream_printf (eb->feedback_stream, pool,
                                  _("\n")));
    }

  return SVN_NO_ERROR;
}


static svn_error_t *
open_root (void *edit_baton, 
           svn_revnum_t base_revision, 
           apr_pool_t *pool,
           void **root_baton)
{
  *root_baton = make_dir_baton (NULL, NULL, SVN_INVALID_REVNUM, 
                                edit_baton, NULL, FALSE, pool);
  return SVN_NO_ERROR;
}


static svn_error_t *
delete_entry (const char *path,
              svn_revnum_t revision, 
              void *parent_baton,
              apr_pool_t *pool)
{
  struct dir_baton *pb = parent_baton;
  const char *mypath = apr_pstrdup (pb->pool, path);

  /* remember this path needs to be deleted. */
  apr_hash_set (pb->deleted_entries, mypath, APR_HASH_KEY_STRING, pb);

  return SVN_NO_ERROR;
}


static svn_error_t *
add_directory (const char *path,
               void *parent_baton,
               const char *copyfrom_path,
               svn_revnum_t copyfrom_rev,
               apr_pool_t *pool,
               void **child_baton)
{
  struct dir_baton *pb = parent_baton;
  struct edit_baton *eb = pb->edit_baton;
  void *val;
  svn_boolean_t is_copy = FALSE;
  struct dir_baton *new_db 
    = make_dir_baton (path, copyfrom_path, copyfrom_rev, eb, pb, TRUE, pool);

  /* This might be a replacement -- is the path already deleted? */
  val = apr_hash_get (pb->deleted_entries, path, APR_HASH_KEY_STRING);

  /* Detect an add-with-history. */
  is_copy = ARE_VALID_COPY_ARGS (copyfrom_path, copyfrom_rev) ? TRUE : FALSE;

  /* Index the node. */
  SVN_ERR (index_node (eb, path, 
                      svn_node_dir,
                      val ? svn_node_action_replace : svn_node_action_add,
                      is_copy,
                      is_copy ? copyfrom_path : NULL, 
                      is_copy ? copyfrom_rev : SVN_INVALID_REVNUM,
                      pool));

  if (val)
    /* Delete the path, it's now been indexed. */
    apr_hash_set (pb->deleted_entries, path, APR_HASH_KEY_STRING, NULL);
  
  new_db->written_out = TRUE;

  *child_baton = new_db;
  return SVN_NO_ERROR;
}


static svn_error_t *
open_directory (const char *path,
                void *parent_baton,
                svn_revnum_t base_revision,
                apr_pool_t *pool,
                void **child_baton)
{
  struct dir_baton *pb = parent_baton;
  struct edit_baton *eb = pb->edit_baton;
  struct dir_baton *new_db;
  const char *cmp_path = NULL;
  svn_revnum_t cmp_rev = SVN_INVALID_REVNUM;

  /* If the parent directory has explicit comparison path and rev,
     record the same for this one. */
  if (pb && ARE_VALID_COPY_ARGS (pb->cmp_path, pb->cmp_rev))
    {
      cmp_path = svn_path_join (pb->cmp_path, 
                                svn_path_basename (path, pool), pool);
      cmp_rev = pb->cmp_rev;
    }
        
  new_db = make_dir_baton (path, cmp_path, cmp_rev, eb, pb, FALSE, pool);
  *child_baton = new_db;
  return SVN_NO_ERROR;
}


static svn_error_t *
close_directory (void *dir_baton,
                 apr_pool_t *pool)
{
  struct dir_baton *db = dir_baton;
  struct edit_baton *eb = db->edit_baton;
  apr_hash_index_t *hi;
  apr_pool_t *subpool = svn_pool_create (pool);
  
  for (hi = apr_hash_first (pool, db->deleted_entries);
       hi;
       hi = apr_hash_next (hi))
    {
      const void *key;
      const char *path;
      apr_hash_this (hi, &key, NULL, NULL);
      path = key;

      svn_pool_clear (subpool);

      /* By sending 'svn_node_unknown', the Node-kind: header simply won't
         be written out.  No big deal at all, really.  The loader
         shouldn't care.  */
      SVN_ERR (index_node (eb, path,
                          svn_node_unknown, svn_node_action_delete,
                          FALSE, NULL, SVN_INVALID_REVNUM, subpool));
    }

  svn_pool_destroy (subpool);
  return SVN_NO_ERROR;
}


static svn_error_t *
add_file (const char *path,
          void *parent_baton,
          const char *copyfrom_path,
          svn_revnum_t copyfrom_rev,
          apr_pool_t *pool,
          void **file_baton)
{
  struct dir_baton *pb = parent_baton;
  struct edit_baton *eb = pb->edit_baton;
  void *val;
  svn_boolean_t is_copy = FALSE;

  /* This might be a replacement -- is the path already deleted? */
  val = apr_hash_get (pb->deleted_entries, path, APR_HASH_KEY_STRING);

  /* Detect add-with-history. */
  is_copy = ARE_VALID_COPY_ARGS (copyfrom_path, copyfrom_rev) ? TRUE : FALSE;

  /* Index the node. */
  SVN_ERR (index_node (eb, path, 
                      svn_node_file,
                      val ? svn_node_action_replace : svn_node_action_add,
                      is_copy,
                      is_copy ? copyfrom_path : NULL, 
                      is_copy ? copyfrom_rev : SVN_INVALID_REVNUM, 
                      pool));

  if (val)
    /* delete the path, it's now been indexed. */
    apr_hash_set (pb->deleted_entries, path, APR_HASH_KEY_STRING, NULL);

  *file_baton = NULL;  /* muhahahaha */
  return SVN_NO_ERROR;
}


static svn_error_t *
open_file (const char *path,
           void *parent_baton,
           svn_revnum_t ancestor_revision,
           apr_pool_t *pool,
           void **file_baton)
{
  struct dir_baton *pb = parent_baton;
  struct edit_baton *eb = pb->edit_baton;
  const char *cmp_path = NULL;
  svn_revnum_t cmp_rev = SVN_INVALID_REVNUM;
  
  /* If the parent directory has explicit comparison path and rev,
     record the same for this one. */
  if (pb && ARE_VALID_COPY_ARGS (pb->cmp_path, pb->cmp_rev))
    {
      cmp_path = svn_path_join (pb->cmp_path, 
                                svn_path_basename (path, pool), pool);
      cmp_rev = pb->cmp_rev;
    }

  SVN_ERR (index_node (eb, path, 
                      svn_node_file, svn_node_action_change, 
                      FALSE, cmp_path, cmp_rev, pool));

  *file_baton = NULL;  /* muhahahaha again */
  return SVN_NO_ERROR;
}


static svn_error_t *
change_dir_prop (void *parent_baton,
                 const char *name,
                 const svn_string_t *value,
                 apr_pool_t *pool)
{
  struct dir_baton *db = parent_baton;
  struct edit_baton *eb = db->edit_baton;

  /* This function is what distinguishes between a directory that is
     opened to merely get somewhere, vs. one that is opened because it
     *actually* changed by itself.  */
  if (! db->written_out)
    {
      SVN_ERR (index_node (eb, db->path, 
                          svn_node_dir, svn_node_action_change, 
                          FALSE, db->cmp_path, db->cmp_rev, pool));
      db->written_out = TRUE;
    }
  return SVN_NO_ERROR;
}



static svn_error_t *
get_index_editor (const svn_delta_editor_t **editor,
                  void **edit_baton,
                  svn_fs_t *fs,
                  svn_revnum_t to_rev,
                  const char *root_path,
                  svn_stream_t *feedback_stream,
                  PGconn *conn,
                  int repos_id,
                  svn_boolean_t verbose,
                  apr_pool_t *pool)
{
  /* Allocate an edit baton to be stored in every directory baton.
     Set it up for the directory baton we create here, which is the
     root baton. */
  struct edit_baton *eb = apr_pcalloc (pool, sizeof (*eb));
  svn_delta_editor_t *index_editor = svn_delta_default_editor (pool);

  /* Set up the edit baton. */
  eb->feedback_stream = feedback_stream;
  eb->bufsize = sizeof(eb->buffer);
  eb->path = apr_pstrdup (pool, root_path);
  SVN_ERR (svn_fs_revision_root (&(eb->fs_root), fs, to_rev, pool));
  eb->current_rev = to_rev;

  eb->conn = conn;
  eb->repos_id = repos_id;

  eb->verbose = verbose;

  /* Set up the editor. */
  index_editor->open_root = open_root;
  index_editor->delete_entry = delete_entry;
  index_editor->add_directory = add_directory;
  index_editor->open_directory = open_directory;
  index_editor->close_directory = close_directory;
  index_editor->change_dir_prop = change_dir_prop;
  index_editor->add_file = add_file;
  index_editor->open_file = open_file;

  *edit_baton = eb;
  *editor = index_editor;
  
  return SVN_NO_ERROR;
}

/*----------------------------------------------------------------------*/

/** The main indexing routine. **/

static svn_error_t *
write_revprop_records (apr_hash_t *hash,
                       PGconn *conn,
                       int repos_id,
                       svn_revnum_t rev,
                       apr_pool_t *pool)
{
  apr_hash_index_t *this;      /* current hash entry */

  for (this = apr_hash_first (pool, hash); this; this = apr_hash_next (this))
    {
      const void *key;
      void *val;
      apr_ssize_t keylen;
      svn_string_t *value;

      /* Get this key and val. */
      apr_hash_this (this, &key, &keylen, &val);
      value = val;

      SVN_ERR(db_write_revprop(conn, repos_id, rev, key, value->data, pool));
    }

  return SVN_NO_ERROR;
}


/* Write a revision record of REV in FS to writable STREAM, using POOL.
 */
static svn_error_t *
write_revision_record (PGconn *conn,
                       svn_fs_t *fs,
                       int repos_id,
                       svn_revnum_t rev,
                       apr_pool_t *pool)
{
  apr_hash_t *props;
  apr_time_t timetemp;
  svn_string_t *datevalue;

  /* Read the revision props even if we're aren't going to dump
     them for verification purposes */
  SVN_ERR (svn_fs_revision_proplist (&props, fs, rev, pool));

  /* Run revision date properties through the time conversion to
     canonize them. */
  /* ### Remove this when it is no longer needed for sure. */
  datevalue = apr_hash_get (props, SVN_PROP_REVISION_DATE,
                            APR_HASH_KEY_STRING);
  if (datevalue)
    {
      SVN_ERR (svn_time_from_cstring (&timetemp, datevalue->data, pool));
      datevalue = svn_string_create (svn_time_to_cstring (timetemp, pool),
                                     pool);
      apr_hash_set (props, SVN_PROP_REVISION_DATE, APR_HASH_KEY_STRING,
                    datevalue);
    }

  SVN_ERR(db_start_revision(conn, repos_id, rev, pool));

  /* Store the revprops in the database. */
  SVN_ERR(write_revprop_records(props, conn, repos_id, rev, pool));

  return SVN_NO_ERROR;
}


/* The main dumper. */
svn_error_t *
svnindex__index (svn_repos_t *repos,
                 svn_stream_t *feedback_stream,
                 svn_revnum_t start_rev,
                 svn_revnum_t end_rev,
                 svn_boolean_t verbose,
                 const char *db,
                 svn_cancel_func_t cancel_func,
                 void *cancel_baton,
                 apr_pool_t *pool)
{
  const svn_delta_editor_t *index_editor;
  void *index_edit_baton;
  svn_revnum_t i;  
  svn_fs_t *fs = svn_repos_fs (repos);
  apr_pool_t *subpool = svn_pool_create (pool);
  svn_revnum_t youngest, db_youngest;
  const char *uuid;
  PGconn *conn;
  int repos_id;

  db_notice_baton_t notice_baton = { feedback_stream, pool, verbose };

  /* Connect to PostgreSQL database. */
  SVN_ERR(db_connect(&conn, db, &notice_baton, pool));

  /* Find the UUID, and get the matching repository id */
  SVN_ERR (svn_fs_get_uuid(fs, &uuid, pool));

  SVN_ERR (db_get_repository_id(conn, uuid, &repos_id, pool));

  /* Determine the current youngest revision of the filesystem and the db. */
  SVN_ERR (svn_fs_youngest_rev (&youngest, fs, pool));

  SVN_ERR (db_get_youngest_rev (&db_youngest, conn, repos_id, pool));

  /* Use default vals if necessary. */
  if (! SVN_IS_VALID_REVNUM (start_rev))
    start_rev = db_youngest+1;
  if (! SVN_IS_VALID_REVNUM (end_rev))
    end_rev = youngest;
  if (! feedback_stream)
    feedback_stream = svn_stream_empty (pool);

  /* Validate the revisions. */
  if (start_rev > end_rev)
    return svn_error_createf (SVN_ERR_REPOS_BAD_ARGS, NULL,
                              _("Start revision %ld"
                                " is greater than end revision %ld"),
                              start_rev, end_rev);
  if (end_rev > youngest)
    return svn_error_createf (SVN_ERR_REPOS_BAD_ARGS, NULL,
                              _("End revision %ld is invalid "
                                "(youngest revision is %ld)"),
                              end_rev, youngest);

  /* Main loop:  we're going to dump revision i.  */
  for (i = start_rev; i <= end_rev; i++)
    {
      svn_revnum_t from_rev, to_rev;
      svn_fs_root_t *to_root;

      svn_pool_clear (subpool);

      /* Check for cancellation. */
      if (cancel_func)
        SVN_ERR (cancel_func (cancel_baton));

      /* Set revision numbers on either side of the delta we're going
         to fetch. */
      from_rev = i - 1;
      to_rev = i;

      if (verbose)
        {
          SVN_ERR (svn_stream_printf (feedback_stream, pool,
                                      _("* Indexing revision %ld.\n"),
                                      to_rev));
        }

      /* Start a new PostgreSQL transaction. */
      SVN_ERR(db_begin(conn));

      /* Create a new revision record in the database. */
      SVN_ERR (write_revision_record (conn,
                                      fs,
                                      repos_id,
                                      to_rev,
                                      subpool));

      /* Fetch the editor that indexes nodes. */
      SVN_ERR (get_index_editor (&index_editor,
                                 &index_edit_baton,
                                 fs,
                                 to_rev,
                                 "/",
                                 feedback_stream,
                                 conn,
                                 repos_id,
                                 verbose,
                                 subpool));

      SVN_ERR (svn_fs_revision_root (&to_root, fs, to_rev, subpool));

      SVN_ERR (svn_repos_replay (to_root, index_editor, 
                                 index_edit_baton, subpool));

      /* Commit transaction. */
      SVN_ERR(db_commit(conn));

      SVN_ERR (svn_stream_printf (feedback_stream, pool,
                                  _("* Indexed revision %ld.\n"),
                                  to_rev));
    }

  svn_pool_destroy (subpool);

  return SVN_NO_ERROR;
}

