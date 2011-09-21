/* database.c --- put data into the PostgreSQL database.
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


static apr_status_t
cleanup_pgconn (void *data)
{
  PGconn *conn = data;

  PQfinish(conn);

  return APR_SUCCESS;
}


static void
notice_receiver(void *arg, const PGresult *res)
{
  db_notice_baton_t *baton = arg;

  svn_stream_t *stream = baton->stream;
  apr_pool_t *pool = baton->pool;
  svn_boolean_t verbose = baton->verbose;

  svn_error_t *err;

  if (verbose)
    {
      const char *message = PQresultErrorMessage(res);
      err = svn_stream_printf (stream, pool,
                               _("%s"),
                               message);

      svn_error_clear(err);
    }
}


svn_error_t *
db_connect(PGconn **conn, const char *db, db_notice_baton_t *notice_baton, apr_pool_t *pool)
{
  *conn = PQconnectdb(db);

  /* Register cleanup function to close the connection when all is done. */
  apr_pool_cleanup_register(pool, *conn,
                            cleanup_pgconn, apr_pool_cleanup_null);

  /* Check the connection to make sure it's ok. */
  if (PQstatus(*conn) == CONNECTION_BAD)
    return svn_error_createf 
      (SVN_ERR_FS_GENERAL, NULL,
       _("Couldn't connect to PostgreSQL database using connect string '%s'; error message '%s'"),
       db, PQerrorMessage(*conn));

  /* Set up a notice receiver for the connection. */
  PQsetNoticeReceiver(*conn, notice_receiver, notice_baton);

  return SVN_NO_ERROR;
}


svn_error_t *
db_begin(PGconn *conn)
{
  PGresult *res;

  res = PQexec(conn, "BEGIN");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't start new transaction; error message '%s'"),
                                PQerrorMessage(conn));
    }
  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_commit(PGconn *conn)
{
  PGresult *res;

  res = PQexec(conn, "COMMIT");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't commit transaction; error message '%s'"),
                                PQerrorMessage(conn));
    }
  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_create_savepoint(PGconn *conn, const char *name)
{
  PGresult *res;
  char buf[1024];

  sprintf(buf, "SAVEPOINT %s", name);

  res = PQexec(conn, buf);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't create savepoint; error message '%s'"),
                                PQerrorMessage(conn));
    }
  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_rollback_to_savepoint(PGconn *conn, const char *name)
{
  PGresult *res;
  char buf[1024];

  sprintf(buf, "ROLLBACK TO SAVEPOINT %s", name);

  res = PQexec(conn, buf);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't rollback to savepoint; error message '%s'"),
                                PQerrorMessage(conn));
    }
  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_release_savepoint(PGconn *conn, const char *name)
{
  PGresult *res;
  char buf[1024];

  sprintf(buf, "RELEASE SAVEPOINT %s", name);

  res = PQexec(conn, buf);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't release savepoint; error message '%s'"),
                                PQerrorMessage(conn));
    }
  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_write_prop (PGconn *conn,
               int node_id,
               const char *key,
               const char *value,
               apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[4];

  param_values[0] = apr_psprintf (pool, "%d", node_id);
  param_values[1] = key;
  param_values[2] = value;

  res = PQexecParams(conn,
                     "INSERT INTO svn.prop (node_id, name, value) VALUES ($1, $2, $3)",
                     3,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't store prop; error message '%s'"),
                                PQerrorMessage(conn));
    }

  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_write_revprop (PGconn *conn,
                  int repos_id,
                  svn_revnum_t rev,
                  const char *key,
                  const char *value,
                  apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[4];

  param_values[0] = apr_psprintf (pool, "%d", repos_id);
  param_values[1] = apr_psprintf (pool, "%ld", rev);
  param_values[2] = key;
  param_values[3] = value;

  res = PQexecParams(conn,
                     "INSERT INTO svn.revprop (repos_id, rev, name, value) VALUES ($1, $2, $3, $4)",
                     4,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't store revprop; error message '%s'"),
                                PQerrorMessage(conn));
    }

  PQclear(res);
  
  return SVN_NO_ERROR;
}


svn_error_t *
db_start_revision(PGconn *conn, int repos_id, int rev, apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[2];

  param_values[0] = apr_psprintf (pool, "%d", repos_id);
  param_values[1] = apr_psprintf (pool, "%ld", rev);

  res = PQexecParams(conn,
                     "SELECT svn.create_revision($1, $2) AS id",
                     2,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    PQclear(res);
    return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                              _("Couldn't create revision; rev = %d, error message '%s'"),
                              rev, PQerrorMessage(conn));
  }

  /* Note that we don't need the return value, since we already know
     the repos_id and rev. */
  PQclear(res);
  
  return SVN_NO_ERROR;
}


