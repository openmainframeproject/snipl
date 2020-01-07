/*
 * vmsmapi.h : header file for vm API for snIPL (simple network IPL)
 *
 * Copyright IBM Corp. 2002, 2011
 * Author(s): Peter Tiedemann    (ptiedem@de.ibm.com)
 * Maintainer: Ursula Braun      (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */

/*  +--------------------------------------------------------------+  */
/*                                                                    */
/*  This header file defines some common things for using the         */
/*  vmsmapi which is a small layer above the automatically generated  */
/*  VM System Management API which exploits RPC calls.                */
/*                                                                    */
/*--------------------------------------------------------------------*/

struct snipl_server_private {
	Session_Token   session_token;
	CLIENT         *serverP;
};

static int vm_image_activate(struct snipl_image *);
static int vm_image_deactivate(struct snipl_image *);
static int vm_image_reset(struct snipl_image *);
static int vm_image_getstatus(struct snipl_image *);

static int vm_check(struct snipl_server *);
static int vm_server_login(struct snipl_server *);
static int vm_server_logout(struct snipl_server *);

static struct snipl_server_ops vm_server_ops = {
	.login = vm_server_login,
	.logout = vm_server_logout,
	.check = vm_check,
};

static struct snipl_image_ops vm_image_ops = {
	.reset          = vm_image_reset,
	.activate       = vm_image_activate,
	.deactivate     = vm_image_deactivate,
	.getstatus      = vm_image_getstatus,
};

static int connectServer(struct snipl_server *);
static void rpcError(struct snipl_server *, int);
static inline void free_priv_conn(struct snipl_server_private *);

#define RS_ANY (-1)
#define RCERR_CONNECT (2000)

static struct {
	int RC;
	int RS;
	const char *desc;
} error_codes[] = {
	{ RC_OK,         RS_NONE,               "Request Successful"},
	{ RC_OK,         RS_NOT_ACTIVE,         "Image Not Active"},
	{ RCERR_TOKEN,   RS_NONE,               "Session Token Not Valid"},
	{ RCERR_SYNTAX,  RS_ANY,                "Syntax Error in Function Parameter"},
	{ RCERR_AUTH,    RS_AUTHERR_ESM,        "Request Not Authorized by External Security Manager"},
	{ RCERR_AUTH,    RS_AUTHERR_SERVER,     "Request Not Authorized by External Security Manager"},
	{ RCERR_USER_PW_BAD,    RS_NONE,        "Authentication Error; Userid or Password not valid"},
	{ RCERR_PW_EXPIRED, RS_NONE,            "Authentication Error; Password expired"},
	{ RCERR_DMSCSL,  RS_ANY,                "Internal Server Error"},
	{ RCERR_IMAGEOP, RS_NOT_FOUND,          "Image Not Found"},
	{ RCERR_IMAGEOP, RS_ALREADY_ACTIVE,     "Image Already Active"},
	{ RCERR_IMAGEOP, RS_NOT_ACTIVE,         "Image Not Active"},
	{ RCERR_IMAGEOP, RS_BEING_DEACT,        "Image Being Deactivated"},
	{ RCERR_IMAGEOP, RS_LIST_NOT_FOUND,     "List Not Found"},
	{ RCERR_IMAGEOP, RS_NOT_ALL,            "Some Images In List Not Activated"},
	{ RCERR_IMAGEOP, RS_SOME_NOT_DEACT,     "Some Images In List Not Deactivated"},
	{ RCERR_IMAGEOP, RS_SOME_NOT_RECYC,     "Some Images In List Not Recycled"},
	{ RCERR_CONNECT, RS_ANY,                "Connection Error"},
};

