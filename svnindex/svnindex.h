/* svnindex.h --- declare svnindex global stuff.
 *
 * Written by Edmund Horner.
 *
 * ====================================================================
 * Copyright (c) 2000-2006 CollabNet.  All rights reserved.
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

/* ==================================================================== */



#ifndef SVNINDEX_H
#define SVNINDEX_H

/*** Includes. ***/
#include <apr_tables.h>
#include <apr_getopt.h>

#include "svn_wc.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_opt.h"
#include "svn_auth.h"

#include <libpq-fe.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


    
/*----------------------------------------------------------------------*/
/* PostgreSQL database manipulation stuff. */

typedef struct
{
  svn_stream_t *stream;
  apr_pool_t *pool;
  svn_boolean_t verbose;
} db_notice_baton_t;

extern svn_error_t *db_connect(PGconn **conn,
                               const char *db,
                               db_notice_baton_t *notice_baton,
                               apr_pool_t *pool);

extern svn_error_t *db_begin(PGconn *conn);

extern svn_error_t *db_commit(PGconn *conn);

extern svn_error_t *db_create_savepoint(PGconn *conn,
                                        const char *name);

extern svn_error_t *db_rollback_to_savepoint(PGconn *conn,
                                             const char *name);

extern svn_error_t *db_release_savepoint(PGconn *conn,
                                         const char *name);

extern svn_error_t *db_write_prop (PGconn *conn,
                                    int node_id,
                                    const char *key,
                                    const char *value,
                                    apr_pool_t *pool);

extern svn_error_t *db_write_revprop (PGconn *conn,
                                       int repos_id,
                                       svn_revnum_t rev,
                                       const char *key,
                                       const char *value,
                                       apr_pool_t *pool);

extern svn_error_t *db_start_revision(PGconn *conn,
                                       int repos_id,
                                       int rev,
                                       apr_pool_t *pool);

extern svn_error_t *db_get_repository_id(PGconn *conn,
                                          const char *uuid,
                                          int *repos_id,
                                          apr_pool_t *pool);

extern svn_error_t *db_get_youngest_rev(svn_revnum_t *rev,
                                         PGconn *conn,
                                         int repos_id,
                                         apr_pool_t *pool);


extern svn_error_t *db_create_node(int *node_id,
										             	 PGconn *conn,
																	 int repos_id,
																	 int rev,
																	 const char *kind_str,
																	 const char *action_str,
																	 const char *path,
                                   const char *copyfrom_rev_str,
                                   const char *copyfrom_path,
														 			 apr_pool_t *pool);

extern svn_error_t *db_set_node_size(PGconn *conn,
                                     const char *path,
                                     int node_id,
                                     const char *size_str,
                                     const char *md5_str,
                                     apr_pool_t *pool);

extern svn_error_t *db_set_node_props(PGconn *conn,
                                      const char *path,
                                      int node_id,
                                      int props_id,
                                      apr_pool_t *pool);

extern svn_error_t *db_set_node_content(PGconn *conn,
                                        const char *path,
                                        int node_id,
                                        const char *md5_str,
                                        const char *content,
                                        svn_stream_t *feedback_stream,
                                        apr_pool_t *pool);



/*----------------------------------------------------------------------*/
/* Diff stuff. */

extern svn_error_t *get_lines_changed(int *lines_changed,
                                       svn_string_t **bufstr,
                                       svn_fs_root_t *root,
                                       const char *path,
                                       svn_fs_root_t *compare_root,
                                       const char *compare_path,
                                       apr_pool_t *pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVNINDEX_H */
