/*
 * Samba-VirusFilter VFS modules
 *
 * Copyright (C) 2010-2015 SATOH Fumiyasu @ OSS Technology Corp., Japan
 *               <http://www.OSSTech.co.jp/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __VFS_VIRUSFILTER_IMPL_H__
#define __VFS_VIRUSFILTER_IMPL_H__

#include "vfs_virusfilter.h"
#include "vfs_virusfilter_util.h"

#define SVF_MODULE_NAME "virusfilter_" SVF_MODULE_ENGINE

/* Default configuration values
 * ====================================================================== */

#define SVF_DEFAULT_SCAN_ON_OPEN		true
#define SVF_DEFAULT_SCAN_ON_CLOSE		false
#define SVF_DEFAULT_MAX_FILE_SIZE		100000000L /* 100MB */
#define SVF_DEFAULT_MIN_FILE_SIZE		0
#define SVF_DEFAULT_EXCLUDE_FILES		NULL

#define SVF_DEFAULT_CACHE_ENTRY_LIMIT		100
#define SVF_DEFAULT_CACHE_TIME_LIMIT		10

#define SVF_DEFAULT_INFECTED_FILE_ACTION	SVF_ACTION_DO_NOTHING
#define SVF_DEFAULT_INFECTED_FILE_COMMAND	NULL
#define SVF_DEFAULT_INFECTED_FILE_ERRNO_ON_OPEN	EACCES
#define SVF_DEFAULT_INFECTED_FILE_ERRNO_ON_CLOSE 0

#define SVF_DEFAULT_SCAN_ERROR_COMMAND		NULL
#define SVF_DEFAULT_SCAN_ERROR_ERRNO_ON_OPEN	EACCES
#define SVF_DEFAULT_SCAN_ERROR_ERRNO_ON_CLOSE	0
#define SVF_DEFAULT_BLOCK_ACCESS_ON_ERROR	false

#define SVF_DEFAULT_QUARANTINE_PREFIX		"svf."

/* ====================================================================== */

int svf_debug_level = DBGC_VFS;

static const struct enum_list svf_actions[] = {
	{ SVF_ACTION_QUARANTINE,	"quarantine" },
	{ SVF_ACTION_DELETE,		"delete" },
	{ SVF_ACTION_DELETE,		"remove" },	/* alias for "delete" */
	{ SVF_ACTION_DELETE,		"unlink" },	/* alias for "delete" */
	{ SVF_ACTION_DO_NOTHING,	"nothing" },
	{ -1,				NULL}
};

typedef struct {
#ifdef SVF_DEFAULT_SCAN_REQUEST_LIMIT
	int				scan_request_count;
	int				scan_request_limit;
#endif
	/* Scan on file operations */
	bool				scan_on_open;
	bool				scan_on_close;
	/* Special scan options */
#ifdef SVF_DEFAULT_SCAN_ARCHIVE
        bool				scan_archive;
#endif
#ifdef SVF_DEFAULT_MAX_NESTED_SCAN_ARCHIVE
        int				max_nested_scan_archive;
#endif
#ifdef SVF_DEFAULT_SCAN_MIME
        bool				scan_mime;
#endif
	/* Size limit */
	ssize_t				max_file_size;
	ssize_t				min_file_size;
	/* Exclude files */
	name_compare_entry		*exclude_files;
	/* Scan result cache */
	svf_cache_handle		*cache_h;
	int				cache_entry_limit;
	int				cache_time_limit;
	/* Infected file options */
	svf_action			infected_file_action;
	const char *			infected_file_command;
	int				infected_file_errno_on_open;
	int				infected_file_errno_on_close;
	/* Scan error options */
	const char *			scan_error_command;
	int				scan_error_errno_on_open;
	int				scan_error_errno_on_close;
	bool				block_access_on_error;
	/* Quarantine infected files */
	const char *			quarantine_dir;
	const char *			quarantine_prefix;
	/* Network options */
#ifdef SVF_DEFAULT_SOCKET_PATH
        const char *			socket_path;
	svf_io_handle			*io_h;
#endif
	/* Module specific configuration options */
#ifdef SVF_MODULE_CONFIG_MEMBERS
	SVF_MODULE_CONFIG_MEMBERS
#endif
} svf_handle;

/* ====================================================================== */