static struct
{
	int fid;
	const char *desc;
} rpc_functions[] = {
	{ VSMAPI_NULL,                        "VSMAPI_NULL" },
	{ LOGIN ,                             "LOGIN" },
	{ LOGOUT ,                            "LOGOUT" },
	{ MODIFY_SESSION_TIMEOUT_INTERVAL ,   "MODIFY_SESSION_TIMEOUT_INTERVAL" },
	{ MODIFY_SERVER_TIMEOUT_INTERVAL ,    "MODIFY_SERVER_TIMEOUT_INTERVAL" },
	{ QUERY_TIMEOUT_INTERVAL ,            "QUERY_TIMEOUT_INTERVAL" },
	{ VSMAPI_DEBUG ,                      "VSMAPI_DEBUG" },
	{ VSMSERVE_SHUTDOWN ,                 "VSMSERVE_SHUTDOWN" },
	{ AUTHORIZATION_LIST_ADD ,            "AUTHORIZATION_LIST_ADD" },
	{ AUTHORIZATION_LIST_REMOVE ,         "AUTHORIZATION_LIST_REMOVE" },
	{ AUTHORIZATION_LIST_QUERY ,          "AUTHORIZATION_LIST_QUERY" },
	{ NAME_LIST_ADD ,                     "NAME_LIST_ADD" },
	{ NAME_LIST_REMOVE ,                  "NAME_LIST_REMOVE" },
	{ NAME_LIST_DESTROY ,                 "NAME_LIST_DESTROY" },
	{ NAME_LIST_QUERY ,                   "NAME_LIST_QUERY" },
	{ STATIC_IMAGE_CHANGES_ACTIVATE ,     "STATIC_IMAGE_CHANGES_ACTIVATE" },
	{ STATIC_IMAGE_CHANGES_DEACTIVATE ,   "STATIC_IMAGE_CHANGES_DEACTIVATE" },
	{ STATIC_IMAGE_CHANGES_IMMEDIATE ,    "STATIC_IMAGE_CHANGES_IMMEDIATE" },
	{ DIRECTORY_MANAGER_COMMAND ,         "DIRECTORY_MANAGER_COMMAND" },
	{ QUERY_DIRECTORY_MANAGER_LEVEL ,     "QUERY_DIRECTORY_MANAGER_LEVEL" },
	{ QUERY_ASYNCHRONOUS_OPERATION ,      "QUERY_ASYNCHRONOUS_OPERATION" },
	{ PROTOTYPE_NAME_QUERY ,              "PROTOTYPE_NAME_QUERY" },
	{ PROTOTYPE_CREATE ,                  "PROTOTYPE_CREATE" },
	{ PROTOTYPE_REPLACE ,                 "PROTOTYPE_REPLACE" },
	{ PROTOTYPE_DELETE ,                  "PROTOTYPE_DELETE" },
	{ PROTOTYPE_QUERY ,                   "PROTOTYPE_QUERY" },
	{ IMAGE_NAME_QUERY ,                  "IMAGE_NAME_QUERY" },
	{ IMAGE_CREATE ,                      "IMAGE_CREATE" },
	{ IMAGE_REPLACE ,                     "IMAGE_REPLACE" },
	{ IMAGE_DELETE ,                      "IMAGE_DELETE" },
	{ IMAGE_QUERY ,                       "IMAGE_QUERY" },
	{ IMAGE_PASSWORD_SET ,                "IMAGE_PASSWORD_SET"},
	{ IMAGE_LOCK ,                        "IMAGE_LOCK" },
	{ IMAGE_UNLOCK ,                      "IMAGE_UNLOCK" },
	{ IMAGE_ACTIVATE ,                    "IMAGE_ACTIVATE" },
	{ IMAGE_DEACTIVATE ,                  "IMAGE_DEACTIVATE" },
	{ IMAGE_RECYCLE ,                     "IMAGE_RECYCLE" },
	{ IMAGE_DEVICE_RESET ,                "IMAGE_DEVICE_RESET" },
	{ IMAGE_STATUS_QUERY ,                "IMAGE_STATUS_QUERY" },
	{ VIRTUAL_NETWORK_LAN_QUERY ,         "VIRTUAL_NETWORK_LAN_QUERY" },
	{ IMAGE_DISK_CREATE ,                 "IMAGE_DISK_CREATE" },
	{ IMAGE_DISK_COPY ,                   "IMAGE_DISK_COPY" },
	{ IMAGE_DISK_SHARE ,                  "IMAGE_DISK_SHARE" },
	{ IMAGE_DISK_DELETE ,                 "IMAGE_DISK_DELETE" },
	{ IMAGE_DISK_UNSHARE ,                "IMAGE_DISK_UNSHARE" },
	{ IMAGE_DEVICE_DEDICATE ,             "IMAGE_DEVICE_DEDICATE" },
	{ IMAGE_DEVICE_UNDEDICATE ,           "IMAGE_DEVICE_UNDEDICATE" },
	{ VIRTUAL_NETWORK_CONNECTION_CREATE , "VIRTUAL_NETWORK_CONNECTION_CREATE" },
	{ VIRTUAL_NETWORK_CONNECTION_DELETE , "VIRTUAL_NETWORK_CONNECTION_DELETE" },
	{ VIRTUAL_NETWORK_ADAPTER_CREATE ,    "VIRTUAL_NETWORK_ADAPTER_CREATE" },
	{ VIRTUAL_NETWORK_ADAPTER_DELETE ,    "VIRTUAL_NETWORK_ADAPTER_DELETE" },
	{ VIRTUAL_NETWORK_LAN_CONNECT ,       "VIRTUAL_NETWORK_LAN_CONNECT" },
	{ VIRTUAL_NETWORK_LAN_DISCONNECT ,    "VIRTUAL_NETWORK_LAN_DISCONNECT" },
	{ VIRTUAL_NETWORK_VSWITCH_CONNECT ,   "VIRTUAL_NETWORK_VSWITCH_CONNECT" },
	{ VIRTUAL_NETWORK_VSWITCH_SET ,       "VIRTUAL_NETWORK_VSWITCH_SET" },
	{ VIRTUAL_NETWORK_VSWITCH_DISCONNECT, "VIRTUAL_NETWORK_VSWITCH_DISCONNECT" },
	{ VIRTUAL_NETWORK_VSWITCH_QUERY ,     "VIRTUAL_NETWORK_VSWITCH_QUERY" },
	{ SHARED_STORAGE_CREATE ,             "SHARED_STORAGE_CREATE" },
	{ SHARED_STORAGE_REPLACE ,            "SHARED_STORAGE_REPLACE" },
	{ SHARED_STORAGE_DELETE ,             "SHARED_STORAGE_DELETE" },
	{ SHARED_STORAGE_QUERY ,              "SHARED_STORAGE_QUERY" },
	{ SHARED_STORAGE_ACCESS_ADD ,         "SHARED_STORAGE_ACCESS_ADD" },
	{ SHARED_STORAGE_ACCESS_REMOVE ,      "SHARED_STORAGE_ACCESS_REMOVE" },
	{ SHARED_STORAGE_ACCESS_QUERY ,       "SHARED_STORAGE_ACCESS_QUERY" },
};

static const char *image_active = "Image Active";
