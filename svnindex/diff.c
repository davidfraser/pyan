/* diff.c --- get and manipulate diffs for indexing.
 *
 * Written by Edmund Horner.
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


#include <assert.h>

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
#include "svn_diff.h"



/* Copied from svnlook/main.c. */
static svn_error_t *
open_writable_binary_file (apr_file_t **fh, 
                           const char *path /* UTF-8! */, 
                           apr_pool_t *pool)
{
  apr_array_header_t *path_pieces;
  svn_error_t *err, *err2 = NULL;
  int i;
  const char *full_path, *dir;
  
  /* Try the easy way to open the file. */
  err = svn_io_file_open (fh, path, 
                          APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
                          APR_OS_DEFAULT, pool);
  if (! err)
    return SVN_NO_ERROR;

  svn_path_split (path, &dir, NULL, pool);

  path_pieces = svn_path_decompose (dir, pool);

  /* If the file path has no parent, then we've already tried to open
     it as best as we care to try above. */
  if (! path_pieces->nelts)
    return err;

  full_path = "";
  for (i = 0; i < path_pieces->nelts; i++)
    {
      svn_node_kind_t kind;
      const char *piece = ((const char **) (path_pieces->elts))[i];
      full_path = svn_path_join (full_path, piece, pool);
      if ((err2 = svn_io_check_resolved_path (full_path, &kind, pool)))
        goto cleanup;

      /* Does this path component exist at all? */
      if (kind == svn_node_none)
        {
          if ((err2 = svn_io_dir_make (full_path, APR_OS_DEFAULT, pool)))
            goto cleanup;
        }
      else if (kind != svn_node_dir)
        {
          return svn_error_createf (err->apr_err, err,
                                    _("Error creating dir '%s' (path exists)"),
                                    full_path);
        }
    }

  /* Now that we are ensured that the parent path for this file
     exists, try once more to open it. */
  err2 = svn_io_file_open (fh, path, 
                           APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
                           APR_OS_DEFAULT, pool);

 cleanup:
  svn_error_clear (err);
  return err2;
}


/* Copied from svnlook/main.c. */
static svn_error_t *
dump_contents (apr_file_t *fh,
               svn_fs_root_t *root,
               const char *path /* UTF-8! */,
               apr_pool_t *pool)
{
  svn_stream_t *contents, *file_stream;

  /* Grab the contents and copy them into fh. */
  SVN_ERR (svn_fs_file_contents (&contents, root, path, pool));
  file_stream = svn_stream_from_aprfile (fh, pool);
  SVN_ERR (svn_stream_copy (contents, file_stream, pool));
  return SVN_NO_ERROR;
}


/* Copied from svnlook/main.c. */
static svn_error_t *
prepare_tmpfiles (const char **tmpfile1,
                  const char **tmpfile2,
                  svn_boolean_t *is_binary,
                  svn_fs_root_t *root1,
                  const char *path1,
                  svn_fs_root_t *root2,
                  const char *path2,
                  const char *tmpdir,
                  apr_pool_t *pool)
{
  svn_string_t *mimetype;
  apr_file_t *fh;

  /* Init the return values. */
  *tmpfile1 = NULL;
  *tmpfile2 = NULL;
  *is_binary = FALSE;

  assert (path1 && path2);

  /* Check for binary mimetypes.  If either file has a binary
     mimetype, get outta here.  */
  if (root1)
    {
      SVN_ERR (svn_fs_node_prop (&mimetype, root1, path1, 
                                 SVN_PROP_MIME_TYPE, pool));
      if (mimetype && svn_mime_type_is_binary (mimetype->data))
        {
          *is_binary = TRUE;
          return SVN_NO_ERROR;
        }
    }
  if (root2)
    {
      SVN_ERR (svn_fs_node_prop (&mimetype, root2, path2, 
                                 SVN_PROP_MIME_TYPE, pool));
      if (mimetype && svn_mime_type_is_binary (mimetype->data))
        {
          *is_binary = TRUE;
          return SVN_NO_ERROR;
        }
    }

  /* Now, prepare the two temporary files, each of which will either
     be empty, or will have real contents.  The first file we will
     make in our temporary directory. */
  *tmpfile2 = svn_path_join (tmpdir, "sometempfile", pool);
  SVN_ERR (open_writable_binary_file (&fh, *tmpfile2, pool));
  if (root2)
    SVN_ERR (dump_contents (fh, root2, path2, pool));
  apr_file_close (fh);

  /* The second file is constructed from the first one's path. */
  SVN_ERR (svn_io_open_unique_file (&fh, tmpfile1, *tmpfile2, 
                                    ".tmp", FALSE, pool));
  if (root1)
    SVN_ERR (dump_contents (fh, root1, path1, pool));
  apr_file_close (fh);

  return SVN_NO_ERROR;
}


static svn_error_t *
remove_tmpfiles(const char *orig_path, const char *new_path, apr_pool_t *pool)
{
  if (orig_path)
    SVN_ERR(svn_io_remove_file(orig_path, pool));
  if (new_path)
    SVN_ERR(svn_io_remove_file(new_path, pool));

  return SVN_NO_ERROR;
}