#ifdef svf_module_connect
static int svf_module_connect(
	vfs_handle_struct *vfs_h,
	svf_handle *svf_h,
	const char *svc,
	const char *user);
#endif

#ifdef svf_module_disconnect
static int svf_module_disconnect(vfs_handle_struct *vfs_h);
#endif

#ifdef svf_module_destruct_config
static int svf_module_destruct_config(svf_handle *svf_h);
#endif

#ifdef svf_module_scan_init
static svf_result svf_module_scan_init(svf_handle *svf_h);
#endif

#ifdef svf_module_scan_end
static void svf_module_scan_end(svf_handle *svf_h);
#endif

static svf_result svf_module_scan(
	vfs_handle_struct *vfs_h,
	svf_handle *svf_h,
	const struct smb_filename *smb_fname,
	const char **reportp);

/* ====================================================================== */

static int svf_destruct_config(svf_handle *svf_h)
{
#ifdef svf_module_destruct_config
	/* FIXME: Check return code */
	svf_module_destruct_config(svf_h);
#endif

	return 0;
}

static int svf_vfs_connect(
	vfs_handle_struct *vfs_h,
	const char *svc,
	const char *user)
{
	int snum = SNUM(vfs_h->conn);
	svf_handle *svf_h;
	const char *exclude_files;
#ifdef SVF_DEFAULT_SOCKET_PATH
	int connect_timeout, io_timeout;
#endif
	char *quarantine_dir_default;

	quarantine_dir_default = talloc_asprintf(talloc_tos(),
		"%s/quarantine", get_dyn_STATEDIR());
	if (!quarantine_dir_default) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		return -1;
	}

	svf_h = talloc_zero(vfs_h, svf_handle);
	if (!svf_h) {
		DEBUG(0, ("talloc_zero failed\n"));
		return -1;
	}
	talloc_set_destructor(svf_h, svf_destruct_config);

	SMB_VFS_HANDLE_SET_DATA(vfs_h,
		svf_h,
		NULL,
		svf_handle,
		return -1);

#ifdef SVF_DEFAULT_SCAN_REQUEST_LIMIT
        svf_h->scan_request_limit = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"scan request limit",
		SVF_DEFAULT_SCAN_REQUEST_LIMIT);
#endif

        svf_h->scan_on_open = lp_parm_bool(
		snum, SVF_MODULE_NAME,
		"scan on open",
		SVF_DEFAULT_SCAN_ON_OPEN);
        svf_h->scan_on_close = lp_parm_bool(
		snum, SVF_MODULE_NAME,
		"scan on close",
		SVF_DEFAULT_SCAN_ON_CLOSE);
#ifdef SVF_DEFAULT_MAX_NESTED_SCAN_ARCHIVE
        svf_h->max_nested_scan_archive = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"max nested scan archive",
		SVF_DEFAULT_MAX_NESTED_SCAN_ARCHIVE);
#endif
#ifdef SVF_DEFAULT_SCAN_ARCHIVE
        svf_h->scan_archive = lp_parm_bool(
		snum, SVF_MODULE_NAME,
		"scan archive",
		SVF_DEFAULT_SCAN_ARCHIVE);
#endif
#ifdef SVF_DEFAULT_MIME_SCAN
        svf_h->scan_mime = lp_parm_bool(
		snum, SVF_MODULE_NAME,
		"scan mime",
		SVF_DEFAULT_SCAN_MIME);
