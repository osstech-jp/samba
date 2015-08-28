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

#ifndef __VFS_VIRUSFILTER_H__
#define __VFS_VIRUSFILTER_H__

#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "system/filesys.h"
#include "transfer_file.h"
#include "auth.h"
#include "passdb.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../lib/tsocket/tsocket.h"

/* Samba debug classs for SVF */
#undef DBGC_CLASS
#define DBGC_CLASS svf_debug_level
extern int svf_debug_level;

/* Samba global variable */
extern userdom_struct current_user_info;

/* ====================================================================== */

typedef enum {
	SVF_ACTION_DO_NOTHING,
	SVF_ACTION_QUARANTINE,
	SVF_ACTION_DELETE,
	/* FIXME: SVF_ACTION_RENAME, */
} svf_action;

typedef enum {
	SVF_RESULT_OK,
	SVF_RESULT_CLEAN,
	SVF_RESULT_ERROR,
	SVF_RESULT_INFECTED,
	/* FIXME: SVF_RESULT_SUSPECTED, */
	/* FIXME: SVF_RESULT_RISKWARE, */
} svf_result;

#define conn_session_info(conn)		((conn)->session_info)
#define conn_socket(conn)		((conn)->transport.sock)
#define conn_domain_name(conn)		((conn)->session_info->info->domain_name)
#define conn_client_name(conn)		((conn)->sconn->remote_hostname)
#define conn_client_addr(conn)		tsocket_address_inet_addr_string((conn)->sconn->remote_address, talloc_tos())

#define conn_server_addr(conn)	tsocket_address_inet_addr_string((conn)->sconn->local_address, talloc_tos())

#endif /* __VFS_VIRUSFILTER_H__ */
