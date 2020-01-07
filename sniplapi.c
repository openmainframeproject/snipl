/*
 * sniplapi.c : API for snIPL (simple network IPL)
 *
 * Copyright IBM Corp. 2002, 2016
 * Author:    Ursula Braun       (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <time.h>
#include <hwmcaapi.h>
#include "snipl.h"
#include "sniplapi.h"


/**************************************************************/
/* initialize server structures                               */
/**************************************************************/
extern void prepare(struct snipl_server *server)
{
	server->ops = &snipl_server_ops;
	return;
}

/**************************************************************/
/* set the encrypted connection parameters for a snipl server */
/* snmp target						      */
/**************************************************************/
static void set_encrypted_connection(HWMCA_SNMP_TARGET_T *snmp_target,
				     const char *user, const char *password)
{
	int id_item_len;

	snmp_target->ulSecurityVersion = HWMCA_SECURITY_VERSION3;

	id_item_len = HWMCA_MAX_USERNAME_LEN - 1;
	strncpy(snmp_target->szUsername, user, id_item_len);
	strncpy(snmp_target->szPassword, password, id_item_len);
}

/**************************************************************/
/* do parameter checking                                      */
/* initialize SNMP interfaces                                 */
/**************************************************************/
static int snipl_lpar_prepare_check(struct snipl_server *server)
{
	int ret;
	struct snipl_image *image;
	const char * compare_string = "0000000000000000";
	int flags = O_WRONLY | O_CREAT;;
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
	char *scsi_op;

	ret = 0;
	if (server->user && !server->enc) {
		create_msg(server, "%soption --userid must not be specified "
			   "for unencrypted LPAR-type server connection\n",
			   server->problem);
		ret = USERNAME_ENC_OFF;
	}
	if (server->enc && !server->user) {
		create_msg(server, "%soption --userid must be specified "
			   "for encrypted LPAR-type server connection\n",
			   server->problem);
		ret = NO_USERNAME;
	}

	if (server->port != UNDEFINED) {
		create_msg(server, "%soption --port must not be specified "
			   "for an LPAR-type server\n",
			   server->problem);
		ret = CONFLICTING_OPTIONS;
	}

	if (server->password &&
	    strlen(server->password) > 16) {
		create_msg(server, "%spassword too long - maximum size is "
			   "16 characters\n",
			   server->problem);
		ret = INVALID_PARAMETER_VALUE;
	}

	if (!server->password) {
		create_msg(server, "%soption --password must be specified\n",
			   server->problem);
		ret = MISSING_PASSWORD;
	}

	if (!server->timeout)
		server->timeout = 60000;

	if (server->parms.msg_timeout != -1 &&
	    server->parms.image_op != DIALOG) {
		create_msg(server, "%soption --msgtimeout can only be "
			   "specified for command --dialog\n",
			   server->problem);
		ret = CONFLICTING_OPTIONS;
	}
	if (server->parms.msg_timeout != -1 && server->parms.msg_timeout < 1) {
		create_msg(server,
			"%smsgtimeout value %i is zero or negative\n",
			server->problem, server->parms.msg_timeout);
		ret = INVALID_PARAMETER_VALUE;
	}
	if (server->parms.msgfilename) {
		if (server->parms.image_op != DIALOG) {
			create_msg(server,
				   "%soption --msgfilename can only be "
				   "specified for command --dialog\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
}

	if (server->parms.profile) {
		if (server->parms.image_op != ACTIVATE) {
			create_msg(server, "%soption --profile can only be "
				   "specified for command --activate\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (strlen(server->parms.profile) > 16) {
			create_msg(server, "%sprofile_name %s too long - "
				   "maximum size is 16 characters\n",
				   server->problem,
				   server->parms.profile);
			ret = INVALID_PARAMETER_VALUE;
		}
	}

	if (server->parms.image_op != LOAD &&
	    server->parms.image_op != SCSILOAD &&
	    server->parms.image_op != SCSIDUMP) {
		if (server->parms.load_address) {
			create_msg(server, "%soption --address_load can only "
				   "be specified for commands --load, "
				   "--scsiload, and --scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (server->parms.load_parms) {
			create_msg(server, "%soption --parameters_load can "
				   "only be specified for command --load, "
				   "--scsiload, and --scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
	}
	if (server->parms.image_op != LOAD ) {
		if (server->parms.clear != -1) {
			create_msg(server, "%soption --noclear can only be "
				   "specified for command --load\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (server->parms.store_stat != -1) {
			create_msg(server, "%soption --storestatus can only be "
				   "specified for command --load\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (server->parms.load_timeout != -1) {
			create_msg(server, "%soption --load_timeout can only "
				   "be specified for command --load\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
	}
	if (server->parms.load_parms) {
		if (strlen(server->parms.load_parms) > 8) {
			create_msg(server, "%sparameters_load %s too long - "
				   "maximum length is 8\n",
				   server->problem,
				   server->parms.load_parms);
			ret = INVALID_PARAMETER_VALUE;
		}
	}
	if (server->parms.load_timeout != -1) {
		if (server->parms.load_timeout < 60) {
			create_msg(server, "%sload_timeout value %i too small "
				   " - minimum value is 60\n",
				   server->problem,
				   server->parms.load_timeout);
			ret = INVALID_PARAMETER_VALUE;
		}
		else if (server->parms.load_timeout > 600) {
			create_msg(server, "%sload_timeout value %i too large "
				   "- maximum value is 600\n",
				   server->problem,
				   server->parms.load_timeout);
			ret = INVALID_PARAMETER_VALUE;
		}
	}
	if (server->parms.image_op != SCSILOAD &&
	    server->parms.image_op != SCSIDUMP) {
		if (!strncmp(&server->parms.scsiload_wwpn[0], compare_string,
		    16)) {
			create_msg(server, "%soption --wwpn_scsiload can only "
				   "be specified for commands --scsiload and "
				   "--scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (!strncmp(&server->parms.scsiload_lun[0], compare_string,
		    16)) {
			create_msg(server, "%soption --lun_scsiload can only "
				   "be specified for command --scsiload and "
				   "--scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (server->parms.scsiload_bps != -1) {
			create_msg(server, "%soption --bps_scsiload can only "
				   "be specified for command --scsiload and "
				   "--scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (server->parms.scsiload_ossparms) {
			create_msg(server, "%soption --ossparms_scsiload can "
				   "only be specified for command --scsiload "
				   "and --scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
		if (!strncmp(&server->parms.scsiload_bootrec[0],
			     compare_string, 16)) {
			create_msg(server, "%soption --bootrecord_scsiload can "
				   "only be specified for command --scsiload "
				   "and --scsidump\n",
				   server->problem);
			ret = CONFLICTING_OPTIONS;
		}
	} else {
		if (server->parms.load_address && strlen(server->parms.load_address) > 5) {
			fprintf(stderr, "load_address %s too long - maximum size "
				"when using SCSI load is 5 characters\n",
				server->parms.load_address);
			ret = INVALID_PARAMETER_VALUE;
		}
	}
	if (server->parms.scsiload_wwpn) {
		if (strlen(server->parms.scsiload_wwpn) > 16) {
			create_msg(server, "%swwpn_scsiload %s too long - "
				   "maximum length is 16\n",
				   server->problem,
				   server->parms.scsiload_wwpn);
			ret = INVALID_PARAMETER_VALUE;
		}
	}
	if (server->parms.scsiload_lun) {
		if (strlen(server->parms.scsiload_lun) > 16) {
			create_msg(server, "%slun_scsiload %s too long - "
				   "maximum length is 16\n",
				   server->problem,
				   server->parms.scsiload_lun);
			ret = INVALID_PARAMETER_VALUE;
		}
	}
	if (server->parms.scsiload_bootrec) {
		if (strlen(server->parms.scsiload_bootrec) > 16) {
			create_msg(server, "%sbootrecord_scsiload %s too long "
				   "- maximum length is 16\n",
				   server->problem,
				   server->parms.scsiload_bootrec);
			ret = INVALID_PARAMETER_VALUE;
		}
	}
	if (server->parms.scsiload_bps != -1) {
		if (server->parms.scsiload_bps > 30) {
			create_msg(server, "%sbps_scsiload value %i too large "
				   "- maximum value is 30\n",
				   server->problem,
				   server->parms.scsiload_bps);
			ret = INVALID_PARAMETER_VALUE;
		}
	}


	snipl_for_each_image(server, image) {
		if (strlen(image->name) > 80) {
			create_msg(server, "%simage_name %s too long - maximum "
				   "size is 80 characters\n",
				   server->problem,
				   image->name);
			ret = INVALID_PARAMETER_VALUE;
		}
	}

	/*
	 * Prohibit multiple LPAR image IPL from the same CCW device.
	 */
	if (server->parms.image_op == LOAD &&
	    server->_images && server->_images->_next &&
	    server->parms.load_address &&
	    server->parms.force != 1) {
		create_msg(server, "More than one image name specified for "
			   "LOAD operation with the load address "
			   "specified without -F (--force) parameter.\n");
		ret = MORE_THAN_ONE_IMAGE;
	}
	/*
	 * Prohibit multiple LPAR image IPL or dump from/to the same SCSI
	 * device.
	 */
	if ((server->parms.image_op == SCSILOAD ||
	     server->parms.image_op == SCSIDUMP) &&
	    server->_images && server->_images->_next &&
	    (server->parms.load_address ||
	     server->parms.scsiload_wwpn[0] ||
	     server->parms.scsiload_lun[0]) &&
	    server->parms.force != 1) {
		if (server->parms.image_op == SCSILOAD)
			scsi_op = "SCSILOAD";
		else
			scsi_op = "SCSIDUMP";
		create_msg(server, "More than one image name specified for "
			   "%s operation with the load address "
			   "or SCSI WWPN or SCSI LUN specified "
			   "without -F (--force) parameter.\n", scsi_op);
		ret = MORE_THAN_ONE_IMAGE;
	}

	if (ret)
		return ret;

	/******************************/
	/* initialize data structures */
	/******************************/
	server->priv = calloc(1, sizeof(*server->priv));
	if (!server->priv) {
		server->problem = strdup("cannot allocate buffer for "
					 "snipl_server_private\n");
		server->problem_class = FATAL;
		return STORAGE_PROBLEM;
	}
	server->priv->snmp_data_p = calloc(BUFSIZE, 1);
	if (!server->priv->snmp_data_p) {
		server->problem =
			strdup("cannot allocate buffer for snmp_data_p\n");
		server->problem_class = FATAL;
		return STORAGE_PROBLEM;
	}
	memset(&server->priv->snmp_command, '\0', HWMCA_INITIALIZE_SIZE);
	/* set IP address of SE */
	server->priv->snmp_ctarget.pHost = (void *)server->address;
	/* set password */
	strncpy(server->priv->snmp_ctarget.szCommunity, server->password,
		HWMCA_MAX_COMMUNITY_LEN);
	server->priv->snmp_command.pTarget = &server->priv->snmp_ctarget;

	if (server->enc)
		set_encrypted_connection(&server->priv->snmp_ctarget,
					 server->user, server->password);

	if (server->parms.image_op == DIALOG) {
		server->priv->snmp_notify_p = calloc(BUFSIZE, 1);
		if (!server->priv->snmp_notify_p) {
			server->problem = strdup("cannot allocate buffer for "
						 "snmp_notify_p\n");
			server->problem_class = FATAL;
			return STORAGE_PROBLEM;
		}
		memset(&server->priv->snmp_notify, '\0', HWMCA_INITIALIZE_SIZE);
		/* set IP address of SE */
		server->priv->snmp_ntarget.pHost = (void *) server->address;
		/* set password */
		strncpy(server->priv->snmp_ntarget.szCommunity,
			server->password,
			HWMCA_MAX_COMMUNITY_LEN);
		server->priv->snmp_notify.pTarget = &server->priv->snmp_ntarget;

		if (server->enc)
			set_encrypted_connection(&server->priv->snmp_ntarget,
						 server->user,
						 server->password);

		if (server->parms.msgfilename) {
			server->priv->msgfile = open(server->parms.msgfilename,
				flags, mode);
			if (server->priv->msgfile == -1) {
				create_msg(server, "%sOpening msgfilename %s "
					   "returns <%s>\n",
					   server->problem,
					   server->parms.msgfilename,
					   strerror(errno));
				ret = INVALID_PARAMETER_VALUE;
			}
			lseek(server->priv->msgfile, 0, SEEK_END);
		}
	}
	return ret;
}


/**************************************************************/
/* initialize SNMP interfaces                                 */
/* in case of an Hwmca Error return code + 2000 is returned   */
/**************************************************************/
static int invoke_HwmcaInitialize(struct snipl_server *server)
{
	int   ret;

	server->priv->snmp_command.ulEventMask =
		HWMCA_EVENT_COMMAND_RESPONSE +
		HWMCA_DIRECT_INITIALIZE +
		HWMCA_SNMP_VERSION_2;

	ret = 0;
	ret = HwmcaInitialize(&server->priv->snmp_command, server->timeout);
	if (ret != HWMCA_DE_NO_ERROR) {
		create_msg(server, "return code of HwmcaInitialize is %s\n",
			   getErrorMessage(ret));
		server->problem_class = FATAL;
		/* avoid invocation of HwmcaTerminate */
		/* (segmentation fault in actzsa01.c) */
		free(server->priv->snmp_data_p);
		server->priv->snmp_data_p = NULL;
		if (server->priv->snmp_notify_p) {
			free(server->priv->snmp_notify_p);
			server->priv->snmp_notify_p = NULL;
		}
		return ret+RET_PLUS;
	}

	if (server->priv->snmp_notify_p) {
		/* dialog requested */
		server->priv->snmp_notify.ulEventMask =
			HWMCA_EVENT_OPSYS_MESSAGE +
			HWMCA_DIRECT_INITIALIZE +
			HWMCA_SNMP_VERSION_2;

		ret = HwmcaInitialize(&server->priv->snmp_notify,
				      server->timeout);
		if (ret != HWMCA_DE_NO_ERROR) {
			create_msg(server, "return code of HwmcaInitialize for "
				"notification is %s\n", getErrorMessage(ret));
			server->problem_class = FATAL;
			/* avoid invocation of HwmcaTerminate */
			/* (segmentation fault in actzsa01.c) */
			free(server->priv->snmp_notify_p);
			server->priv->snmp_notify_p = NULL;
			return ret+RET_PLUS;
		}
	}

	server->priv->bufsize = BUFSIZE;
	memset(server->priv->snmp_data_p, '\0', server->priv->bufsize);
	return ret;
}

/**************************************************************/
/* Get data from HMC/SE                                       */
/* in case of an Hwmca Error return code + 2000 is returned   */
/**************************************************************/
static int invoke_HwmcaGet(struct snipl_server *server,
			   char *arg_string,
			   unsigned long *needed)
{
	int   ret;

	ret = 0;
	do {
		ret = HwmcaGet(&server->priv->snmp_command,
				arg_string,
				server->priv->snmp_data_p,
				server->priv->bufsize,
				needed,
				server->timeout);

		if (ret != HWMCA_DE_NO_ERROR) {
			create_msg(server, "return code of HwmcaGet is %s\n",
						getErrorMessage(ret));
			server->problem_class = FATAL;
			return ret+RET_PLUS;
		}

		if (*needed <= server->priv->bufsize)
			/* HwmcaGet successful */
			return ret;

		/* reallocate buffer snmp_data_p */
		server->priv->snmp_data_p =
			realloc(server->priv->snmp_data_p, *needed);
		if (!server->priv->snmp_data_p) {
			create_msg(server,
				   "cannot re-allocate buffer snmp_data_p\n");
			server->problem_class = FATAL;
			return STORAGE_PROBLEM;
		}
		server->priv->bufsize = *needed;
		memset(server->priv->snmp_data_p, '\0', *needed);
	} while (!ret);

	return ret;
}


static int snipl_image_status(struct snipl_server *server,
			      struct snipl_image *image)
{
	int ret = 0;
	unsigned long needed;
	char arg_string[108];

	/* read CPC image status */
	sprintf(arg_string, "%s.%s.%s", HWMCA_CPC_IMAGE_ID,
		HWMCA_STATUS_SUFFIX, image->priv->image_object);
	ret = invoke_HwmcaGet(server, arg_string, &needed);
	DEBUG_PRINT("%.8x\n",
		    *(unsigned int *)server->priv->snmp_data_p->pData);
	image->priv->status = *(unsigned int *)server->priv->snmp_data_p->pData;
	return ret;
}


/**************************************************************/
/* initialize SNMP interfaces                                 */
/* determine available LPAR objects plus LPAR names           */
/* in case of an Hwmca Error return code + 2000 is returned   */
/* if appropriate program should be terminated by caller      */
/**************************************************************/
static int snipl_lpar_login(struct snipl_server *server)
{
	int   ret;
	int   maxsize;
	unsigned long needed;
	char  arg_string[80];
	char *tmp, *image_object_suffix;
	struct snipl_image *image;
	char image_object[HWMCA_MAX_ID_LEN];

	ret = 0;
	/******************************/
	/* initialize SNMP interfaces */
	/******************************/
        ret = invoke_HwmcaInitialize(server);
	if (ret) return ret;

	/*******************************/
	/* read CPC image group object */
	/*******************************/
	sprintf(arg_string, "%s.%s",
		HWMCA_CPC_IMAGE_GROUP_ID,
		HWMCA_GROUP_CONTENTS_SUFFIX);

	ret = invoke_HwmcaGet(server, arg_string, &needed);
	if (ret)
		return ret;

	tmp = (char *)server->priv->snmp_data_p->pData;
	/* DEBUG_PRINT("returned info of HwmcaGet [1] is %s\n", tmp); */

	/* parse group object information */
	maxsize = HWMCA_MAX_ID_LEN;
	/* first group object inforamtion */
	tmp = strtok(tmp, " ");
	while (tmp) {
		/* loop through all returned group object informations */
		strncpy(image_object, tmp, maxsize);
		tmp = strtok(NULL, " ");
		/* next group object information */
		/* cut to unique number for the image object */
		image_object_suffix = &(image_object
					[strlen(HWMCA_CPC_IMAGE_ID)+1]);
		/* DEBUG_PRINT("image_object is: %s\n",image_object); */

		if (server->parms.image_op == LIST) {
			/* allocate another image */
			image = calloc(1, sizeof(*image));
			if (!image) {
				server->problem =
					strdup("cannot allocate buffer for "
					       "snipl_image\n");
				server->problem_class = FATAL;
				return STORAGE_PROBLEM;
			}
			image->_next = server->_images;
			image->server = server;
			server->_images = image;
		}

		/* read CPC image object name */
		sprintf(arg_string, "%s.%s.%s", HWMCA_CPC_IMAGE_ID,
			HWMCA_NAME_SUFFIX, image_object_suffix);

		ret = invoke_HwmcaGet(server, arg_string, &needed);
		if (ret) return ret;
/*		HEXDUMP16(stdout, "USCimn: ",
			  (char *)(server->priv->snmp_data_p->pData)); */
		/* DEBUG_PRINT("returned info of HwmcaGet [2] is %s\n",
				server->priv->snmp_data_p->pData); */

		if (server->parms.image_op == LIST) {
			/* allocate image name space */
			if (!(server->_images->name = calloc(needed+1, 1))) {
				server->problem =
					strdup("cannot allocate buffer for "
					       "image name\n");
				server->problem_class = FATAL;
				return STORAGE_PROBLEM;
			}
			strcat(server->_images->name,
				server->priv->snmp_data_p->pData);
		} else {   /* images are specified */
			snipl_for_each_image(server, image) {
				/* belongs returned image name to one of the
				   specified names? */
				if (!strcasecmp(image->name,
					server->priv->snmp_data_p->pData)) {
					/* image specified */
					/* alloc. storage for private image */
					image->priv = calloc(1,
						sizeof(*image->priv));
					if (!image->priv) {
						server->problem =
						strdup("cannot allocate buffer "
						"for snipl_image_private\n");
						server->problem_class = FATAL;
						return STORAGE_PROBLEM;
					}
					/* save image object information */
					strncpy(image->priv->image_object,
						image_object_suffix,maxsize);
					image->ops = &snipl_image_ops;
					snipl_image_status(server, image);
				};
			}
		}
	} /* end while */

	/* check if specified images exist */
		// for (image=server->_images; image; image=image->_next)
	if (server->parms.image_op != LIST) {
		snipl_for_each_image(server,image) {
			if (!image->ops) {
				create_msg(server, "Given LPAR name %s does not"
					" exist on %s\n",
					image->name,
					server->address);
				ret = SERVER_IMAGE_MISMATCH;
			} else {
				DEBUG_PRINT("LPAR %s was identified\n",
					image->name);
			}
		}
	}

	return ret;
}


/*
 * function: snipl_lpar_logout
 *
 * purpose: shuts down the snmp connections in case of errors
 */
static int snipl_lpar_logout(struct snipl_server *server)
{
	struct snipl_image *image;
	int ret = 0;

	snipl_for_each_image(server, image) {
		free(image->priv);
		image->priv = NULL;
	}

	if (server->priv) {
		if (server->priv->snmp_data_p) {
			ret = HwmcaTerminate(&server->priv->snmp_command,
					     server->timeout);
			if (ret != HWMCA_DE_NO_ERROR) {
				create_msg(server, "%sshutdown of command snmp "
					   "connection failed, "
					   "return code %d\n",
					   server->problem, ret);
			server->problem_class = FATAL;
			}
			free(server->priv->snmp_data_p);
		}
		if (server->priv->snmp_notify_p) {
			ret = HwmcaTerminate(&server->priv->snmp_notify,
					     server->timeout);
			if (ret != HWMCA_DE_NO_ERROR) {
				create_msg(server, "%sshutdown of notify snmp "
					   "connection failed, "
					   "return code %d\n",
					   server->problem, ret);
				server->problem_class = FATAL;
			}
			free(server->priv->snmp_notify_p);
		}
		free(server->priv);
	}
	return ret;
}


/*
 *	function: compare_datatype
 *
 *	purpose: compares two HWMCA_DATATYPEs
 */

static int compare_datatype(const HWMCA_DATATYPE_P data,
			    const UCHAR tag,
			    const void *compare)
{
	if (data->ucType != tag)
		return 0;

	switch (data->ucType) {
	case HWMCA_TYPE_INTEGER:
		DEBUG_PRINT("compare %ld (snmp) with %d (arg)...\n",
			    *(long *)(data->pData), *(int *)compare);
		if (*(long *)(data->pData) == *(int *)compare)
			return 1;
		break;
	case HWMCA_TYPE_OCTETSTRING:
		DEBUG_PRINT("compare %s (snmp) with %s (arg)...\n",
			    (char *)data->pData, (char *)compare);
		if (!strcmp((char *)data->pData, (char *)compare))
			return 1;
		break;
	case HWMCA_TYPE_OBJECTID:
		DEBUG_PRINT("compare %s (snmp) with %s (arg)...\n",
			    (char *)data->pData, (char *)compare);
		if (!strcmp((char *)data->pData, (char *)compare))
			return 1;
		break;
	default:
		DEBUG_PRINT("datatype not supported\n");
		break;
	}

	return 0;
}


/*
 *	function: command_handling
 *
 *	purpose: common handling of HWMCA commands
 */
int command_handling(struct snipl_image *image)
{
	unsigned long ret;
	int i;
	int correlator = 0;
	int pid = 0;
	long ltime = 0;
	int stime = 0;
	char command_target[128];
	char command_identifier[128];
	unsigned long needed;
	unsigned short tmp_force;

	sprintf(command_target, "%s.%s", HWMCA_CPC_IMAGE_ID,
		image->priv->image_object);
	sprintf(command_identifier, "%s",
		image->priv->command_id);
	ret = HWMCA_DE_NO_ERROR;

	for (i = 0; i < image->priv->data_index; i++)
		/* initialize pNext fields */
		image->priv->data[i].pNext = &image->priv->data[i+1];
	/*
	 * Generate the command correlator value.
	 */
	pid = getpid();
	ltime = time(NULL);
	stime = (unsigned)ltime / 2;
	srand(stime);
	correlator = rand() + pid;
	if (image->server->parms.image_op != STOP) {
		if (strcmp(image->priv->command_id, HWMCA_SEND_OPSYS_COMMAND)) {
			/* set force info */
			image->priv->data[image->priv->data_index] =
				(HWMCA_DATATYPE_T) {
					.ucType = HWMCA_TYPE_INTEGER,
					.ulLength = sizeof(tmp_force),
					.pData = &tmp_force,
				};
			if (image->server->parms.force == 1)
				tmp_force = HWMCA_TRUE;
			else
				tmp_force = HWMCA_FALSE;
		} else
			image->priv->data[image->priv->data_index].pNext = NULL;
/*		print_event_buffer(image->priv->data); */   /* SNIPL_DEBUG */
		ret = HwmcaCorrelatedCommand(
				&image->server->priv->snmp_command,
				command_target,
				command_identifier,
				image->priv->data,
				image->server->timeout,
				&correlator,
				sizeof(correlator));
	} else
		ret = HwmcaCorrelatedCommand(
				&image->server->priv->snmp_command,
				command_target,
				command_identifier,
				NULL,
				image->server->timeout,
				&correlator,
				sizeof(correlator));

	if (ret != HWMCA_DE_NO_ERROR) {
		create_msg(image->server,
			"%s: %s failed, return code of HwmcaCommand is %s\n",
			image->name,
			image->priv->command_name,
			getErrorMessage(ret));
		image->server->problem_class = FATAL;
		return ret+RET_PLUS;
	}

	if (strcmp(image->priv->command_id, HWMCA_SEND_OPSYS_COMMAND)) {
		fprintf(stdout, "processing...");
		fflush(stdout);
	}

	DEBUG_PRINT("*** waiting for acknowledge...\n");

	while (1) {
		ret = HwmcaWaitEvent(&image->server->priv->snmp_command,
					image->server->priv->snmp_data_p,
					image->server->priv->bufsize,
					&needed,
					image->server->timeout);

		if (ret != HWMCA_DE_NO_ERROR) {
			if (ret == HWMCA_DE_TIMEOUT) {
				if (strcmp(image->priv->command_id,
					   HWMCA_SEND_OPSYS_COMMAND)) {
					/* repeat */
					fprintf(stdout, ".");
					fflush(stdout);
				}
			} else {   /* error is not a timeout error */
				create_msg(image->server, "%s: %s: return code "
					   "of HwmcaWaitEvent is %s\n",
					   image->name,
					   image->priv->command_name,
					   getErrorMessage(ret));
				image->server->problem_class = FATAL;
				return ret+RET_PLUS;
			}
		} else {   /* no error */
			if (!compare_datatype(image->server->priv->snmp_data_p,
					      HWMCA_TYPE_OBJECTID,
					      command_target)) {
				/* this event buffer is not the response  */
				/* get next event buffer */
				DEBUG_PRINT("Non matching event buffer "
					    "received\n");
				print_event_buffer(image->server->priv->snmp_data_p);
			} else {
				if (needed > image->server->priv->bufsize) {
					/* buffer too small */
					image->server->problem =
						strdup("response buffer too small\n");
					image->server->problem_class = FATAL;
					return BUFFER_OVERFLOW;
				} else {   /* analyze returned info */
					ret = parse_command_response(image->server->priv->snmp_data_p,
						command_target,
						command_identifier,
						correlator,
						image);
					if (strcmp(image->priv->command_id,
						HWMCA_SEND_OPSYS_COMMAND))
						fprintf(stdout, "\n");
					switch (ret) {
					case 1:
						DEBUG_PRINT("Successful acknowledge...\n");
						create_msg(image->server,
							"%s: acknowledged.\n",
							image->name);
						image->server->problem_class =
							OK;
						return 0;
					case 0:
						DEBUG_PRINT("no success...\n");
						return HWMCA_PROBLEM;
					case 2:
						/* get next event buffer */
						break;
					}
				}
			}
		}
	}
	if (strcmp(image->priv->command_id, HWMCA_SEND_OPSYS_COMMAND))
		fprintf(stdout, "\n");
	return 0;
}


/*
 *	function: snipl_image_activate
 *
 *	purpose: activates a partition
 */
static int snipl_image_activate(struct snipl_image *image)
{
	int ret;

	if (!image->server->parms.profile) {
		image->priv->data[0] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[0] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = strlen(image->server->parms.profile) + 1,
			.pData = image->server->parms.profile,
		};
	}
	image->priv->data_index = 1;
	image->priv->command_id = HWMCA_ACTIVATE_COMMAND;
	image->priv->command_name = "activate";
	ret = 0;
	ret = command_handling(image);
	return ret;
}


/*
 *	function: snipl_image_deactivate
 *
 *	purpose: deactivates a partition
 */
static int snipl_image_deactivate(struct snipl_image *image)
{
	int ret;
	image->priv->data_index = 0;
	image->priv->command_id = HWMCA_DEACTIVATE_COMMAND;
	image->priv->command_name = "deactivate";
	ret = 0;
	ret = command_handling(image);
	return ret;
}


/*
 *	function: snipl_image_reset
 *
 *	purpose: reset a partition
 */
static int snipl_image_reset(struct snipl_image *image)
{
	int ret;
	image->priv->data_index = 0;
	image->priv->command_id = HWMCA_RESETCLEAR_COMMAND;
	image->priv->command_name = "reset";
	ret = 0;
	ret = command_handling(image);
	return ret;
}


/*
 *	function: snipl_image_stop
 *
 *	Purpse: stop all cpus of a partition
 */
static int snipl_image_stop(struct snipl_image *image)
{
	int ret;
	image->priv->data_index = 0;
	image->priv->command_id = HWMCA_STOP_COMMAND;
	image->priv->command_name = "stop";
	ret = 0;
	ret = command_handling(image);
	return ret;
}

/*
 *	function: snipl_image_load
 *
 *	purpose: performs a load command
 */

static int snipl_image_load(struct snipl_image *image)
{
	int ret;

	if (!image->server->parms.load_address) {
		image->priv->data[0] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[0] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = strlen(image->server->parms.load_address)+1,
			.pData = image->server->parms.load_address,
		};
	}

	if (!image->server->parms.load_parms) {
		image->priv->data[1] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[1] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = strlen(image->server->parms.load_parms) + 1,
			.pData = image->server->parms.load_parms,
		};
	}

	if (image->server->parms.clear == -1) {
		if (image->server->parms.store_stat == 1)
			image->server->parms.clear = 0;
		else
			/* default: clear=yes */
			image->server->parms.clear = 1;
	}
	image->priv->data[2] = (HWMCA_DATATYPE_T) {
		.ucType = HWMCA_TYPE_INTEGER,
		.ulLength = sizeof(image->server->parms.clear),
		.pData = &image->server->parms.clear,
	};

	if (image->server->parms.load_timeout == -1)
		/* default: 60 */
		image->server->parms.load_timeout = 60;
	image->priv->data[3] = (HWMCA_DATATYPE_T) {
		.ucType = HWMCA_TYPE_INTEGER,
		.ulLength = sizeof(image->server->parms.load_timeout),
		.pData = &image->server->parms.load_timeout,
	};

	if (image->server->parms.store_stat == -1)
		/* default: store_stat=no */
		image->server->parms.store_stat = 0;
	image->priv->data[4] = (HWMCA_DATATYPE_T) {
		.ucType = HWMCA_TYPE_INTEGER,
		.ulLength = sizeof(image->server->parms.store_stat),
		.pData = &image->server->parms.store_stat,
	};

	{   /* SNIPL_DEBUG */
		int i;
		for (i = 0; i < 5; i++)
			print_datatype(&image->priv->data[i]);
	}

	image->priv->data_index = 5;
	image->priv->command_id = HWMCA_LOAD_COMMAND;
	image->priv->command_name = "load";
	ret = 0;
	ret = command_handling(image);
	return ret;
}


/*
 *	function: scsi_setup
 *
 *	purpose: setup parameters for scsiload and scsidump commands
 */

void scsi_setup(struct snipl_image *image)
{
	if (!image->server->parms.load_address) {
	image->priv->data[0] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[0] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = strlen(image->server->parms.load_address)+1,
			.pData = image->server->parms.load_address,
		};
	}

	if (!image->server->parms.load_parms) {
		image->priv->data[1] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[1] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = strlen(image->server->parms.load_parms) + 1,
			.pData = image->server->parms.load_parms,
		};
	}

	if (!image->server->parms.scsiload_wwpn[0]) {
		image->priv->data[2] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[2] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = 17,
			.pData = image->server->parms.scsiload_wwpn,
		};
	}

	if (!image->server->parms.scsiload_lun[0]) {
		image->priv->data[3] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[3] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = 17,
			.pData = image->server->parms.scsiload_lun,
		};
	}

	if (image->server->parms.scsiload_bps == -1) {
		image->priv->data[4] =  (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[4] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_INTEGER,
			.ulLength = 2,
			.pData = &image->server->parms.scsiload_bps,
		};
	}

	if (!image->server->parms.scsiload_ossparms) {
		image->priv->data[5] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[5] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = strlen(image->server->parms.scsiload_ossparms)+1,
			.pData = image->server->parms.scsiload_ossparms,
		};
	}

	if (!image->server->parms.scsiload_bootrec[0]) {
		image->priv->data[6] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_NULL,
			.ulLength = 0,
			.pData = 0,
		};
	} else {
		image->priv->data[6] = (HWMCA_DATATYPE_T) {
			.ucType = HWMCA_TYPE_OCTETSTRING,
			.ulLength = 17,
			.pData = image->server->parms.scsiload_bootrec,
		};
	}

	{   /* SNIPL_DEBUG */
		int i;
		for (i=0; i<7; i++)
			print_datatype(&image->priv->data[i]);
	}

	image->priv->data_index = 7;
}


/*
 *	function: snipl_image_scsiload
 *
 *	purpose: performs a scsiload command
 */

static int snipl_image_scsiload(struct snipl_image *image)
{
	int ret;

	scsi_setup(image);
	image->priv->command_id = HWMCA_SCSI_LOAD_COMMAND;
	image->priv->command_name = "scsiload";
	ret = 0;
	ret = command_handling(image);
	return ret;
}

static int snipl_image_scsidump(struct snipl_image *image)
{
	int ret;

	scsi_setup(image);
	image->priv->command_id = HWMCA_SCSI_DUMP_COMMAND;
	image->priv->command_name = "scsidump";
	ret = 0;
	ret = command_handling(image);
	return ret;
}

/*
 *	function: send_opsys_command
 *
 *	purpose: sends an operating system message
 */

static int send_opsys_command(char *command_string, struct snipl_image *image)
{
	int ret;
	short priority;

	priority = 0;
	image->priv->data[0] = (HWMCA_DATATYPE_T) {
		.ucType = HWMCA_TYPE_INTEGER,
		.ulLength = sizeof(priority),
		.pData = &priority,
	};

	image->priv->data[1] = (HWMCA_DATATYPE_T) {
		.ucType = HWMCA_TYPE_OCTETSTRING,
		.ulLength = (ULONG)strlen(command_string)+1,
		.pData = command_string,
	};

	image->priv->data_index = 1;
	image->priv->command_id = HWMCA_SEND_OPSYS_COMMAND;
	image->priv->command_name = "send_opsys";
	ret = 0;
	ret = command_handling(image);
	return ret;
}


/*
 *	function: parse_command_response
 *
 *	purpose: parses a command response event,
 *		 returns 1 for success, 0 for failure, -1 if this event is
 *		 not for us
 */
static int parse_command_response(const HWMCA_DATATYPE_P data,
                           const char *objectid,
			   const char *command,
			   const int correlator,
			   struct snipl_image *image)
{
	HWMCA_DATATYPE_P pdata;;
	char arg[256];
	int myint;
	int i = 0;

	DEBUG_PRINT("compare OBJECTID of event...\n");

	/*
	 * Check if we have received the event for our command.
	 */
	pdata = data;
/*	print_event_buffer(pdata); */  /* SNIPL_DEBUG */
	/*
	 * Try to move to the command correlator value.
	 */
	for (i = 0; i < 11; i++) {
		pdata = pdata->pNext;
		/*
		 * If received data is shorter than we need, consider the
		 * event as the another application's event.
		 */
		if (!pdata) {
			DEBUG_PRINT("Another application event has been ");
			DEBUG_PRINT("received (event is shorter).\n");
			return 2;
		}
	}
	/*
	 * Check the correlator value.
	 */
	if (pdata->ucType != HWMCA_TYPE_OCTETSTRING) {
		create_msg(image->server,
			"%s: not acknowledged - no command correlator [1]\n",
			image->name);
		image->server->problem_class = FATAL;
		return 0;

	}
	if (pdata->ulLength != (ULONG)sizeof(correlator)) {
		DEBUG_PRINT("Another application event has been received.\n");
		return 2;
	}
	if (*(int *)pdata->pData != correlator) {
		DEBUG_PRINT("Another application event has been received.\n");
		return 2;
	}
	/*
	 * Verify the received data.
	 */
	pdata = data;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [1]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	if (!compare_datatype(pdata, HWMCA_TYPE_OBJECTID, objectid)) {
		create_msg(image->server, "%s: not acknowledged - protocol "
			   "error - objectid [1]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [2]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare event notification type...\n");

	myint = 0;
	if (!compare_datatype(pdata, HWMCA_TYPE_INTEGER, &myint))    {
		create_msg(image->server, "%s: not acknowledged - protocol "
			   "error - event notification is %ld\n",
			   image->name,
			   *(long *)(pdata->pData));
		/* get next event buffer */
		DEBUG_PRINT("Non matching event buffer received\n");
		print_event_buffer(image->server->priv->snmp_data_p);
		return 2;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [3]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare command OBJECTID of event...\n");

	sprintf(arg, "%s.%s.%s",
		HWMCA_CPC_IMAGE_ID,
		HWMCA_COMMAND_OBJECT_ID_SUFFIX,
		image->priv->image_object);

	if (!compare_datatype(pdata, HWMCA_TYPE_OBJECTID, arg)) {
		create_msg(image->server, "%s: not acknowledged - protocol "
			   "error - objectid [2]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [4]\n", image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare command code value\n");

	if (!compare_datatype(pdata, HWMCA_TYPE_OBJECTID, command)) {
		create_msg(image->server, "%s: not acknowledged - protocol "
			   "error - objectid [3]\n",
			   image->name);
//		print_event_buffer(pdata);   // used to debug HMC problem
		image->server->problem_class = FATAL;
		return 0;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [5]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare condition code OBJECTID\n");

	sprintf(arg, "%s.%s.%s",
		HWMCA_CPC_IMAGE_ID,
		HWMCA_COMMAND_CONDITION_CODE_SUFFIX,
		image->priv->image_object);

	if (!compare_datatype(pdata, HWMCA_TYPE_OBJECTID, arg)) {
		create_msg(image->server, "%s: not acknowledged - command "
			   "response protocol error: condition code\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [6]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare condition code value\n");

	myint = 0;
	if (!compare_datatype(pdata, HWMCA_TYPE_INTEGER, &myint)) {
		create_msg(image->server, "%s: not acknowledged - command was "
			   "not successful - rc is %ld\n",
			   image->name,
			   *(long *)pdata->pData);
		image->server->problem_class = FATAL;
		return 0;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [7]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare last indicator OBJECTID\n");

	sprintf(arg, "%s.%s.%s",
		HWMCA_CPC_IMAGE_ID,
		HWMCA_COMMAND_LAST_INDICATOR_SUFFIX,
		image->priv->image_object);

	if (!compare_datatype(pdata, HWMCA_TYPE_OBJECTID, arg)) {
		create_msg(image->server, "%s: not acknowledged - command "
			   "response protocol error: last indicator\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	pdata = pdata->pNext;
	if (!pdata) {
		create_msg(image->server, "%s: not acknowledged - no "
			   "sufficient data returned [8]\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}

	DEBUG_PRINT("compare last indicator value\n");

	myint = 1;
	if (!compare_datatype(pdata, HWMCA_TYPE_INTEGER, &myint)) {
		create_msg(image->server, "%s: not acknowledged - protocol "
			   "error: last indicator not set\n",
			   image->name);
		image->server->problem_class = FATAL;
		return 0;
	}
	pdata = pdata->pNext;

#if 0
	if (pdata) {
		printf("more data pending\n");
		return 0;
	}
#endif

	return 1;
}

#if SNIPL_DEBUG
/*
 *	function: print_datatype
 *
 *	purpose: dumps a HWMCA_DATATYPE for debugging purposes
 */

static void print_datatype(const HWMCA_DATATYPE_T *snmp_data)
{
	printf("size: %lu\n", snmp_data->ulLength);
	switch (snmp_data->ucType) {
	case HWMCA_TYPE_SEQUENCE:
		printf("HWMCA_TYPE_SEQUENCE not supported yet\n");
		break;
	case HWMCA_TYPE_INTEGER:
		printf("datatype integer: %d\n", *(int *)(snmp_data->pData));
		printf("or datatype short: %d\n", *(short *)(snmp_data->pData));
		printf("or datatype ulong: %lu\n",
		       *(unsigned long *)(snmp_data->pData));
		break;
	case HWMCA_TYPE_OCTETSTRING:
		printf("datatype string: %s\n", (char *)snmp_data->pData);
		break;
	case HWMCA_TYPE_NULL:
		printf("datatype NULL\n");
		break;
	case HWMCA_TYPE_OBJECTID:
		printf("datatype objectid: %s\n", (char *)snmp_data->pData);
		break;
	case HWMCA_TYPE_IPADDRESS:
		printf("HWMCA_TYPE_IPADDRESS not supported yet\n");
		break;
	default:
		printf("no such ucType %x\n", snmp_data->ucType);
		break;
	}
}
#endif


/*
 *	function: write_opsys_message
 *
 *	purpose: writes operating system message
 *		 to stdout and optionally to msgfile
 */

static void write_opsys_message(struct snipl_image *image,
				HWMCA_DATATYPE_P *snmp_notify_loop_p)
{
	ssize_t len;
	char *c;

	DEBUG_PRINT("output string >%s<...\n",
		(char *)(*snmp_notify_loop_p)->pData);
	fprintf(stdout, "%s\n", (char *)(*snmp_notify_loop_p)->pData);
	if (image->server->priv->msgfile) {
		c = (*snmp_notify_loop_p)->pData +
		    (*snmp_notify_loop_p)->ulLength - 1;
		*c = '\n';
		len = write(image->server->priv->msgfile,
			(*snmp_notify_loop_p)->pData,
			(*snmp_notify_loop_p)->ulLength);
		if (len == -1) {
			fprintf(stderr, "msgfile error %s\n", strerror(errno));
		}
	}
}

/*
 *	function: parse_opsys_message
 *
 *	purpose: parses operating system messages event
 */

static void parse_opsys_message(struct snipl_image *image,
				HWMCA_DATATYPE_P *snmp_notify_loop_p,
				int needed)
{
	int i;

	print_event_buffer(*snmp_notify_loop_p);

	DEBUG_PRINT("start of parse_opsys_message\n");
	if (!(*snmp_notify_loop_p = (*snmp_notify_loop_p)->pNext)) {
		DEBUG_PRINT("protocol error\n");
		fprintf(stderr, "protocol error\n");
	}

	/* print_datatype(snmp_notify_loop_p); */
	/* print address */
	/* DEBUG_PRINT("one: %.8x\n",(unsigned int)*snmp_notify_loop_p); */
	if (!*(long *)((*snmp_notify_loop_p)->pData)) {
		/* DEBUG_PRINT("pData is zero\n"); */
		/* print address */
		/* DEBUG_PRINT("two: %.8x\n",
			(unsigned int)*snmp_notify_loop_p); */
		while ((*snmp_notify_loop_p)->pNext &&
		    (*snmp_notify_loop_p)->pNext <=
		     image->server->priv->snmp_notify_p + needed) {
			*snmp_notify_loop_p =
				(*snmp_notify_loop_p)->pNext;
			/* print address */
			/* DEBUG_PRINT("thr: %.8x\n",
				(unsigned int)*snmp_notify_loop_p); */
		}
	} else if (*(long *)((*snmp_notify_loop_p)->pData) == 1) {
		/* DEBUG_PRINT("pData is one\n"); */
		i = 0;
		while ((*snmp_notify_loop_p)->pNext &&
		    (*snmp_notify_loop_p)->pNext <=
		     image->server->priv->snmp_notify_p + needed) {
			if (i == 4) {
				/* print message text */
				write_opsys_message(image, snmp_notify_loop_p);
			}
			i++;
			*snmp_notify_loop_p = (*snmp_notify_loop_p)->pNext;
		}
	}
	*snmp_notify_loop_p = (*snmp_notify_loop_p)->pData +
			      (*snmp_notify_loop_p)->ulLength;
}


/*
 *	function: poll_opsys_messages
 *
 *	purpose: repeatedly calls HwmcaWaitEvent to look for operating
 *	         system messages
 *	         This is the main routine of the child process.
 *	         It is killed from the parent process.
 */

static void poll_opsys_messages(struct snipl_image *image)
	__attribute__((noreturn));
static void poll_opsys_messages(struct snipl_image *image)
{
	unsigned long needed = 0;
	unsigned long bufsize = BUFSIZE;
	int ret = 0;
	int rc = 0;
	const char *errstr =
		"snipl reconnect necessary, possible data loss!\n";
	char *tmp;
	HWMCA_DATATYPE_P snmp_notify_loop_p;

	DEBUG_PRINT("poll_opsys_messages has started...\n");
	DEBUG_PRINT("image name is %s\n", image->name);

	while (1) {
		do {   // wait for messages and handle them
			DEBUG_PRINT("before HwmcaWaitEvent\n");
			memset(image->server->priv->snmp_notify_p, '\0',
			       needed);
			ret = HwmcaWaitEvent(&image->server->priv->snmp_notify,
				   image->server->priv->snmp_notify_p,
				   bufsize,
				   &needed,
				   image->server->parms.msg_timeout);
			DEBUG_PRINT("after HwmcaWaitEvent %u\n",
				(unsigned int)ret);
			switch (ret) {
			case HWMCA_DE_NO_ERROR:
				break;
			case HWMCA_CMD_TIMEOUT:
				DEBUG_PRINT("timeout...\n");
				break;
			case HWMCA_CMD_REQUEST_RECV_ERROR:
				/* broken connection, try to reconnect */
				fprintf(stderr, "%s\n", errstr);
				fflush(stderr);
				if (image->server->priv->msgfile)
					rc = write(image->server->priv->msgfile,
					errstr, strlen(errstr));
				rc = HwmcaTerminate(&image->server->priv->snmp_notify,
					image->server->timeout);
				if (rc != HWMCA_DE_NO_ERROR) {
					ret = ret + RET_PLUS;
					goto out;
				}
				image->server->priv->snmp_notify.ulEventMask =
					HWMCA_EVENT_OPSYS_MESSAGE +
					HWMCA_DIRECT_INITIALIZE +
					HWMCA_SNMP_VERSION_2;
				rc = HwmcaInitialize(&image->server->priv->snmp_notify,
					image->server->timeout);
				if (rc != HWMCA_DE_NO_ERROR) {
					ret = ret + RET_PLUS;
					goto out;
				}
				break;
			default:
				fprintf(stderr, "error %s occurred\n",
					getErrorMessage(ret));
				fflush(stderr);
				ret = ret + RET_PLUS;
				goto out;
			}
		} while (ret != HWMCA_DE_NO_ERROR);

		DEBUG_PRINT("messages received...\n");

		if (needed > bufsize) {
			fprintf(stderr, "response buffer too small\n");
			fflush(stderr);
			exit(BUFFER_OVERFLOW);
		}

		snmp_notify_loop_p = image->server->priv->snmp_notify_p;

		do {
			/* check whether this message is for us */
/*			print_event_buffer(snmp_notify_loop_p); */
			DEBUG_PRINT("image->priv->image_object: %s\n",
				image->priv->image_object);
			tmp = (char *)snmp_notify_loop_p->pData +
				strlen(HWMCA_CPC_IMAGE_ID) + 1;
			/* this code results of trial and error ==> dirty! */
			if (snmp_notify_loop_p->ulLength == 38)
				tmp = tmp + 4;
			if (strcmp(image->priv->image_object, tmp) != 0) {
				DEBUG_PRINT("tmp: %s\n", tmp);
				DEBUG_PRINT("ignoring message...\n");
				snmp_notify_loop_p = NULL;
			} else {
				DEBUG_PRINT("it's a message for us...\n");
				parse_opsys_message(image, &snmp_notify_loop_p,
						    needed);
			}
		} while (snmp_notify_loop_p &&
			 (char)snmp_notify_loop_p->ucType &&
			 (snmp_notify_loop_p <=
			  image->server->priv->snmp_notify_p + needed));
	}  /* while */

out:
	fsync(image->server->priv->msgfile);
	close(image->server->priv->msgfile);
	DEBUG_PRINT("poll_opsys_messages is ending... \n");
	exit(ret);
}


/*
 *	function: handle_del
 *
 *	purpose: handle backward delete
 */

static void handle_del(char *command_string, int *current, int *last)
{
	int k;

	if (*current > 0) {
		for (k=*current-1;k<*last-1;k++)
			command_string[k] = command_string[k+1];
		command_string[*last-1] = ' ';
		*current -= 1;
		// overwrite the rest on output
		for (k=*current;k<*last;k++) {
			fputc(command_string[k], stdout);
		}
		/* reset cursor */
		for (k=*current;k<*last;k++) {
			fputc(27, stdout);
			fputc(91, stdout);
			fputc(68, stdout);
		}
	}
	else if (*last > 0) {
		/* rewrite the first character */
		fputc(command_string[0], stdout);
		/* reset cursor */
		fputc(27, stdout);
		fputc(91, stdout);
		fputc(68, stdout);
	}
}


/*
 *	function: handle_esc
 *
 *	purpose: handle ESC sequences
 *		 [C cursor left
 *		 [D cursor right
 *		 DEL delete current character
 */

static void handle_esc(char *command_string, int *current, int *last)
{
	int k;
	int command_char;

	command_char = fgetc(stdin);
	command_char = fputc(command_char, stdout);
	tcdrain(fileno(stdout));
	if (command_char == 91) {
		/* char is [ */
		command_char = fgetc(stdin);
		command_char = fputc(command_char, stdout);
		tcdrain(fileno(stdout));
		switch (command_char) {
		case 'C':  /* arrow right */
			if (*current == *last) {
				command_string[*current] = ' '; /* blank */
				*last += 1;
			}
			*current += 1;
			break;
		case 'D':  /* arrow left */
			if (*current > 0)
				*current -= 1;
			break;
		case '3':
			command_char = fgetc(stdin);
			command_char = fputc(command_char, stdout);
			tcdrain(fileno(stdout));
			if (command_char == 126) {
				/* delete current char */
				for (k=*current;k<*last-1;k++)
					command_string[k] = command_string[k+1];
				command_string[*last-1] = ' ';
				/* overwrite the rest on output */
				for (k=*current;k<*last;k++) {
					command_char =
						fputc(command_string[k],stdout);
				}
				/* reset cursor */
				for (k=*current;k<*last;k++) {
					command_char = fputc(27, stdout);
					command_char = fputc(91, stdout);
					command_char = fputc(68, stdout);
				}
			}
			break;
		}
	}
}


/*
 *	function: snipl_image_dialog
 *
 *	purpose: starts another process to poll for operating system messages
 *               which come asynchronously
 *              prepares filtering (in) for those which are for us (chosen LPAR)
 *               and prompts for input
 */

static int snipl_image_dialog(struct snipl_image *image)
{
	int current;
	int last;
	char command_string[256] = {'\0'};
	int command_char;
	int ret;
	pid_t child;
	struct termios initialrsettings, newrsettings;

	if (image->server->parms.msg_timeout == -1)
		image->server->parms.msg_timeout = 5000;
			// default timeout value for DIALOG

	fprintf(stdout, "\nStarting operating system messages "
		"interaction for\n");
	fprintf(stdout, "partition %s (Ctrl-D to abort):\n",
		image->name);

	/* operating system messages interaction */

	DEBUG_PRINT("*** polling of operating system messages starts ***\n");
	child = fork();
	if (child<0) {
		perror("problem with fork\n");
		return FORK_PROBLEM;
	}
	if (child == 0) {
		/* this is the child process */
		poll_opsys_messages(image);
	} else
		DEBUG_PRINT("Child process is #%d.\n", child);

	ret = 0;
	tcgetattr(fileno(stdin), &initialrsettings);  // save stdin settings
	newrsettings = initialrsettings;
	newrsettings.c_lflag &= ~(ECHO | ICANON);     // no echo, no buffering
	newrsettings.c_cc[VMIN] = 1;  // set the minimum number of
				      // characters to receive before
				      // returning to stdin */
	newrsettings.c_cc[VTIME] = 0; // set the minimum time to wait
				      // for more characters to zero
	if (tcsetattr(fileno(stdin), TCSAFLUSH |TCSANOW, &newrsettings) != 0) {
		DEBUG_PRINT("Could not set attributes\n");
		ret = STDIN_PROBLEM;
		goto kill;
	}
	do {

		DEBUG_PRINT("reading commands from stdin...\n");

		/* poll for operating system messages */
		command_string[0] = '\0';
		current = 0;
		last = 0;
		command_char = fgetc(stdin);
		while (command_char != EOF) {
			if (command_char == 4) {
				/* CTRL-D */
				goto kill;
			}
			if (command_char == '\n') {
				/* reset cursor */
				command_string[last] = '\0';
				command_char = fputc('\r', stdout);
				break;
			}
			command_char = fputc(command_char, stdout);
			if (command_char == EOF) {
				DEBUG_PRINT("problem with fputc\n");
			}
			tcdrain(fileno(stdout));
			switch (command_char) {
			case '\b':
			case 127:
				/* Backspace, DEL: shift 1 character */
				handle_del(command_string, &current, &last);
				break;
			case 27:
				/* ESC: read next char */
				handle_esc(command_string, &current, &last);
				break;
			default:
				command_string[current] = command_char;
				if (current == last)
					last += 1;
				current += 1;
			}
			/* read next input */
			command_char = fgetc(stdin);
		}
		if (strlen(command_string)) {
			DEBUG_PRINT("sending command >%s<...\n",
				    command_string);
			ret = send_opsys_command(command_string, image);
		}
	} while (ret == 0);

kill:
	tcsetattr(fileno(stdin), TCSAFLUSH | TCSANOW, &initialrsettings);
	DEBUG_PRINT("** initiate end of polling of operat. system mess. **\n");

	fprintf(stdout, "\nAborting operating system messages interaction ");
	fprintf(stdout, "for partition %s.\n", image->name);

	kill(child, SIGKILL);  /* kill child process */

	DEBUG_PRINT("*** polling has stopped ***\n");
	return ret;
}


static int snipl_image_getstatus(struct snipl_image *image)
{
	int ret = 0;

	DEBUG_PRINT("status %d\n", image->priv->status);
	fprintf(stdout, "status of %s: ", image->name);
	if (image->priv->status & HWMCA_STATUS_OPERATING)
		fprintf(stdout, " operating");
	if (image->priv->status & HWMCA_STATUS_NOT_OPERATING)
		fprintf(stdout, " not_operating");
	if (image->priv->status & HWMCA_STATUS_NO_POWER)
		fprintf(stdout, " no_power");
	if (image->priv->status & HWMCA_STATUS_NOT_ACTIVATED)
		fprintf(stdout, " not_activated");
	if (image->priv->status & HWMCA_STATUS_EXCEPTIONS)
		fprintf(stdout, " exceptions");
	if (image->priv->status & HWMCA_STATUS_STATUS_CHECK)
		fprintf(stdout, " status_check");
	if (image->priv->status & HWMCA_STATUS_SERVICE)
		fprintf(stdout, " service");
	if (image->priv->status & HWMCA_STATUS_LINKNOTACTIVE)
		fprintf(stdout, " link_not_active");
	if (image->priv->status & HWMCA_STATUS_POWERSAVE)
		fprintf(stdout, " power_save");
	if (image->priv->status & HWMCA_STATUS_SERIOUSALERT)
		fprintf(stdout, " serious_alert");
	if (image->priv->status & HWMCA_STATUS_ALERT)
		fprintf(stdout, " alert");
	if (image->priv->status & HWMCA_STATUS_ENVALERT)
		fprintf(stdout, " env_alert");
	if (image->priv->status & HWMCA_STATUS_SERVICE_REQ)
		fprintf(stdout, " service_req");
	if (image->priv->status & HWMCA_STATUS_DEGRADED)
		fprintf(stdout, " degraded");
	if (image->priv->status & HWMCA_STATUS_STORAGE_EXCEEDED)
		fprintf(stdout, "storage_exceeded");
	if (image->priv->status & HWMCA_STATUS_LOGOFF_TIMEOUT)
		fprintf(stdout, " logoff_timeout");
	if (image->priv->status & HWMCA_STATUS_FORCED_SLEEP)
		fprintf(stdout, " forced_sleep");
	if (image->priv->status & HWMCA_STATUS_IMAGE_NOT_OPERATING)
		fprintf(stdout, " image_not_operating");
	if (image->priv->status & HWMCA_STATUS_IMAGE_NOT_ACTIVATED)
		fprintf(stdout, " image_not_activated");
	if (image->priv->status & HWMCA_STATUS_IMAGE_NOT_CAPABLE)
		fprintf(stdout, " image_not_capable");
	if (image->priv->status & HWMCA_STATUS_UNKNOWN)
		fprintf(stdout, " unknown");
	fprintf(stdout, "\n");
	return ret;
}


static char* errorMessage[] =
{
	/*******************************************************/
	/* Equals the Return code Values                       */
	/*   for the Hardware Management Console Data Exchange */
	/*   and for the Hardware Management Console Command   */
	/*******************************************************/
	[0]  "NO_ERROR - 0"             ,
	[1]  "NO_SUCH_OBJECT - 1"       ,
	[2]  "INVALID_DATA_TYPE - 2"    ,
	[3]  "INVALID_DATA_LENGTH - 3"  ,
	[4]  "INVALID_DATA_PTR - 4"     ,
	[5]  "INVALID_DATA_VALUE - 5"   ,
	[6]  "INVALID_INIT_PTR - 6"     ,
	[7]  "INVALID_ID_PTR - 7"       ,
	[8]  "INVALID_BUF_PTR - 8"      ,
	[9]  "INVALID_BUF_SIZE - 9"     ,
	[10] "INVALID_DATATYPE_PTR - 10",
	[11] "INVALID_TARGET (wrong IP address) - 11"      ,
	[12] "INVALID_EVENT_MASK - 12"  ,
	[13] "INVALID_PARAMETER - 13"   ,
	[14] "READ_ONLY_OBJECT - 14"    ,
	[15] "SNMP_INIT_ERROR - 15"     ,
	[16] "INVALID_OBJECT_ID - 16"   ,
	[17] "REQUEST_ALLOC_ERROR - 17" ,
	[18] "REQUEST_SEND_ERROR - 18"  ,
	[19] "TIMEOUT (may be also wrong community/password) - 19" ,
	[20] "REQUEST_RECV_ERROR - 20"  ,
	[21] "SNMP_ERROR - 21"          ,
	[22] "INVALID_TIMEOUT - 22"     ,
	[23] "INVALID_CMD - 23"         ,
	[24] "OBJECT_BUSY - 24"         ,
	[25] "INVALID_OBJECT - 25"      ,
	[26] "COMMAND_FAILED - 26 "     ,
	[27] "INITTERM_OK - 27"         ,
	[28] "INVALID_HOST / DISRUPTIVE_OK - 28",
	[29] "INVALID_COMMUNITY / PARTIAL_HW - 29"
};

static char *errorUnknown = "unknown error";

static char *getErrorMessage(const ULONG errorNumber)
{
	if (errorNumber < DIMOF(errorMessage))
		return errorMessage[errorNumber];
	else
		return errorUnknown;
}


#if SNIPL_DEBUG
static void print_event_buffer(const HWMCA_DATATYPE_P pdata)
{
	HWMCA_DATATYPE_P temp_pdata;
	char *char_ptr;
	unsigned int *int_ptr;
	unsigned int i, len;
	int_ptr = (unsigned int *)pdata;

	if (!int_ptr)
		return;
	fprintf(stdout, "data returned:\n");
	fprintf(stdout, "%.8x ", (unsigned int)int_ptr);  /* print address */
	for (i = 0; i < 100; i++)    {
		fprintf(stdout, "%.8x ", *int_ptr);  /* print data */
		int_ptr = int_ptr++;
		if ((i+1)%8 == 0) {
			fprintf(stdout, "\n");
			/* print address */
			fprintf(stdout, "%.8x ", (unsigned int)int_ptr);
		}
	}
	fprintf(stdout, "\n");
	temp_pdata = pdata;
	fprintf(stdout, "values returned within data:\n");
	while (temp_pdata) {
		char_ptr = (char *)temp_pdata->pData;
		len = temp_pdata->ulLength;
		for (i = 0; i < len; i++) {
			fprintf(stdout, "%.2x", (int)*char_ptr);
			char_ptr = char_ptr++;
			if ((i+1)%4 == 0)
				printf(" ");
		}
		fprintf(stdout, "\n");
		fprintf(stdout, "pData in Prosa: %s\n",
			(char *)temp_pdata->pData);
		temp_pdata = temp_pdata->pNext;
	}
	fflush(stdout);
	return;
}
#endif