#endif

        svf_h->max_file_size = (ssize_t)lp_parm_ulong(
		snum, SVF_MODULE_NAME,
		"max file size",
		SVF_DEFAULT_MAX_FILE_SIZE);
        svf_h->min_file_size = (ssize_t)lp_parm_ulong(
		snum, SVF_MODULE_NAME,
		"min file size",
		SVF_DEFAULT_MIN_FILE_SIZE);

        exclude_files = lp_parm_const_string(
		snum, SVF_MODULE_NAME,
		"exclude files",
		SVF_DEFAULT_EXCLUDE_FILES);
	if (exclude_files) {
		set_namearray(&svf_h->exclude_files, exclude_files);
	}

        svf_h->cache_entry_limit = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"cache entry limit",
		SVF_DEFAULT_CACHE_ENTRY_LIMIT);
        svf_h->cache_time_limit = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"cache time limit",
		SVF_DEFAULT_CACHE_TIME_LIMIT);

        svf_h->infected_file_action = lp_parm_enum(
		snum, SVF_MODULE_NAME,
		"infected file action", svf_actions,
		SVF_DEFAULT_INFECTED_FILE_ACTION);
        svf_h->infected_file_command = lp_parm_const_string(
		snum, SVF_MODULE_NAME,
		"infected file command",
		SVF_DEFAULT_INFECTED_FILE_COMMAND);
        svf_h->scan_error_command = lp_parm_const_string(
		snum, SVF_MODULE_NAME,
		"scan error command",
		SVF_DEFAULT_SCAN_ERROR_COMMAND);
        svf_h->block_access_on_error = lp_parm_bool(
		snum, SVF_MODULE_NAME,
		"block access on error",
		SVF_DEFAULT_BLOCK_ACCESS_ON_ERROR);

        svf_h->quarantine_dir = lp_parm_const_string(
		snum, SVF_MODULE_NAME,
		"quarantine directory",
		quarantine_dir_default);
        svf_h->quarantine_prefix = lp_parm_const_string(
		snum, SVF_MODULE_NAME,
		"quarantine prefix",
		SVF_DEFAULT_QUARANTINE_PREFIX);

        svf_h->infected_file_errno_on_open = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"infected file errno on open",
		SVF_DEFAULT_INFECTED_FILE_ERRNO_ON_OPEN);
        svf_h->infected_file_errno_on_close = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"infected file errno on close",
		SVF_DEFAULT_INFECTED_FILE_ERRNO_ON_CLOSE);
        svf_h->scan_error_errno_on_open = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"scan error errno on open",
		SVF_DEFAULT_SCAN_ERROR_ERRNO_ON_OPEN);
        svf_h->scan_error_errno_on_close = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"scan error errno on close",
		SVF_DEFAULT_SCAN_ERROR_ERRNO_ON_CLOSE);

#ifdef SVF_DEFAULT_SOCKET_PATH
        svf_h->socket_path = lp_parm_const_string(
		snum, SVF_MODULE_NAME,
		"socket path",
		SVF_DEFAULT_SOCKET_PATH);
        connect_timeout = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"connect timeout",
		SVF_DEFAULT_CONNECT_TIMEOUT);
        io_timeout = lp_parm_int(
		snum, SVF_MODULE_NAME,
		"io timeout",
		SVF_DEFAULT_TIMEOUT);

	svf_h->io_h = svf_io_new(svf_h, connect_timeout, io_timeout);
	if (!svf_h->io_h) {
		DEBUG(0,("svf_io_new failed"));
		return -1;
	}
#endif

	if (svf_h->cache_entry_limit > 0) {
		svf_h->cache_h = svf_cache_new(vfs_h,
			svf_h->cache_entry_limit, svf_h->cache_time_limit);
		if (!svf_h->cache_h) {
			DEBUG(0,("Initializing cache failed: Cache disabled"));
		}
	}

#ifdef svf_module_connect
	if (svf_module_connect(vfs_h, svf_h, svc, user) == -1) {
		return -1;
	}
#endif

	return SMB_VFS_NEXT_CONNECT(vfs_h, svc, user);
}

static void svf_vfs_disconnect(vfs_handle_struct *vfs_h)
{
	svf_handle *svf_h;

#ifdef svf_module_disconnect
	svf_module_disconnect(vfs_h);
#endif

	SMB_VFS_HANDLE_GET_DATA(vfs_h, svf_h,
				svf_handle,
				return);

	free_namearray(svf_h->exclude_files);
#ifdef SVF_DEFAULT_SOCKET_PATH
	svf_io_disconnect(svf_h->io_h);
#endif

	SMB_VFS_NEXT_DISCONNECT(vfs_h);
}

static int svf_set_module_env(svf_env_struct *env_h)
{
#if 0
	/* FIXME */
	if (svf_env_set(env_h, SVF_ENV_PREFIX "VERSION", SVF_VERSION) == -1) {
		return -1;
	}
#endif
	if (svf_env_set(env_h, SVF_ENV_PREFIX "MODULE_NAME", SVF_MODULE_NAME) == -1) {
		return -1;
	}
#ifdef SVF_MODULE_VERSION
	if (svf_env_set(env_h, SVF_ENV_PREFIX "MODULE_VERSION", SVF_MODULE_VERSION) == -1) {
		return -1;
	}
#endif

	return 0;
}

