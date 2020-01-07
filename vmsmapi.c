/*
 * vmsmapi.c : vm API for snIPL (simple network IPL)
 *             for RPC-based SMAPI environment
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

/*--------------------------------------------------------------------*/
/*  API to VM System Management API via RPC                           */
/*                                                                    */
/*  This C routine implements the stonith API to shutdown a zVM LINUX */
/*  system. Eventually sending the vmsmapi_imageDeactivate of the VM  */
/*  System Management API which is accessed via RPC calls.            */
/*                                                                    */
/*  Each VM SM API routine other than login requires a SessionToken.  */
/*  The SessionToken is initially returned by a call to vmsmapi_login */
/*  and furtheron each VM SM API function call returns a new          */
/*  SessionToken. The SessionToken is saved to an instance of the     */
/*  static structure snipl_server_private.                            */
/*--------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <ctype.h>
#include "dmsvsma.h"
#include "snipl.h"
#include "vmsmapi.h"


const char *vmsmapi_get_error_description(int rc, int rs)
{
	unsigned int i;
	char *err;

	for (i=0; i < DIMOF(error_codes); i++) {
		if (error_codes[i].RC != rc)
			continue;
		if (error_codes[i].RS == rs || error_codes[i].RS == RS_ANY)
			return error_codes[i].desc;
	}
	err = strdup("");
	if (asprintf(&err, "VM rc/rs = %d/%d", rc, rs) == -1)
		err = NULL;

	return err;
}


static const char *get_rpc_function(int fid)
{
	unsigned int i;

	for (i=0; i < DIMOF(rpc_functions); i++)
	{
		if (rpc_functions[i].fid == fid)
			return rpc_functions[i].desc;
		else continue;
	}

	return "unknown function";
}


extern void prepare(struct snipl_server *server)
{
	struct snipl_image * image;

	DEBUG_PRINT("vmsmapi : start of function\n");

	server->priv = calloc(1, sizeof(struct snipl_server_private));
	if (server->priv == NULL)
		return;

	server->ops = &vm_server_ops;
	// for each image in server set ops:
	snipl_for_each_image(server, image)
		image->ops = &vm_image_ops;

	return;
}

static int vm_check(struct snipl_server *server)
{
	DEBUG_PRINT("vmsmapi : start of function\n");

	if (server->address == NULL)
		return MISSING_SERVER;

	return parms_check_vm(server);
}

/*--------------------------------------------------------------------*/
static int connectServer(struct snipl_server *server)
{
	char *server_name = server->address;
	int rc;

	DEBUG_PRINT("vmsmapi : start of function\n");

	free_priv_conn(server->priv);
	server->priv->serverP = clnt_create(server_name,
					    VSMAPI_PROGRAM,
					    VSMAPI_V2,
					    "tcp");
	if (server->priv->serverP == NULL) {
		clnt_pcreateerror(server_name);
		rc = RCERR_CONNECT;
		server->problem = strdup("connection error\n");
		server->problem_class = FATAL;
	} else {
		/* server->priv->serverP already contains */
		/* pointer to connection data */
		rc = RC_OK;
	}
	return rc;
} /* connectServer() */