static svn_error_t *
store_diff(apr_file_t **tempfile,
           svn_diff_t *diff,
           const char *original_path,
           const char *modified_path,
           apr_pool_t *pool)
{
  const char *tempdir;
  svn_stream_t *temp_stream;
  apr_off_t offset = 0;

  /* Create a temporary file and open a stream to it. */
  SVN_ERR(svn_io_temp_dir(&tempdir, pool));
  SVN_ERR(svn_io_open_unique_file2(tempfile, NULL,
                                   apr_psprintf(pool, "%s/dump", tempdir),
                                   ".tmp", svn_io_file_del_on_close, pool));
  temp_stream = svn_stream_from_aprfile(*tempfile, pool);

  SVN_ERR (svn_diff_file_output_unified2
            (temp_stream, diff, original_path, modified_path,
            NULL, NULL, svn_cmdline_output_encoding(pool), pool));

  offset = 0;
  SVN_ERR(svn_io_file_seek(*tempfile, APR_SET, &offset, pool));

  return SVN_NO_ERROR;
}


typedef struct
{
  int count;
} output_baton_t;

svn_error_t *output_diff_modified(void *output_baton,
                                 apr_off_t original_start, apr_off_t original_length,
                                 apr_off_t modified_start, apr_off_t modified_length,
                                 apr_off_t latest_start, apr_off_t latest_length)
{
  output_baton_t *b = output_baton;

  b->count += (int) modified_length;

  return SVN_NO_ERROR;
}


static svn_diff_output_fns_t output_fns =
{
  NULL,
  output_diff_modified,
  NULL,
  NULL,
  NULL
};


svn_error_t *
get_lines_changed(int *lines_changed,
                  svn_string_t **bufstr,
                  svn_fs_root_t *root, const char *path,
                  svn_fs_root_t *compare_root, const char *compare_path,
                  apr_pool_t *pool)
{
  char *tempdir;
  const char *orig_path = NULL, *new_path = NULL;
  svn_boolean_t binary = FALSE;
  svn_diff_t *diff;
  output_baton_t output_baton;
  apr_file_t *temp_file;
  svn_stringbuf_t *strbuf;
  svn_boolean_t had_first_block = FALSE;

  SVN_ERR (svn_io_temp_dir (&tempdir, pool));
  
  /* Save the two versions of the file to temporary files. */
  SVN_ERR (prepare_tmpfiles (&orig_path, &new_path, &binary,
                             compare_root, compare_path, root, path,
                             tempdir, pool));

  /* Compute the diff. */
  if (binary)
  {
    *lines_changed = 0;
    *bufstr = NULL;
    SVN_ERR(remove_tmpfiles(orig_path, new_path, pool));
    return SVN_NO_ERROR;
  }
  
  SVN_ERR (svn_diff_file_diff (&diff, orig_path, new_path, pool));
  if (svn_diff_contains_diffs (diff))
  {
    SVN_ERR(store_diff(&temp_file, diff, orig_path, new_path, pool));
  }

  /* Count how many +/- lines there are in it. */
  output_baton.count = 0;
  SVN_ERR(svn_diff_output(diff, &output_baton, &output_fns));

  *lines_changed = output_baton.count;

  if (*lines_changed == 0)
  {
    *bufstr = NULL;
    SVN_ERR(remove_tmpfiles(orig_path, new_path, pool));
    return SVN_NO_ERROR;
  }

  /* Read the diff, ignore irrelevant parts (everything except +/- lines),
     and tokenise it. */
  strbuf = svn_stringbuf_create ("", pool);

  do
    {
      static unsigned char buf[1024 * 1024 + 1];
      apr_size_t buflen;
      svn_error_t *err;

      buflen = sizeof(buf) - 1;

      err = svn_io_read_length_line(temp_file, buf, &buflen, pool);
      if (err)
        {
          if (APR_STATUS_IS_EOF(err->apr_err))
            {
              svn_error_clear(err);
              break;
            }
/*          else if (APR_STATUS_IS
            {
              *bufstr = NULL;
              return SVN_NO_ERROR;
            }*/
          else
            return err;
        }

      if (buflen == 0)
        continue;

      if (buf[0] == '@')
        had_first_block = TRUE;

      if (had_first_block && (buf[0] == '+' || buf[0] == '-'))
        {
          svn_stringbuf_appendbytes (strbuf, buf, buflen);
          svn_stringbuf_appendbytes (strbuf, "\n", 1);
        }
    }
  while (TRUE);

  *bufstr = svn_string_create_from_buf (strbuf, pool);

  /* Remove non-alphanumeric characters. */
  {
    unsigned int i;
    for (i = 0; i < (*bufstr)->len; i++)
    {
      if ((unsigned char) (*bufstr)->data[i] >= 127 || !isalnum((unsigned char) (*bufstr)->data[i]))
        *((unsigned char *) &(*bufstr)->data[i]) = ' ';
    }
  }

  SVN_ERR(remove_tmpfiles(orig_path, new_path, pool));
  return SVN_NO_ERROR;
}