static svf_action svf_do_infected_file_action(
	vfs_handle_struct *vfs_h,
	svf_handle *svf_h,
	const struct smb_filename *smb_fname,
	const char **filepath_newp)
{
	TALLOC_CTX *mem_ctx = talloc_tos();
	connection_struct *conn = vfs_h->conn;
	struct smb_filename *q_smb_fname = NULL;
	char *q_dir;
	char *q_prefix;
	char *q_filepath;
	int q_fd;

	*filepath_newp = NULL;

	switch (svf_h->infected_file_action) {
	case SVF_ACTION_QUARANTINE:
		/* FIXME: Do SMB_VFS_NEXT_MKDIR(svf_h->quarantine_dir) hierarchy */
		q_dir = svf_string_sub(mem_ctx, conn, svf_h->quarantine_dir);
		q_prefix = svf_string_sub(mem_ctx, conn, svf_h->quarantine_prefix);
		if (!q_dir || !q_prefix) {
			DEBUG(0,("Quarantine failed: %s/%s: "
				"Cannot allocate memory\n",
				conn->connectpath,
				smb_fname->base_name));
			return SVF_ACTION_DO_NOTHING;
		}
		q_filepath = talloc_asprintf(talloc_tos(), "%s/%sXXXXXX", q_dir, q_prefix);
		TALLOC_FREE(q_dir);
		TALLOC_FREE(q_prefix);
		if (!q_filepath) {
			DEBUG(0,("Quarantine failed: %s/%s: "
				"Cannot allocate memory\n",
				conn->connectpath,
				smb_fname->base_name));
			return SVF_ACTION_DO_NOTHING;
		}

		become_root();

		q_fd = mkstemp(q_filepath);
		if (q_fd == -1) {
			unbecome_root();
			DEBUG(0,("Quarantine failed: %s/%s: "
				"Cannot open destination: %s: %s\n",
				conn->connectpath,
				smb_fname->base_name,
				q_filepath, strerror(errno)));
			return SVF_ACTION_DO_NOTHING;
		}
		close(q_fd);

		q_smb_fname = synthetic_smb_fname(mem_ctx, q_filepath, smb_fname->stream_name, NULL);
		if (q_smb_fname == NULL) {
			unlink(q_filepath);
			unbecome_root();
			return SVF_ACTION_DO_NOTHING;
		}

		if (svf_vfs_next_move(vfs_h, smb_fname, q_smb_fname) == -1) {
			unbecome_root();
			DEBUG(0,("Quarantine failed: %s/%s: Rename failed: %s\n",
				conn->connectpath,
				smb_fname->base_name,
				strerror(errno)));
			return SVF_ACTION_DO_NOTHING;
		}
		unbecome_root();

		*filepath_newp = q_filepath;

		return SVF_ACTION_QUARANTINE;

	case SVF_ACTION_DELETE:
		become_root();
		if (SMB_VFS_NEXT_UNLINK(vfs_h, smb_fname) == -1) {
			unbecome_root();
			DEBUG(0,("Delete failed: %s/%s: Unlink failed: %s\n",
				conn->connectpath,
				smb_fname->base_name,
				strerror(errno)));
			return SVF_ACTION_DO_NOTHING;
		}
		unbecome_root();
		return SVF_ACTION_DELETE;

	case SVF_ACTION_DO_NOTHING:
	default:
		return SVF_ACTION_DO_NOTHING;
	}
}