/*--------------------------------------------------------------------*/
Return_Code vm_image_activate(struct snipl_image *image)
{
	int rc;
	int rs;
	IMAGEACTIVATE_args ia_args;
	IMAGEACTIVATE_res *ia_res = 0;
	IMAGEACTIVATE_Return_Buffer *image_retbuf;
	Failing_Images_List *filP;
	const char *err_dsc;

	DEBUG_PRINT("vmsmapi : start of function\n");

	/* obtain session token */
	memcpy(
		ia_args.SessionToken,
		image->server->priv->session_token,
		sizeof(Session_Token));
	ia_args.TargetIdentifier = image->name;
	ia_res = image_activate_1(&ia_args,image->server->priv->serverP);
	if (!ia_res) {
		rpcError(image->server, IMAGE_ACTIVATE);
		return 396;
	}

	rc = ia_res->rc;
	if (rc == RCERR_IMAGEOP){
		rs = ia_res->IMAGEACTIVATE_res_u.resfailbuf.rs;

		if (rs == RS_NOT_ALL){
			image_retbuf = &ia_res->
					IMAGEACTIVATE_res_u.resfailbuf.
					IMAGEACTIVATE_resfail_buffer_u.
					resfail_buf.ReturnBuffer;
			create_msg(image->server, "* NumImagesActivated=%d"
				   " NumImagesNotActivated=%d\n",
				   image_retbuf->NumImagesActivated,
				   image_retbuf->NumImagesNotActivated);
			image->server->problem_class = WARNING;
			filP = image_retbuf->FailingImagesList;
			for (; filP; filP = filP->next_entry) {
				create_msg(image->server, "%s * %s rc=%d "
					   "reason=%d\n",
					   image->server->problem,
					   filP->ImageName,
					   filP->ImageReturnCode,
					   filP->ImageReasonCode);
			}
			/* save session token */
			memcpy(image->server->priv->session_token,
			       ia_res->IMAGEACTIVATE_res_u.resfailbuf.
				       IMAGEACTIVATE_resfail_buffer_u.
				       resfail_buf.SessionToken,
			       sizeof(Session_Token));
		} else {
			err_dsc = vmsmapi_get_error_description(rc, rs);
			create_msg(image->server,
				   "* ImageActivate : Image %s %s\n",
				   image->name, err_dsc);
			image->server->problem_class = FATAL;
			/* save session token */
			memcpy(image->server->priv->session_token,
			       ia_res->IMAGEACTIVATE_res_u.resfailbuf.
				       IMAGEACTIVATE_resfail_buffer_u.
				       resfail_nobuf.SessionToken,
			       sizeof(Session_Token));
		}
	} else if (rc) {	/* all other bad cases */
		rs = ia_res->IMAGEACTIVATE_res_u.resfail.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageActivate : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = FATAL;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       ia_res->IMAGEACTIVATE_res_u.resfail.SessionToken,
		       sizeof(Session_Token));

	} else {	/* ok case */
		rs = ia_res->IMAGEACTIVATE_res_u.resok.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageActivate : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = OK;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       ia_res->IMAGEACTIVATE_res_u.resok.SessionToken,
		       sizeof(Session_Token));
	}
	return rc;
} /* vmsmapi_imageActivate() */

/*--------------------------------------------------------------------*/
Return_Code vm_image_reset(struct snipl_image *image)
{
	int rc;
	int rs;
	IMAGERECYCLE_args ir_args;
	IMAGERECYCLE_res *ir_res = 0;
	IMAGERECYCLE_Return_Buffer *image_retbuf = 0;
	Failing_Images_List *filP = 0;
	const char *err_dsc;

	DEBUG_PRINT("vmsmapi : start of function\n");

	/* obtain session token */
	memcpy(ir_args.SessionToken,
	       image->server->priv->session_token,
	       sizeof(Session_Token));
	ir_args.TargetIdentifier = image->name;
	ir_res = image_recycle_1(&ir_args,image->server->priv->serverP);
	if (!ir_res) {
		rpcError(image->server, IMAGE_RECYCLE);
		return 396;
	}

	rc = ir_res->rc;
	if (rc == RCERR_IMAGEOP) {
		rs = ir_res->IMAGERECYCLE_res_u.resfailbuf.rs;
		if (rs == RS_SOME_NOT_RECYC) {
			image_retbuf = &ir_res->IMAGERECYCLE_res_u.resfailbuf.
				IMAGERECYCLE_resfail_buffer_u.resfail_buf.
				ReturnBuffer;
			create_msg(image->server, "* NumImagesRecycled=%d"
				   " NumImagesNotRecycled=%d\n",
				   image_retbuf->NumImagesRecycled,
				   image_retbuf->NumImagesNotRecycled);
			image->server->problem_class = WARNING;

		filP = image_retbuf->FailingImagesList;
		for (; filP; filP = filP->next_entry) {
			/* message chaining */
			create_msg(image->server, "%s * %s rc=%d reason=%d\n",
				   image->server->problem,
				   filP->ImageName,
				   filP->ImageReturnCode,
				   filP->ImageReasonCode);
		}
		/* save session token */
		memcpy(image->server->priv->session_token,
		       ir_res->IMAGERECYCLE_res_u.resfailbuf.
		       IMAGERECYCLE_resfail_buffer_u.
		       resfail_buf.SessionToken,
		       sizeof(Session_Token));

	} else { /* (rs == RS_SOME_NOT_RECYC) */
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageRecycle : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = FATAL;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       ir_res->IMAGERECYCLE_res_u.resfailbuf.
		       IMAGERECYCLE_resfail_buffer_u.
		       resfail_nobuf.SessionToken,
		       sizeof(Session_Token));
	} /* (rc == RCERR_IMAGEOP) */

	} else if (rc) {
		rs = ir_res->IMAGERECYCLE_res_u.resfail.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageRecycle : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = FATAL;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       ir_res->IMAGERECYCLE_res_u.resfail.SessionToken,
		       sizeof(Session_Token));

	} else {
		rs = ir_res->IMAGERECYCLE_res_u.resok.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageRecycle : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = OK;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       ir_res->IMAGERECYCLE_res_u.resok.SessionToken,
		       sizeof(Session_Token));
	}

	return rc;
} /* vmsmapi_imageRecycle() */

