/*
 * vmsmapi6.h : header file for vm API for snIPL (simple network IPL)
 *
 * Copyright IBM Corp. 2011, 2016
 * Author(s): Ursula Braun    (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */


#define STIMEOUT 20000
#define INPUT_LEN 100

enum socket_state {
	NOT_CREATED,
	ALLOCATED,
	CONNECTED,
};

struct snipl_server_private {	/* vm server private data */
	int	sockid;
	int	sock_state;
	char	*inlist;
	char	*outlist;
	SSL     *sslhandle;
	SSL_CTX *sslcontext;
};

static int vm6_image_activate(struct snipl_image *);
static int vm6_image_deactivate(struct snipl_image *);
static int vm6_image_reset(struct snipl_image *);
static int vm6_image_getstatus(struct snipl_image *);

static int vm6_check(struct snipl_server *);
static int vm6_server_login(struct snipl_server *);
static int vm6_server_logout(struct snipl_server *);
static int vm6_confirm(struct snipl_server *);

static struct snipl_server_ops vm6_server_ops = {
	.login = vm6_server_login,
	.logout = vm6_server_logout,
	.check = vm6_check,
	.confirm = vm6_confirm,
};

static struct snipl_image_ops vm6_image_ops = {
	.reset          = vm6_image_reset,
	.activate       = vm6_image_activate,
	.deactivate     = vm6_image_deactivate,
	.getstatus      = vm6_image_getstatus,
};

extern int vm6_prepare(struct snipl_server *server);

#define RC_OK					0
#define RCERR_SYNTAX				24
#define RCERR_AUTH				100
#define RCERR_USER_PW_BAD			120
#define RCERR_PW_EXPIRED			128
#define RCERR_IMAGEOP				200
#define RCERR_SERVER				900

#define RS_ANY					(-1)
#define RS_NONE					0
#define RS_AUTHERR_ESM				8
#define RS_AUTHERR_SERVER			16
#define RS_NOT_FOUND				4
#define RS_ALREADY_ACTIVE			8
#define RS_NOT_ACTIVE				12
#define RS_BEING_DEACT				16
#define RS_LIST_NOT_FOUND			24
#define RS_NOT_ALL				28
#define RS_SOME_NOT_DEACT			32
#define RS_SOME_NOT_RECYC			36
#define RS_SFS_ERROR				24
#define RS_DEACT_TIME				300

static struct {
	int RC;
	int RS;
	const char *desc;
} error_codes[] = {
	{ RC_OK,         RS_NONE,               "Request Successful"},
	{ RC_OK,         RS_DEACT_TIME,         "Request Successful"},
	{ RC_OK,         RS_NOT_ACTIVE,         "Image Not Active"},
	{ RCERR_SYNTAX,  RS_ANY,                "Syntax Error in Function Parameter"},
	{ RCERR_AUTH,    RS_AUTHERR_ESM,        "Request Not Authorized by External Security Manager"},
	{ RCERR_AUTH,    RS_AUTHERR_SERVER,     "Request Not Authorized by Server"},
	{ RCERR_USER_PW_BAD, RS_NONE,           "Authentication Error; Userid or Password not valid"},
	{ RCERR_PW_EXPIRED, RS_NONE,            "Authentication Error; Password expired"},
	{ RCERR_IMAGEOP, RS_NOT_FOUND,          "Image Not Found"},
	{ RCERR_IMAGEOP, RS_ALREADY_ACTIVE,     "Image Already Active"},
	{ RCERR_IMAGEOP, RS_NOT_ACTIVE,         "Image Not Active"},
	{ RCERR_IMAGEOP, RS_BEING_DEACT,        "Image Being Deactivated"},
	{ RCERR_IMAGEOP, RS_LIST_NOT_FOUND,     "List Not Found"},
	{ RCERR_IMAGEOP, RS_NOT_ALL,            "Some Images In List Not Activated"},
	{ RCERR_IMAGEOP, RS_SOME_NOT_DEACT,     "Some Images In List Not Deactivated"},
	{ RCERR_IMAGEOP, RS_SOME_NOT_RECYC,     "Some Images In List Not Recycled"},
};

static const char *image_active = "Image Active";

struct vm6_image_response {
	int32_t output_length;
	int32_t request_id;
	int32_t return_code;
	int32_t reason_code;
	int32_t processed;
	int32_t not_processed;
	int32_t failing_array_length;
};