static svf_action svf_treat_infected_file(
	vfs_handle_struct *vfs_h,
	svf_handle *svf_h,
	const struct smb_filename *smb_fname,
	const char *report,
	bool is_cache)
{
	connection_struct *conn = vfs_h->conn;
	TALLOC_CTX *mem_ctx = talloc_tos();
	int i;
	svf_action action;
	const char *action_name = "UNKNOWN";
	const char *filepath_q = NULL;
	svf_env_struct *env_h = NULL;
	char *command = NULL;
	int command_result;

	action = svf_do_infected_file_action(vfs_h, svf_h, smb_fname, &filepath_q);
	for (i=0; svf_actions[i].name; i++) {
		if (svf_actions[i].value == action) {
			action_name = svf_actions[i].name;
			break;
		}
	}
	DEBUG(1,("Infected file action: %s/%s: %s\n",
		vfs_h->conn->connectpath,
		smb_fname->base_name,
		action_name));

	if (!svf_h->infected_file_command) {
		return action;
	}

	env_h = svf_env_new(mem_ctx);
	if (!env_h) {
		DEBUG(0,("svf_env_new failed\n"));
		goto done;
	}
	if (svf_set_module_env(env_h) == -1) {
		goto done;
	}
	if (svf_env_set(env_h, SVF_ENV_PREFIX "INFECTED_SERVICE_FILE_PATH", smb_fname->base_name) == -1) {
		goto done;
	}
	if (report && svf_env_set(env_h, SVF_ENV_PREFIX "INFECTED_FILE_REPORT", report) == -1) {
		goto done;
	}
	if (svf_env_set(env_h, SVF_ENV_PREFIX "INFECTED_FILE_ACTION", action_name) == -1) {
		goto done;
	}
	if (filepath_q && svf_env_set(env_h, SVF_ENV_PREFIX "QUARANTINED_FILE_PATH", filepath_q) == -1) {
		goto done;
	}
	if (is_cache && svf_env_set(env_h, SVF_ENV_PREFIX "RESULT_IS_CACHE", "yes") == -1) {
		goto done;
	}

	command = svf_string_sub(mem_ctx, conn, svf_h->infected_file_command);
	if (!command) {
		DEBUG(0,("svf_string_sub failed\n"));
		goto done;
	}

	DEBUG(3,("Infected file command line: %s/%s: %s\n",
		vfs_h->conn->connectpath,
		smb_fname->base_name,
		command));

	command_result = svf_shell_run(command, 0, 0, env_h, vfs_h->conn, true);
	if (command_result != 0) {
		DEBUG(0,("Infected file command failed: %d\n", command_result));
	}

	DEBUG(10,("Infected file command finished: %d\n", command_result));

done:
	TALLOC_FREE(env_h);
	TALLOC_FREE(command);

	return action;
}

static void svf_treat_scan_error(
	vfs_handle_struct *vfs_h,
	svf_handle *svf_h,
	const struct smb_filename *smb_fname,
	const char *report,
	bool is_cache)
{
	connection_struct *conn = vfs_h->conn;
	TALLOC_CTX *mem_ctx = talloc_tos();
	svf_env_struct *env_h = NULL;
	char *command = NULL;
	int command_result;

	if (!svf_h->scan_error_command) {
		return;
	}

	env_h = svf_env_new(mem_ctx);
	if (!env_h) {
		DEBUG(0,("svf_env_new failed\n"));
		goto done;
	}
	if (svf_set_module_env(env_h) == -1) {
		goto done;
	}
	if (svf_env_set(env_h, SVF_ENV_PREFIX "SCAN_ERROR_SERVICE_FILE_PATH", smb_fname->base_name) == -1) {
		goto done;
	}
	if (report && svf_env_set(env_h, SVF_ENV_PREFIX "SCAN_ERROR_REPORT", report) == -1) {
		goto done;
	}
	if (is_cache && svf_env_set(env_h, SVF_ENV_PREFIX "RESULT_IS_CACHE", "1") == -1) {
		goto done;
	}

	command = svf_string_sub(mem_ctx, conn, svf_h->scan_error_command);
	if (!command) {
		DEBUG(0,("svf_string_sub failed\n"));
		goto done;
	}

	DEBUG(3,("Scan error command line: %s/%s: %s\n",
		vfs_h->conn->connectpath,
		smb_fname->base_name,
		command));

	command_result = svf_shell_run(command, 0, 0, env_h, vfs_h->conn, true);
	if (command_result != 0) {
		DEBUG(0,("Scan error command failed: %d\n", command_result));
	}

done:
	TALLOC_FREE(env_h);
	TALLOC_FREE(command);
}