/*--------------------------------------------------------------------*/
/*
   deactivate image - api
*/
Return_Code vm_image_deactivate(struct snipl_image *image)
{
	int rc;
	int rs;
	int force = image->server->parms.force;
	char * force_time;
	IMAGEDEACTIVATE_args id_args;
	IMAGEDEACTIVATE_res* id_res;
	IMAGEDEACTIVATE_Return_Buffer* image_retbuf;
	Failing_Images_List* filP;
	const char *err_dsc;

	DEBUG_PRINT("vmsmapi : start of function\n");

	if (force == 1)
		force_time = "IMMED";
	else
		force_time = "WITHIN 300";

	/* obtain session token */
	memcpy(id_args.SessionToken,
	       image->server->priv->session_token,
	       sizeof(Session_Token));
	id_args.TargetIdentifier = image->name;
	id_args.ForceTime = force_time;
	id_res = image_deactivate_1(&id_args,image->server->priv->serverP);
	if (!id_res) {
		rpcError(image->server, IMAGE_DEACTIVATE);
		return 396;
	}

	rc = id_res->rc;
	if (rc == RCERR_IMAGEOP) {
		rs = id_res->IMAGEDEACTIVATE_res_u.resfailbuf.rs;
		if (rs == RS_NOT_ALL){
			image_retbuf = &id_res->
			   IMAGEDEACTIVATE_res_u.resfailbuf.
			   IMAGEDEACTIVATE_resfail_buffer_u.
			   resfail_buf.ReturnBuffer;
			create_msg(image->server, "* NumImagesDeactivated=%d"
				   " NumImagesNotDeactivated=%d\n",
				   image_retbuf->NumImagesDeactivated,
				   image_retbuf->NumImagesDeactivated);
			image->server->problem_class = WARNING;

			filP = image_retbuf->FailingImagesList;
			for (; filP; filP = filP->next_entry) {
				/* message chaining */
				create_msg(image->server,
					   "%s * %s rc=%d reason=%d\n",
					   image->server->problem,
					   filP->ImageName,
					   filP->ImageReturnCode,
					   filP->ImageReasonCode);
			}
			/* save session token */
			memcpy(image->server->priv->session_token,
			       id_res->
			       IMAGEDEACTIVATE_res_u.resfailbuf.
			       IMAGEDEACTIVATE_resfail_buffer_u.
			       resfail_buf.SessionToken,
			       sizeof(Session_Token));
		} else { /* rs != RS_NOT_ALL */
			err_dsc = vmsmapi_get_error_description(rc, rs);
			create_msg(image->server,
				   "* ImageDeactivate : Image %s %s\n",
				   image->name, err_dsc);
			image->server->problem_class = FATAL;
			/* save session token */
			memcpy(image->server->priv->session_token,
			       id_res->IMAGEDEACTIVATE_res_u.resfailbuf.
			       IMAGEDEACTIVATE_resfail_buffer_u.
			       resfail_nobuf.SessionToken,
			       sizeof(Session_Token));
		} /* else */
	} else if (rc) {
		rs = id_res->IMAGEDEACTIVATE_res_u.resfail.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageDeactivate : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = FATAL;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       id_res->IMAGEDEACTIVATE_res_u.resfail.SessionToken,
		       sizeof(Session_Token));
	} else { /* rc == 0 */
		/*
		 *returned rs contains force timeout, which is of no use here
		 * rs = id_res->IMAGEDEACTIVATE_res_u.resok.rs;
		 * */
		rs = RS_NONE;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server, "* ImageDeactivate : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = OK;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       id_res->IMAGEDEACTIVATE_res_u.resok.SessionToken,
		       sizeof(Session_Token));
	}

	return rc;

} /* vmsmapi_imageDeactivate() */