svn_error_t *
db_get_repository_id(PGconn *conn, const char *uuid, int *repos_id, apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[1];
  int id_fnum;

  param_values[0] = uuid;
  res = PQexecParams(conn,
                     "SELECT svn.get_repository_id($1) AS id",
                     1,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't fetch repository id; error message '%s'"),
                                PQerrorMessage(conn));
    }

  id_fnum = PQfnumber(res, "id");

  *repos_id = atol(PQgetvalue(res, 0, id_fnum));

  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_get_youngest_rev(svn_revnum_t *rev, PGconn *conn, int repos_id, apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[1];
  int rev_fnum;

  param_values[0] = apr_psprintf (pool, "%d", repos_id);;
  res = PQexecParams(conn,
                     "SELECT MAX(rev) AS rev FROM svn.revision WHERE repos_id = $1",
                     1,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't fetch youngest revision; error message '%s'"),
                                PQerrorMessage(conn));
    }

  rev_fnum = PQfnumber(res, "rev");

  if (PQgetisnull(res, 0, rev_fnum))
    {
      *rev = -1;
    }
  else
    {
      *rev = atol(PQgetvalue(res, 0, rev_fnum));
    }

  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_create_node(int *node_id,
							 PGconn *conn,
							 int repos_id,
							 int rev,
							 const char *kind_str,
							 const char *action_str,
							 const char *path,
               const char *copyfrom_rev_str,
               const char *copyfrom_path,
							 apr_pool_t *pool)
{
	PGresult *res;
	const char *param_values[7];
  int id_fnum;

	param_values[0] = apr_psprintf (pool, "%d", repos_id);
	param_values[1] = apr_psprintf (pool, "%ld", rev);
	param_values[2] = (*path == '/') ? path + 1 : path;
	param_values[3] = kind_str;
	param_values[4] = action_str;
	param_values[5] = copyfrom_rev_str;
	param_values[6] = copyfrom_path;

	res = PQexecParams(conn,
						"SELECT svn.create_node($1, $2, $3, $4, $5, $6, $7) AS id",
						7,
						NULL,
						param_values,
						NULL,
						NULL,
						0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
		PQclear(res);
		return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
									_("Couldn't create node; path '%s', error message '%s'"),
									path, PQerrorMessage(conn));
		}

	id_fnum = PQfnumber(res, "id");

	*node_id = atol(PQgetvalue(res, 0, id_fnum));

	PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_set_node_size(PGconn *conn, const char *path, int node_id, const char *size_str, const char *md5_str, apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[3];

  param_values[0] = apr_psprintf (pool, "%d", node_id);
  param_values[1] = size_str;
  param_values[2] = md5_str;

  res = PQexecParams(conn,
                     "UPDATE svn.node SET size = $2, md5 = $3 WHERE id = $1",
                     3,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't update node; path '%s', error message '%s'"),
                                path, PQerrorMessage(conn));
    }

  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_set_node_props(PGconn *conn, const char *path, int node_id, int props_id, apr_pool_t *pool)
{
  PGresult *res;
  const char *param_values[1];

  param_values[0] = apr_psprintf (pool, "%d", node_id);

  res = PQexecParams(conn,
                     "UPDATE svn.node SET props_id = $1 WHERE id = $1",
                     1,
                     NULL,
                     param_values,
                     NULL,
                     NULL,
                     0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      PQclear(res);
      return svn_error_createf (SVN_ERR_FS_GENERAL, NULL,
                                _("Couldn't update node; path '%s', error message '%s'"),
                                path, PQerrorMessage(conn));
    }

  PQclear(res);

  return SVN_NO_ERROR;
}


svn_error_t *
db_set_node_content(PGconn *conn, const char *path, int node_id, const char *md5_str, const char *content, svn_stream_t *feedback_stream, apr_pool_t *pool)
{

  PGresult *res;
  const char *param_values[3];

  SVN_ERR (db_create_savepoint(conn, "pre_content"));

  param_values[0] = apr_psprintf (pool, "%d", node_id);
  param_values[1] = md5_str;
  param_values[2] = content;

  res = PQexecParams(conn,
                      "SELECT svn.content($1, $2, $3)",
                      3,
                      NULL,
                      param_values,
                      NULL,
                      NULL,
                      0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      SVN_ERR (svn_stream_printf (feedback_stream, pool,
                                  _("WARNING: Couldn't store content; path '%s', error message '%s'\n"),
                                  path, PQerrorMessage(conn)));
      SVN_ERR (db_rollback_to_savepoint(conn, "pre_content"));
    }
  else
    {
      SVN_ERR (db_release_savepoint(conn, "pre_content"));
    }


  PQclear(res);

  return SVN_NO_ERROR;
}