static svf_result svf_scan(
	vfs_handle_struct *vfs_h,
	svf_handle *svf_h,
	const struct smb_filename *smb_fname)
{
	svf_result scan_result;
	const char *scan_report = NULL;
	char *fname = smb_fname->base_name;
	svf_cache_entry *scan_cache_e = NULL;
	bool is_cache = false;
	svf_action file_action;
	bool add_scan_cache;

	if (svf_h->cache_h) {
		DEBUG(10, ("Searching cache entry: fname: %s\n", fname));
		scan_cache_e = svf_cache_get(svf_h->cache_h, fname, -1);
		if (scan_cache_e) {
			DEBUG(10, ("Cache entry found: cached result: %d\n", scan_cache_e->result));
			is_cache = true;
			scan_result = scan_cache_e->result;
			scan_report = scan_cache_e->report;
			goto svf_scan_result_eval;
		}
		DEBUG(10, ("Cache entry not found\n"));
	}

#ifdef svf_module_scan_init
	if (svf_module_scan_init(svf_h) != SVF_RESULT_OK) {
		scan_result = SVF_RESULT_ERROR;
		scan_report = "Initializing scanner failed";
		goto svf_scan_result_eval;
	}
#endif

	scan_result = svf_module_scan(vfs_h, svf_h, smb_fname, &scan_report);

#ifdef svf_module_scan_end
#ifdef SVF_DEFAULT_SCAN_REQUEST_LIMIT
	if (svf_h->scan_request_limit > 0) {
		svf_h->scan_request_count++;
		if (svf_h->scan_request_count >= svf_h->scan_request_limit) {
			svf_module_scan_end(svf_h);
			svf_h->scan_request_count = 0;
		}
	}
#else
	svf_module_scan_end(svf_h);
#endif
#endif

svf_scan_result_eval:

	file_action = SVF_ACTION_DO_NOTHING;
	add_scan_cache = true;

	switch (scan_result) {
	case SVF_RESULT_CLEAN:
		DEBUG(5, ("Scan result: Clean: %s/%s\n",
			vfs_h->conn->connectpath,
			fname));
		break;
	case SVF_RESULT_INFECTED:
		DEBUG(0, ("Scan result: Infected: %s/%s: %s\n",
			vfs_h->conn->connectpath,
			fname,
			scan_report));
		file_action = svf_treat_infected_file(vfs_h, svf_h, smb_fname, scan_report, is_cache);
		if (file_action != SVF_ACTION_DO_NOTHING) {
			add_scan_cache = false;
		}
		break;
	case SVF_RESULT_ERROR:
		DEBUG(0, ("Scan result: Error: %s/%s: %s\n",
			vfs_h->conn->connectpath,
			fname,
			scan_report));
		svf_treat_scan_error(vfs_h, svf_h, smb_fname, scan_report, is_cache);
		break;
	default:
		DEBUG(0, ("Scan result: Unknown result code %d: %s/%s: %s\n",
			scan_result,
			vfs_h->conn->connectpath,
			fname,
			scan_report));
		svf_treat_scan_error(vfs_h, svf_h, smb_fname, scan_report, is_cache);
		break;
	}

	if (svf_h->cache_h && !is_cache && add_scan_cache) {
		DEBUG(10, ("Adding new cache entry: %s, %d\n", fname, scan_result));
		scan_cache_e = svf_cache_entry_new(svf_h->cache_h, fname, -1);
		if (!scan_cache_e) {
			DEBUG(0,("Cannot create cache entry: svf_cache_entry_new failed"));
			goto svf_scan_return;
		}
		scan_cache_e->result = scan_result;
		if (scan_report) {
			scan_cache_e->report = talloc_strdup(scan_cache_e, scan_report);
			if (!scan_cache_e->report) {
				DEBUG(0,("Cannot create cache entry: talloc_strdup failed"));
				svf_cache_entry_free(scan_cache_e);
				goto svf_scan_return;
			}
		} else {
			scan_cache_e->report = NULL;
		}

		svf_cache_add(svf_h->cache_h, scan_cache_e);
	}

svf_scan_return:

	return scan_result;
}

