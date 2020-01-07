/*
 * snipl.h : header file for lpar API for snIPL (simple network IPL)
 *
 * Copyright IBM Corp. 2002, 2016
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

#define SNIPL_VERSION "snipl - Linux Image Control - version 3.1.0"
#define SNIPL_COPYRIGHT "Copyright IBM Corp. 2001, 2016"

// Simple Error handling macro (print)
#ifdef SNIPL_DEBUG
   #define DEBUG_PRINT(args...) \
	do { \
		fprintf(stderr, "*** in %s: ", __func__);\
		fprintf(stderr, args);\
		fflush(stderr);\
	} while (0)
#else
   #define DEBUG_PRINT(args...)
#endif
#define HEXDUMP16(importance,header,ptr) \
fprintf(importance, header "1: %02x %02x %02x %02x  %02x %02x %02x %02x  " \
                "%02x %02x %02x %02x  %02x %02x %02x %02x\n", \
	        *(((char*)ptr)),*(((char*)ptr)+1),*(((char*)ptr)+2), \
	        *(((char*)ptr)+3),*(((char*)ptr)+4),*(((char*)ptr)+5), \
	        *(((char*)ptr)+6),*(((char*)ptr)+7),*(((char*)ptr)+8), \
	        *(((char*)ptr)+9),*(((char*)ptr)+10),*(((char*)ptr)+11), \
	        *(((char*)ptr)+12),*(((char*)ptr)+13), \
	        *(((char*)ptr)+14),*(((char*)ptr)+15)); \
fprintf(importance, header "2: %02x %02x %02x %02x  %02x %02x %02x %02x  " \
                "%02x %02x %02x %02x  %02x %02x %02x %02x\n", \
		*(((char*)ptr)+16),*(((char*)ptr)+17), \
		*(((char*)ptr)+18),*(((char*)ptr)+19), \
		*(((char*)ptr)+20),*(((char*)ptr)+21), \
		*(((char*)ptr)+22),*(((char*)ptr)+23), \
		*(((char*)ptr)+24),*(((char*)ptr)+25), \
		*(((char*)ptr)+26),*(((char*)ptr)+27), \
		*(((char*)ptr)+28),*(((char*)ptr)+29), \
		*(((char*)ptr)+30),*(((char*)ptr)+31));

#define DIMOF(a)	(sizeof(a)/sizeof(a[0]))

// snipl return codes
#define UNKNOWN_PARAMETER        1
#define INVALID_PARAMETER_VALUE  2
#define DUPLICATE_OPTION         3
#define CONFLICTING_OPTIONS      4
#define NO_COMMAND               5
#define MISSING_SERVER           6
#define MISSING_IMAGENAME        7
#define MISSING_USERID           8
#define MISSING_PASSWORD         9
#define SERVER_IMAGE_MISMATCH   10
#define MORE_THAN_ONE_IMAGE     22
#define NO_USERNAME		24
#define USERNAME_ENC_OFF	25
#define LIB_LOAD_PROBLEM        30
#define FORK_PROBLEM            40
#define STDIN_PROBLEM           41
#define HWMCA_PROBLEM           50
#define BUFFER_OVERFLOW         60
#define STORAGE_PROBLEM         90
#define INTERNAL_ERROR          99
#define CONNECTION_ERROR       100

#define UNDEFINED		-1

/**********************************************************************
 * image objects and access methods
 *
 * A single instance of an operating system image that
 * can be managed.
 **********************************************************************/

/*
 * possible image operations
 */
enum   image_op {	/* to prepare server for this operation	 */
	OPUNKNOWN,
	RESET,
	ACTIVATE,
	DEACTIVATE,
	STOP,		/* LPAR only */
	LOAD,		/* LPAR only */
	SCSILOAD,	/* LPAR only */
	SCSIDUMP,	/* LPAR only */
	DIALOG,		/* LPAR only */
	LIST,		/* LPAR only */
	GETSTATUS,
};