/*--------------------------------------------------------------------*/
Return_Code vm_image_getstatus(struct snipl_image *image)
{
	int rc, rs;
	IMAGESTATUSQUERY_args is_args;
	IMAGESTATUSQUERY_res *is_res = 0;
	const char *err_dsc;

	DEBUG_PRINT("vmsmapi : start of function\n");

	/* obtain session token */
	memcpy(is_args.SessionToken,
	       image->server->priv->session_token,
	       sizeof(Session_Token));
	is_args.TargetIdentifier = image->name;
	is_res = image_status_query_2(&is_args, image->server->priv->serverP);
	if (!is_res) {
		rpcError(image->server, IMAGE_STATUS_QUERY);
		return 396;
	}

	rc = is_res->rc;
	if (rc) {
		rs = is_res->IMAGESTATUSQUERY_res_u.resfail.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(image->server,
			   "* ImageStatusQuery : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = FATAL;
		/* save session token */
		memcpy(image->server->priv->session_token,
		       is_res->IMAGESTATUSQUERY_res_u.resfail.SessionToken,
		       sizeof(Session_Token));
	} else {
		rs = is_res->IMAGESTATUSQUERY_res_u.resok.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		if (rs == RS_NONE)
			err_dsc = image_active;
		create_msg(image->server,
			   "* ImageStatusQuery : Image %s %s\n",
			   image->name, err_dsc);
		image->server->problem_class = OK;

		/* save session token */
		memcpy(image->server->priv->session_token,
		       is_res->IMAGESTATUSQUERY_res_u.resok.SessionToken,
		       sizeof(Session_Token));
	}
	return rc;
}


/*--------------------------------------------------------------------*/
/*
   login to vm server :
   connect to server
   then do a login using userid and password
*/
Return_Code vm_server_login(struct snipl_server *server)
{
	LOGIN_args loginArgs;
	LOGIN_res *login_res = 0;
	int rc;
	int rs;
	const char *err_dsc;

	DEBUG_PRINT("vmsmapi : start of function\n");

	rc = connectServer(server);
	if (rc != RC_OK)
		return rc;

	loginArgs.AuthenticatedUserid = server->user;
	loginArgs.loginpw             = server->password;;
	login_res = login_1(&loginArgs,server->priv->serverP);
	if (!login_res) {
		rpcError(server, LOGIN);
		return 396;
	}

	rc = login_res->rc;
	if (rc == 0) {
		/* save session token */
		memcpy(server->priv->session_token,
			login_res->LOGIN_res_u.resok.SessionToken,
			sizeof (Session_Token));
	} else {
		rs = login_res->LOGIN_res_u.resfail.rs;
		err_dsc = vmsmapi_get_error_description(rc, rs);
		create_msg(server, "* login error(%d/%d) : %s\n", rc, rs,
			   err_dsc);
		server->problem_class = FATAL;
	}
	return rc;

} /* vmsmapi_login() */


static inline void free_priv_conn(struct snipl_server_private *priv)
{
	if (priv->serverP) {
		clnt_destroy(priv->serverP);	// destroy old connection
		priv->serverP = NULL;
	}
}

static int vm_server_logout(struct snipl_server *server)
{
	DEBUG_PRINT("vmsmapi : start of function\n");

	if (server) {
		if (server->priv) {
			free_priv_conn(server->priv);
			free(server->priv);
			server->priv = NULL;
		}
	}
	return RC_OK;
}

/*--------------------------------------------------------------------*/
/* rpcError returns rc and an error message is written to server->problem
 * if an rpc error occured
 * else 0 is returned and server->problem is set to NULL */
static void rpcError(struct snipl_server *server, int fnum)
{
	char *msgP = NULL;

	msgP = clnt_sperror(server->priv->serverP, 0);
	create_msg(server, "* Error calling %s : %d %s",
		   get_rpc_function(fnum), (int)*(msgP + 1), msgP + 5);
	server->problem_class = FATAL;
	return;
} /* rpcError(...) */