static int svf_vfs_open(
	vfs_handle_struct *vfs_h,
	struct smb_filename *smb_fname,
	files_struct *fsp,
	int flags, mode_t mode)
{
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	svf_handle *svf_h;
	svf_result scan_result;
	char *fname = smb_fname->base_name;
	int scan_errno = 0;

	SMB_VFS_HANDLE_GET_DATA(vfs_h, svf_h,
				svf_handle,
				return -1);

        if (!svf_h->scan_on_open) {
                DEBUG(5, ("Not scanned: scan on open is disabled: %s/%s\n",
			vfs_h->conn->connectpath, fname));
		goto svf_vfs_open_next;
        }

	if (flags & O_TRUNC) {
                DEBUG(5, ("Not scanned: Open flags have O_TRUNC: %s/%s\n",
			vfs_h->conn->connectpath, fname));
		goto svf_vfs_open_next;
	}

	if (SMB_VFS_NEXT_STAT(vfs_h, smb_fname) != 0) {
		/* FIXME: Return immediately if !(flags & O_CREAT) && errno != ENOENT? */
		goto svf_vfs_open_next;
	}
	if (!S_ISREG(smb_fname->st.st_ex_mode)) {
                DEBUG(5, ("Not scanned: Directory or special file: %s/%s\n",
			vfs_h->conn->connectpath, fname));
		goto svf_vfs_open_next;
	}
	if (svf_h->max_file_size > 0 && smb_fname->st.st_ex_size > svf_h->max_file_size) {
                DEBUG(5, ("Not scanned: file size > max file size: %s/%s\n",
			vfs_h->conn->connectpath, fname));
		goto svf_vfs_open_next;
	}
	if (svf_h->min_file_size > 0 && smb_fname->st.st_ex_size < svf_h->min_file_size) {
                DEBUG(5, ("Not scanned: file size < min file size: %s/%s\n",
			vfs_h->conn->connectpath, fname));
		goto svf_vfs_open_next;
	}

	if (svf_h->exclude_files && is_in_path(fname, svf_h->exclude_files, false)) {
                DEBUG(5, ("Not scanned: exclude files: %s/%s\n",
			vfs_h->conn->connectpath, fname));
		goto svf_vfs_open_next;
	}

	scan_result = svf_scan(vfs_h, svf_h, smb_fname);

	switch (scan_result) {
	case SVF_RESULT_CLEAN:
		break;
	case SVF_RESULT_INFECTED:
		scan_errno = svf_h->infected_file_errno_on_open;
		goto svf_vfs_open_fail;
	case SVF_RESULT_ERROR:
		if (svf_h->block_access_on_error) {
			DEBUG(5, ("Block access\n"));
			scan_errno = svf_h->scan_error_errno_on_open;
			goto svf_vfs_open_fail;
		}
		break;
	default:
		scan_errno = svf_h->scan_error_errno_on_open;
		goto svf_vfs_open_fail;
	}

svf_vfs_open_next:
	TALLOC_FREE(mem_ctx);
	return SMB_VFS_NEXT_OPEN(vfs_h, smb_fname, fsp, flags, mode);

svf_vfs_open_fail:
	TALLOC_FREE(mem_ctx);
	errno = (scan_errno != 0) ? scan_errno : EACCES;
	return -1;
}

static int svf_vfs_close(
	vfs_handle_struct *vfs_h,
	files_struct *fsp)
{
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	connection_struct *conn = vfs_h->conn;
	svf_handle *svf_h;
	char *fname = fsp->fsp_name->base_name;
	int close_result, close_errno;
	svf_result scan_result;
	int scan_errno = 0;

	SMB_VFS_HANDLE_GET_DATA(vfs_h, svf_h,
				svf_handle,
				return -1);

	/* FIXME: Must close after scan? */
	close_result = SMB_VFS_NEXT_CLOSE(vfs_h, fsp);
	close_errno = errno;
	/* FIXME: Return immediately if errno_result == -1, and close_errno == EBADF or ...? */

	if (fsp->is_directory) {
                DEBUG(5, ("Not scanned: Directory: %s/%s\n",
			conn->connectpath, fname));
		TALLOC_FREE(mem_ctx);
		return close_result;
	}

	if (!svf_h->scan_on_close) {
                DEBUG(5, ("Not scanned: scan on close is disabled: %s/%s\n",
			conn->connectpath, fname));
		TALLOC_FREE(mem_ctx);
		return close_result;
	}

	if (!fsp->modified) {
		DEBUG(3, ("Not scanned: File not modified: %s/%s\n",
			conn->connectpath, fname));

		TALLOC_FREE(mem_ctx);
		return close_result;
	}

	if (svf_h->exclude_files && is_in_path(fname, svf_h->exclude_files, false)) {
                DEBUG(5, ("Not scanned: exclude files: %s/%s\n",
			conn->connectpath, fname));
		TALLOC_FREE(mem_ctx);
		return close_result;
	}

	scan_result = svf_scan(vfs_h, svf_h, fsp->fsp_name);

	switch (scan_result) {
	case SVF_RESULT_CLEAN:
		break;
	case SVF_RESULT_INFECTED:
		scan_errno = svf_h->infected_file_errno_on_close;
		goto svf_vfs_close_fail;
	case SVF_RESULT_ERROR:
		if (svf_h->block_access_on_error) {
			DEBUG(5, ("Block access\n"));
			scan_errno = svf_h->scan_error_errno_on_close;
			goto svf_vfs_close_fail;
		}
		break;
	default:
		scan_errno = svf_h->scan_error_errno_on_close;
		goto svf_vfs_close_fail;
	}

	TALLOC_FREE(mem_ctx);
	errno = close_errno;

	return close_result;

svf_vfs_close_fail:

	TALLOC_FREE(mem_ctx);
	errno = (scan_errno != 0) ? scan_errno : close_errno;

	return close_result;
}