/*
 * possible image attributes
 */
struct snipl_parms {
	int    force;			/* 0=no,1=yes,-1=undefined */
	char  *profile;
	char  *load_address;
	char  *load_parms;
	short  clear;			/* 0=no,1=yes,-1=undefined */
	short  store_stat;		/* 0=no,1=yes,-1=undefined */
	short  load_timeout;		/* -1=undefined */
	char   scsiload_wwpn[17];
	char   scsiload_lun[17];
	short  scsiload_bps;		/* -1=undefined */
	char  *scsiload_ossparms;
	char   scsiload_bootrec[17];
	char  *msgfilename;
	int    msg_timeout;		/* -1=undefined */
	int    image_op;
	int    shutdown_time;
};

/*
 * image attributes that are set by the specific
 * server module (e.g. for type LPAR)
 */

struct snipl_image_private;

/*
 * image object. There is a collection of it in the server
 */
struct snipl_image {
	char *name;
	char *alias;
	struct snipl_server	*server;	/* backreference */
	struct snipl_image_ops	*ops;
	struct snipl_image	*_next;
	struct snipl_image_private *priv;
};

/*
 * image functions. These are defined per image type
 */
struct snipl_image_ops {
	int (*reset)     (struct snipl_image *);
	int (*activate)	 (struct snipl_image *);
	int (*deactivate)(struct snipl_image *);
	int (*stop)	 (struct snipl_image *);
	int (*load)	 (struct snipl_image *);
	int (*scsiload)  (struct snipl_image *);
	int (*scsidump)  (struct snipl_image *);
	int (*dialog)	 (struct snipl_image *);
	int (*getstatus) (struct snipl_image *);
};

/* access method to reset an image */
static inline int snipl_reset(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->reset) ?
		simg->ops->reset(simg) : -1;
}

/* access method to power on an image */
static inline int snipl_activate(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->activate) ?
		simg->ops->activate(simg) : -1;
}

/* access method to power off an image */
static inline int snipl_deactivate(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->deactivate) ?
		simg->ops->deactivate(simg) : -1;
}

/* access method to stop all cpus of an image */
static inline int snipl_stop(struct snipl_image *simg)
{	return (simg && simg->ops && simg->ops->stop) ?
		simg->ops->stop(simg) : -1;
}

/* access method to load an operating system into the image */
static inline int snipl_load(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->load) ?
		simg->ops->load(simg) : -1;
}

/* access method to scsiload an operating system into the image */
static inline int snipl_scsiload(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->scsiload) ?
		simg->ops->scsiload(simg) : -1;
}

/* access method to scsidump an operating system into the image */
static inline int snipl_scsidump(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->scsidump) ?
		simg->ops->scsidump(simg) : -1;
}

/* access method to enter console interaction mode */
static inline int snipl_dialog(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->dialog) ?
		simg->ops->dialog(simg) : -1;
}

/* access method to retrieve status information about an image */
static inline int snipl_getstatus(struct snipl_image *simg)
{
	return (simg && simg->ops && simg->ops->getstatus) ?
		simg->ops->getstatus(simg) : -1;
}


/**********************************************************************
 * server objects and access methods
 *
 * A server that is connected to and manages one or more
 * instances of snipl_image.
 **********************************************************************/

struct snipl_server_ops;
struct snipl_server_private;

/*
 * server object
 */
struct snipl_server {
	char *address;
	char *user;
	char *password;
	char *type;
	char *sslfingerprint;
	int   timeout;
	int   port;
	int   enc;
	_Bool password_prompt;
	struct snipl_parms parms;
	char *problem;		/* called function puts errorstring here */
	int   problem_class;	/* problem class : WARNING, ERROR, ... */
	void *module_handle;
	struct snipl_server_ops *ops;
	struct snipl_server_private *priv;
	struct snipl_server *_next;
	struct snipl_image *_images;
};

/*
 * server functions. These are defined per server type
 */
