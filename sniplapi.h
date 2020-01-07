/*
 * sniplapi.h : header file for LPAR API for snIPL (simple network IPL)
 *
 * Copyright IBM Corp. 2010, 2011
 * Author:    Ursula Braun       (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */

#define RET_PLUS        2000    /* constant to add to sniplapi return codes   */
#define BUFSIZE        10000    /* buffersize used to communicate with HMC/SE */

static int snipl_lpar_prepare_check(struct snipl_server *);
static int snipl_lpar_logout(struct snipl_server *);
static int snipl_lpar_login(struct snipl_server *);
static int snipl_image_reset(struct snipl_image *);
static int snipl_image_activate(struct snipl_image *);
static int snipl_image_deactivate(struct snipl_image *);
static int snipl_image_stop(struct snipl_image *);
static int snipl_image_load(struct snipl_image *);
static int snipl_image_scsiload(struct snipl_image *);
static int snipl_image_scsidump(struct snipl_image *);
static int snipl_image_dialog(struct snipl_image *);
static int snipl_image_getstatus(struct snipl_image *image);

static struct snipl_image_ops snipl_image_ops = {    /* image operations */
	.reset = snipl_image_reset,
	.activate = snipl_image_activate,
	.deactivate = snipl_image_deactivate,
	.stop = snipl_image_stop,
	.load = snipl_image_load,
	.scsiload = snipl_image_scsiload,
	.scsidump = snipl_image_scsidump,
	.dialog = snipl_image_dialog,
	.getstatus = snipl_image_getstatus
};

static struct snipl_server_ops snipl_server_ops = {  /* server operations */
	.logout  = snipl_lpar_logout,
	.login   = snipl_lpar_login,
	.check   = snipl_lpar_prepare_check
};

struct snipl_server_private {                  /* private server info         */
	HWMCA_DATATYPE_P        snmp_data_p;   /* buffer HMC/SEcommunication  */
	HWMCA_DATATYPE_P        snmp_notify_p; /* buffer DIALOG HwmcaWaitEvent*/
	HWMCA_INITIALIZE_T      snmp_command;  /* initialize struct. commands */
	HWMCA_INITIALIZE_T      snmp_notify;   /* initialize struct. notific. */
	HWMCA_SNMP_TARGET_T     snmp_ctarget;  /* target struct. commands     */
	HWMCA_SNMP_TARGET_T     snmp_ntarget;  /* target struct. notifications*/
	unsigned long           bufsize;       /* size of snmp_data_p buffer  */
	int                     msgfile;       /* console message file        */
};

struct snipl_image_private {                   /* private image info          */
	char image_object[HWMCA_MAX_ID_LEN];
					 /* image group object of console API */
	HWMCA_DATATYPE_T data[8];              /* data area for HwmcaCommands */
	int data_index;                        /* used part of data           */
	char *command_id;                      /* HwmcaCommand identification */
	char *command_name;                    /* HwmcaCommand in prosa       */
	unsigned int status;                   /* image status                */
};

static int parse_command_response(const HWMCA_DATATYPE_P,
				  const char *,
				  const char *,
				  const int ,
				  struct snipl_image *);

static char *getErrorMessage(const unsigned long errorNumber);

#ifdef SNIPL_DEBUG
static void print_event_buffer(HWMCA_DATATYPE_P);
static void print_datatype(const HWMCA_DATATYPE_T *);
#else
static inline void print_event_buffer(HWMCA_DATATYPE_P pdata) {}
static inline void print_datatype(const HWMCA_DATATYPE_T *data) {}
#endif