static int svf_vfs_unlink(
	vfs_handle_struct *vfs_h,
	const struct smb_filename *smb_fname)
{
	int ret = SMB_VFS_NEXT_UNLINK(vfs_h, smb_fname);
	svf_handle *svf_h;
	char *fname;
	svf_cache_entry *scan_cache_e;

	if (ret != 0 && errno != ENOENT) {
		return ret;
	}

	SMB_VFS_HANDLE_GET_DATA(vfs_h, svf_h,
				svf_handle,
				return -1);

	if (svf_h->cache_h) {
		fname = smb_fname->base_name;
		DEBUG(10, ("Searching cache entry: fname: %s\n", fname));
		scan_cache_e = svf_cache_get(svf_h->cache_h, fname, -1);
		if (scan_cache_e) {
			svf_cache_remove(svf_h->cache_h, scan_cache_e);
			svf_cache_entry_free(scan_cache_e);
		}
	}

	return ret;
}

static int svf_vfs_rename(
	vfs_handle_struct *vfs_h,
	const struct smb_filename *smb_fname_src,
	const struct smb_filename *smb_fname_dst)
{
	int ret = SMB_VFS_NEXT_RENAME(vfs_h, smb_fname_src, smb_fname_dst);
	svf_handle *svf_h;
	char *fname;
	svf_cache_entry *scan_cache_e;

	if (ret != 0) {
		return ret;
	}

	SMB_VFS_HANDLE_GET_DATA(vfs_h, svf_h,
				svf_handle,
				return -1);

	if (svf_h->cache_h) {
		fname = smb_fname_dst->base_name;
		DEBUG(10, ("Searching cache entry: fname: %s\n", fname));
		scan_cache_e = svf_cache_get(svf_h->cache_h, fname, -1);
		if (scan_cache_e) {
			svf_cache_remove(svf_h->cache_h, scan_cache_e);
			svf_cache_entry_free(scan_cache_e);
		}

		fname = smb_fname_src->base_name;
		DEBUG(10, ("Searching cache entry: fname: %s\n", fname));
		scan_cache_e = svf_cache_get(svf_h->cache_h, fname, -1);
		if (scan_cache_e) {
			if (!svf_cache_entry_rename(scan_cache_e, smb_fname_dst->base_name, -1)) {
				DEBUG(0,("Cannot rename cache entry: svf_cache_entry_rename failed"));
				svf_cache_remove(svf_h->cache_h, scan_cache_e);
				svf_cache_entry_free(scan_cache_e);
			}
		}
	}

	return ret;
}

/* VFS operations */
static struct vfs_fn_pointers vfs_svf_fns = {
	.connect_fn =	svf_vfs_connect,
	.disconnect_fn =svf_vfs_disconnect,
	.open_fn =	svf_vfs_open,
	.close_fn =	svf_vfs_close,
	.unlink_fn =	svf_vfs_unlink,
	.rename_fn =	svf_vfs_rename,
};

NTSTATUS samba_init_module(void)
{
	NTSTATUS ret;

	ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				SVF_MODULE_NAME, &vfs_svf_fns);
	if (!NT_STATUS_IS_OK(ret)) {
		return ret;
	}

	svf_debug_level = debug_add_class(SVF_MODULE_NAME);
	if (svf_debug_level == -1) {
		svf_debug_level = DBGC_VFS;
		DEBUG(0, ("Couldn't register custom debugging class!\n"));
	} else {
		DEBUG(10, ("Debug class number of '%s': %d\n",
			SVF_MODULE_NAME, svf_debug_level));
	}

	DEBUG(5,("%s registered\n", SVF_MODULE_NAME));

	return ret;
}

#endif /* __VFS_VIRUSFILTER_IMPL_H__ */