struct snipl_server_ops {
	int (*logout)(struct snipl_server *);
	int (*login)(struct snipl_server *);
	int (*check)(struct snipl_server *);
	int (*confirm)(struct snipl_server *);
};

/*
 * server iterator over all "contained" images
 */
#define snipl_for_each_image(serv, image) \
	for (image = snipl_next(serv, 0); image; \
	     image = snipl_next(serv, image))

/*
 * prepare loads type specific module and initializes structures
 * (function pointers) according to server type
 */
extern int snipl_prepare(struct snipl_server *);

/*
 * prepare_check checks type specific parameters
 * This is done by the specific module function loaded by snipl_prepare.
 */
static inline int snipl_prepare_check(struct snipl_server *sserv)
{
	return (sserv && sserv->ops && sserv->ops->check) ?
		sserv->ops->check(sserv) : -1;
}

/* log in to the server */
static inline int snipl_login(struct snipl_server *sserv)
{
	return (sserv && sserv->ops && sserv->ops->login) ?
		sserv->ops->login(sserv) : -1;
}

/* log off from the server and free resources */
static inline int snipl_logout(struct snipl_server *sserv)
{
	return (sserv && sserv->ops && sserv->ops->logout) ?
		sserv->ops->logout(sserv) : -1;
}

/*
 * Confirm connection to a server.
 * Return 1, if the connection is confirmed, otherwise, if the function is
 * implemented returns 0. If the function is not implemented for server
 * returns -1.
 */
static inline int snipl_confirm(struct snipl_server *sserv)
{
	return (sserv && sserv->ops && sserv->ops->confirm) ?
		sserv->ops->confirm(sserv) : -1;
}

/**********************************************************************
 * configuration objects and access methods
 *
 * A representation of the configuration file, containing one or
 * more instances of snipl_server;
 *********************************************************************/

/*
 * the configuration object
 */
struct snipl_configuration {
	const char *filename;
	char *problem;
	int   problem_class;

	/* internal data below */
	char *_buffer;			/* data buffer for all strings */
	struct snipl_server *_servers;	/* first member in list */
	struct snipl_server *_last;	/* last member in list */
};

/* create a configuration object from a file */
extern struct snipl_configuration *snipl_configuration_from_file
					(const char *);

extern struct snipl_configuration *snipl_configuration_from_line
					(const char *);

/* free resources for the configuration */
extern void snipl_configuration_free(struct snipl_configuration *);

/* return the following image */
extern struct snipl_image *snipl_next(struct snipl_server *,
				      struct snipl_image *);

/* search through configuration for image and return containing snipl_server */
extern struct snipl_server *find_next_server(struct snipl_configuration *,
				 const char *,
				 const char *,
				 struct snipl_server*);

/* search through config. for image and return first matching image object */
/* the server containiner can be reached by the server pointer field */
extern struct snipl_image *find_next_image(struct snipl_configuration *,
				const char *,
				const char *,
				struct snipl_server *);

extern
struct snipl_server *find_server_with_address(struct snipl_configuration *,
				struct snipl_server *, struct snipl_server *);

extern char *get_config_file_name(const char *);

extern int parms_check_vm(struct snipl_server *);

extern int isscanf_ok(int, char, char *);

/*
 * configurator iterator over all "contained" servers
 */
#define snipl_for_each_server(conf, serv) \
	for (serv = conf->_servers; serv; serv = serv->_next)

/*
 * special configurator iterator over all "contained" servers
 * beginning with the next server after the passed one if not NULL
 */
#define snipl_for_each_next_server(conf, serv) \
	for (serv = (serv ? serv->_next : conf->_servers); serv; \
	     serv = serv->_next)


extern void create_msg(struct snipl_server *, const char *, ...)
		       __attribute__((format(printf, 2, 3)));

/*
 * problem class used in server and configuration
 * object to rank the error
 */
enum problem_class {
	OK,
	WARNING,
	FATAL,
	CERTIFICATE_ERROR,
};
