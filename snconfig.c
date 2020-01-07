/*
   snconfig.c - configuration file handling for stonith/snipl

   Copyright IBM Corp. 2003, 2016
   Author(s): Arnd Bergmann      (arndb@de.ibm.com)
              Peter Tiedemann    (ptiedem@de.ibm.com)
   Maintainer: Ursula Braun      (ursula.braun@de.ibm.com)

   Published under the terms and conditions of the CPL (common public license)

   PLEASE NOTE:
   snconfig is provided under the terms of the enclosed common public license
   ("agreement"). Any use, reproduction or distribution of the program
   constitutes recipient's acceptance of this agreement.

*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "snipl.h"

enum conf_key {
	/* accepted keys */
	SERVER,
	USER,
	PASSWORD,
	TYPE,
	PORT,
	IMAGE,
	CPCID,
	ENCRYPTION,
	SSLFINGERPRINT,
	/* special keys */
	UNKNOWN, /* number of valid words */
	NR_KEYWORDS = UNKNOWN,
};

static const char *conf_keywords[NR_KEYWORDS] = {
	[SERVER]	"server",
	[USER]		"user",
	[PASSWORD]	"password",
	[TYPE]		"type",
	[PORT]		"port",
	[IMAGE]		"image",
	[CPCID]		"cpcid",
	[ENCRYPTION]	"encryption",
	[SSLFINGERPRINT] "sslfingerprint"
};


/*
 * function: snipl_next
 *
 * purpose: return the following image
 */
struct snipl_image *snipl_next(struct snipl_server *server,
				      struct snipl_image *image)
{
	if (!image)
		return server->_images;
	else
		return image->_next;
}


static enum conf_key
conf_identify_key(const char *s)
{
	enum conf_key key;
	for (key = 0; key < NR_KEYWORDS; key++) {
		if (0 == strcasecmp(s, conf_keywords[key]))
			return key;
	}
	return UNKNOWN;
}


static inline void
set_cfg_error(struct snipl_configuration *conf, char *text, int severity,
	      int alloc_flag)
{
	free(conf->problem);
	if (alloc_flag)
		conf->problem = strdup(text);
	else
		conf->problem = text;
	conf->problem_class = severity;
}


static int
conf_new_server(struct snipl_configuration *conf, char *value)
{
	struct snipl_server *serv;

	DEBUG_PRINT("snconfig : start of function\n");

	serv = calloc(1,sizeof(*serv));
	if (!serv) {
		set_cfg_error(conf, "out of memory", FATAL, 1);
		return FATAL;
	}

	*serv = (struct snipl_server){
		.address = value,
		.problem_class=OK,
		.enc = UNDEFINED,
		.port = UNDEFINED,

	};

	serv->parms = (struct snipl_parms) {
		.force = -1,
		.clear = -1,
		.store_stat = -1,
		.load_timeout = -1,
		.scsiload_bps = -1,
		.msg_timeout = -1,
		.image_op = OPUNKNOWN,
	};

	if (conf->_last == NULL) {
		conf->_servers = serv;
	} else {
		conf->_last->_next = serv;
	}
	conf->_last = serv;
	return 0;
}

static int
conf_new_image(struct snipl_configuration *conf, char *value)
{
	struct snipl_image *image;
	char *alias;

	DEBUG_PRINT("snconfig : start of function\n");

	if (!conf->_servers) {
		set_cfg_error(conf, "no server defined yet", FATAL, 1);
		return FATAL;
	}

	image = calloc(1,sizeof(*image));
	if (!image) {
		set_cfg_error(conf, "out of memory", FATAL, 1);
		return FATAL;
	}

	alias = strchr(value, '/');
	if (alias) {
		*alias = '\0';
		++alias;
	} else alias = value;

	*image = (struct snipl_image) {
		.name = value,
		.alias = alias,
                .server = conf->_last,
		._next = conf->_last->_images,	// conf->_last points to the
		                                // actual server def and
						// conf->_last->_images to its
						// actual image list head.
	};

	conf->_last->_images = image;
	return 0;
}

static _Bool isID(char * s)
{	char c;
	if (s == NULL) return 0;
	if (!isalpha(*s++)) return 0;
	while ((c = *s++))
		if (!isalnum(c)) return 0;

	return 1;
}

/*
 * check if parameter is a positiv number
 */
static int ispnum(char *s)
{
	while (*s)
		if (!isdigit(*s++))
			return 0;
	return 1;
}

/*
 * check result of parameter parsing with sscanf
 */
int isscanf_ok(int ret, char next_char, char *arg)
{
	if (!ret ||
	    (ret == 2 &&
	     next_char != ' ' &&
	     next_char != '\0') ||
	     !ispnum(arg))
		return 0;
	return 1;
}

/*
 * systems_unique checks the configuration for uniqueness of
 * LPAR system addresses (required) and for uniqueness of
 * VM system/user combination (also required).
 * returns 1 if systems or system/user 's are unique
 *		0 otherwise together with detailed information in
 *		the config->problem field.
 *
 * Note that this function contains knowledge about VM and LPAR
 * things which should better be in lower-level modules.
 * But this knowledge is required before these modules are loaded.
 */
static void check_systems_unique(struct snipl_configuration *conf)
{
	struct snipl_server *serv1 = NULL;
	struct snipl_server *serv2;
	char * err = strdup("");

	DEBUG_PRINT("snconfig : start of function\n");

	snipl_for_each_server(conf, serv1) {
		if (serv1->type == NULL){
			if (asprintf(&err, "missing type for server %s",
				serv1->address) > 0)
				set_cfg_error(conf, err, FATAL, 0);
			return;
		}
		serv2 = serv1;
		snipl_for_each_next_server(conf, serv2) {
			if (0 == strcasecmp(serv1->address,serv2->address)) {
				if (0 == strcasecmp("lpar",serv1->type)) {
					if (asprintf(&err, "duplicate LPAR "
						" server def. detected : %s",
						serv1->address) > 0)
						set_cfg_error(conf, err,
							FATAL, 0);
					return;
				}
				else if ((0 == strcasecmp("vm",serv1->type)) &&
					 (serv1->user) && (serv2->user) &&
					 (0 == strcasecmp(serv1->user,
							  serv2->user))) {
					if (asprintf(&err, "duplicate VM "
						"server/user def. detected : "
						" %s/%s", serv1->address,
						serv1->user) > 0)
						set_cfg_error(conf, err,
							FATAL, 0);
					return;
				}
			}
		}
	}
	free(err);
	return;
}

static int
conf_set_attribute(struct snipl_configuration *conf, enum conf_key key,
		  char *val)
{
	struct snipl_server *serv;
	char **oldval = NULL;
	int *oldint = NULL;
	char next_char;
	int temp_ret;
	int valint;

	serv = conf->_last;
	if (!serv) {
		set_cfg_error(conf, "no server defined yet", FATAL, 1);
		return FATAL;
	}

	switch (key) {
	case USER:
		oldval = &serv->user;
		if (!isID(val)) {
			set_cfg_error(conf, "non-proper user id", FATAL, 1);
			return FATAL;
		}
		break;
	case PASSWORD:
		oldval = &serv->password;
		if (!val) {
			set_cfg_error(conf, "non-proper password", FATAL, 1);
			return FATAL;
		}
		break;
	case TYPE:
		oldval = &serv->type;
		if (!isID(val)) {
			set_cfg_error(conf, "non-proper server type", FATAL, 1);
			return FATAL;
		}
		break;
	case PORT:
		oldint = &serv->port;
		if (!val) {
			set_cfg_error(conf, "non-proper port", FATAL, 1);
			return FATAL;
		}
		temp_ret = sscanf(val, "%5i%1c", &valint, &next_char);
		if (!isscanf_ok(temp_ret, next_char, val)) {
			set_cfg_error(conf, "non-proper port", FATAL, 1);
			return FATAL;
		}
		if (valint > 65535) {
			set_cfg_error(conf, "non-proper port", FATAL, 1);
			return FATAL;
		}
		break;
	case ENCRYPTION:
		oldint = &serv->enc;
		if (!val) {
			set_cfg_error(conf,
				      "non-proper encryption value (yes|no)",
				      FATAL, 1);
			return FATAL;
		}
		if (strcasecmp("yes", val) == 0) {
			valint = 1;
		} else if (strcasecmp("no", val) == 0) {
			valint = 0;
		} else {
			set_cfg_error(conf,
				      "non-proper encryption value (yes/no)",
				      FATAL, 1);
			return FATAL;
		}
		break;
	case SSLFINGERPRINT:
		oldval = &serv->sslfingerprint;
		break;
	default:
		set_cfg_error(conf, "unknown key for conf_set_attribute", FATAL,
			      1);
		return FATAL;
	};

	if ((oldval && *oldval) ||
	    (oldint && *oldint != UNDEFINED)) {
		set_cfg_error(conf, "already defined", WARNING, 1);
		return WARNING;
	}
	if (oldval)
		*oldval = val;
	if (oldint)
		*oldint = valint;

	return 0;
};


/*      function: replace_char
 *
 *      purpose: replace character "old" by character "new"
 **/
static inline void
replace_char(char *buffer, unsigned char old, unsigned char new)
{
	unsigned char *character;

	if (buffer) {
		character = (unsigned char *)strchr(buffer, old);
		if (character)
			*character = new;
	}
}


static int
conf_parse_line(struct snipl_configuration *conf, char *buffer, regex_t *re)
{
	regmatch_t pmatch[3];
	char *key, *value;
	enum conf_key keyval;

	DEBUG_PRINT("snconfig : start of function\n");

	if (regexec(re, buffer, 3, pmatch, 0) == REG_NOMATCH
	    || pmatch[1].rm_so == -1
	    || pmatch[2].rm_so == -1) {
		set_cfg_error(conf, "syntax error", FATAL, 1);
		return FATAL;
	}

	key   = buffer + pmatch[1].rm_so;
	buffer[pmatch[1].rm_eo] = '\0';
	value = buffer + pmatch[2].rm_so;
	buffer[pmatch[2].rm_eo] = '\0';

	keyval = conf_identify_key(key);
	switch (keyval) {
	case SERVER:
		return conf_new_server(conf, value);
	case IMAGE:
		replace_char(value, '-', 0x0a);
		return conf_new_image(conf, value);
	case CPCID:	/* ignore the sncap cpcid attribute */
		return 0;
	case UNKNOWN:
		set_cfg_error(conf, "unknown keyword", WARNING, 1);
		return WARNING;
	default:
		return conf_set_attribute(conf, keyval, value);
	}
}

static void
conf_parse_buffer(struct snipl_configuration *conf, char *pattern)
{
	static char regerror_string[64];
	static const char token_pattern[] =
			"^[[:blank:]]*([[:alpha:]]+)[[:blank:]]*"
			"=[[:blank:]]*([^[:blank:]]+)";
	char*	pos;
	char*	end;
	regex_t line;
	regex_t token;
	regmatch_t pmatch[2];
	int	lineno;
	int	ret;
	char*	problem = strdup("");
	char*   oldproblem;

	DEBUG_PRINT("snconfig : start of function\n");

	/* process expressions */
	ret = regcomp(&line, pattern, REG_EXTENDED | REG_NEWLINE);
	if (ret) {
		regerror(ret, &line, regerror_string, sizeof(regerror_string));
		set_cfg_error(conf, regerror_string, FATAL, 1);
		return;
	}
	ret = regcomp(&token, token_pattern, REG_EXTENDED | REG_NEWLINE);
	if (ret) {
		regerror(ret, &token, regerror_string, sizeof(regerror_string));
		set_cfg_error(conf, regerror_string, FATAL, 1);
		return;
	}

	lineno = 1;
	pos = conf->_buffer;
	end = pos + strlen(conf->_buffer);

	/* scan input line by line */
	for (; pos < end; pos += pmatch[0].rm_eo+1) {
		if (regexec(&line, pos, 2, pmatch, 0) == REG_NOMATCH)
			break;

		if (pmatch[1].rm_so != -1) {
			/* non-empty line found, parse it */
			pos[pmatch[1].rm_eo] = '\0';
			ret = conf_parse_line(conf, pos, &token);
		}

		/* XXX: do this message chaining better : */
		switch (ret) {
		case WARNING:
			oldproblem = problem;
			if (asprintf(&problem, "%sWARNING: %s:%d: %s: `%s'\n",
					oldproblem,
					conf->filename,
					lineno,
					conf->problem,
					pos) > 0)
				free(oldproblem);
			else
				problem = oldproblem;
			conf->problem_class = WARNING;
			break;
		case FATAL:
			oldproblem = problem;
			if (asprintf(&problem, "%sFATAL: %s:%d: %s: `%s'\n",
					oldproblem,
					conf->filename,
					lineno,
					conf->problem,
					pos) > 0)
				free(oldproblem);
			else
				problem = oldproblem;
			conf->problem_class = FATAL;
			goto out;
		}
		lineno++;
	}

out:
	regfree(&line);
	regfree(&token);
	set_cfg_error(conf, problem, conf->problem_class, 0);
	if (conf->problem_class != FATAL)
		/* let other FATAL problems come first */
		check_systems_unique(conf);
		/* could update conf->problem and conf->problem_class */
}

/* allocate buffer and read complete file to that buffer,
 * returns 0 on success or error code. */
static int
conf_read_to_buffer(const char *filename, char **buffer)
{
	char *pos;
	struct stat statbuf;
	int fd;
	int ret;
	size_t size;

	DEBUG_PRINT("snconfig : start of function\n");

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return errno;

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		ret = errno;
		goto out;
	}

	size = statbuf.st_size;
	*buffer = calloc(1,size + 1);
	if (!*buffer) {
		ret = ENOMEM;
		goto out;
	}

	pos = *buffer;
	do {
		ret = read(fd, pos, size);
		if (ret < 0) {
			ret = errno;
			goto out;
		}
		pos += ret;
		size -= ret;
	} while (ret);
	*pos = '\0';

out:
	close(fd);
	return ret;
}


struct snipl_configuration *
snipl_configuration_from_file (const char *filename)
{
	struct snipl_configuration *conf;
	int ret;

	DEBUG_PRINT("snconfig : start of function\n");

	conf = calloc(1,sizeof(struct snipl_configuration));
	if (!conf)
		return NULL;

	*conf = (struct snipl_configuration) {
		.filename = filename,
		.problem_class=OK,
	};

	ret = conf_read_to_buffer(filename, &conf->_buffer);
	if (ret) {
		set_cfg_error(conf, strerror(ret), FATAL, 1);
		return conf;
	}

	conf_parse_buffer(conf, "([^[:space:]#]+[[:blank:]]*)?(#.*)?$");

	return conf;
}

struct snipl_configuration *
snipl_configuration_from_line(const char *line)
{
	struct snipl_configuration *conf;

	DEBUG_PRINT("snconfig : start of function\n");

	conf = calloc(1,sizeof(struct snipl_configuration));
	if (!conf)
		return NULL;

	*conf = (struct snipl_configuration) {
		.filename = "<user input>",
		._buffer = strdup(line),
		.problem_class=OK,
	};

	conf_parse_buffer(conf, "([^,]*)");

	return conf;
}

void snipl_configuration_free(struct snipl_configuration *conf)
{
	struct snipl_server *serv;
	struct snipl_server *trash_server;
	struct snipl_image  *image;
	struct snipl_image  *trash_image;

	DEBUG_PRINT("snconfig : start of function\n");

	if (conf == NULL)
		return;
	trash_server = NULL;
	snipl_for_each_server(conf, serv) {
		trash_image = NULL;
		snipl_for_each_image(serv, image) {
			free(trash_image);
			trash_image = image;
		}
		free(trash_image);
		free(trash_server);
		trash_server = serv;
	}
	free(trash_server);
	free(conf->problem);
	free(conf->_buffer);
	free(conf);
}


/*
 *	search for the server definition containing
 *	the specified image_name and user (which may also be NULL)
 *	search begins at the next of the passed snipl_server reference which may
 *	be NULL to start at the beginning.
 */
struct snipl_server* find_next_server(struct snipl_configuration *conf,
				 const char *image_name,
				 const char *user,
				 struct snipl_server *serv)
{
	struct snipl_image  *image;

	if (user){
		snipl_for_each_next_server(conf, serv) {
			if (!serv->user ||
			    (serv->user &&
			    (0 == strcasecmp(user, serv->user))))
				snipl_for_each_image(serv, image) {
					if ((0 == strcasecmp(image->alias,
							     image_name)) ||
					    (0 == strcasecmp(image->name,
							     image_name)))
						return serv;
			}
		}
	}
	else {
		snipl_for_each_next_server(conf, serv) {
			snipl_for_each_image(serv, image) {
				if ((0 == strcasecmp(image->alias, image_name))
				 || (0 == strcasecmp(image->name, image_name)))
					return serv;
			}
		}
	}
	return NULL; /* server not found */
}

/*
 *	search for the server definition containing
 *	the specified image_name and user (which may also be Null)
 *	search begins at the next of the passed snipl_server reference which may
 *	be NULL to start at the beginning.
 */
struct snipl_image *find_next_image(struct snipl_configuration *conf,
				    const char *img_name,
				    const char *user,
				    struct snipl_server *serv)
{
	struct snipl_image  *image;
	char * image_name = strdup(img_name);
	replace_char(image_name, '-', 0x0a);

	snipl_for_each_next_server(conf, serv) {
		if ((user == NULL) || (0 == strcasecmp(user, serv->user)))
			snipl_for_each_image(serv, image) {
				if ((0 == strcasecmp(image->alias, image_name))
				 || (0 == strcasecmp(image->name, image_name)))
					goto clean_exit;
			}
	}
	image = NULL;

clean_exit:
	free(image_name);
	return image;
}


/*
 *	function: find_server_with_address
 *
 *	purpose: find a server within conf with the same server address,
 *               userid and image as in server
 */
struct snipl_server *find_server_with_address(struct snipl_configuration *conf,
					      struct snipl_server *server,
					      struct snipl_server *loop_server)
{
	do {
		loop_server = find_next_server(conf, server->_images->name,
					       server->user, loop_server);
		if ((loop_server) &&
		(!strcasecmp(server->address, loop_server->address)))
			return loop_server;
	}
	while (loop_server);
	return NULL;
}


/*
 *	function: configfile_handling helper function
 *
 *	purpose: return name of accessible configuration file
 *               according config file search priority
 */
/*****************************************************************/
#define SNIPL_HOME_CFG "/.snipl.conf"
#define SNIPL_ETC_CFG  "/etc/snipl.conf"
char * get_config_file_name(const char *cfgname)
{
	char * home = getenv("HOME");

	DEBUG_PRINT("snconfig : start of function\n");

	if (cfgname) {
		if (0 == access(cfgname, R_OK))
			return strdup(cfgname);
	}

	char * cname = alloca(strlen (home) + strlen(SNIPL_HOME_CFG) + 1);
	if (cname == NULL) return NULL;
	strcpy(cname,home);
	cname = strcat(cname, SNIPL_HOME_CFG);
	if (0 == access(cname, R_OK))
		return strdup(cname);

	cname = SNIPL_ETC_CFG;
	if (0 == access(cname, R_OK))
		return strdup(cname);

	return NULL;
}


int parms_check_vm(struct snipl_server *server)
{
	server->problem_class = FATAL;
	if (server->user == NULL) {
		create_msg(server, "Error: missing Userid for VM server %s\n",
			   server->address);
		return MISSING_USERID;
	}
	if (server->password == NULL) {
		create_msg(server, "Error: missing Password for VM server %s\n",
			   server->address);
		return MISSING_PASSWORD;
	}
	if ((server->parms.force != -1) &&
	    (server->parms.image_op != DEACTIVATE)) {
		create_msg(server, "For a VM-type server, option force may "
			   "only be specified in conjunction with the "
			   "deactivate command\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.profile != NULL) {
		create_msg(server, "option profile must not be "
			   "specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.load_address != NULL) {
		create_msg(server, "option load_address must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.load_parms != NULL) {
		create_msg(server, "option load_parms must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.clear != -1) {
		create_msg(server, "option noclear must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.store_stat != -1) {
		create_msg(server, "option storestatus must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.load_timeout != -1) {
		create_msg(server, "option load_timeout must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.msg_timeout != -1) {
		create_msg(server, "option msg_timeout must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.scsiload_wwpn[0]) {
		create_msg(server, "option scsiload_wwpn must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.scsiload_lun[0]) {
		create_msg(server, "option scsiload_lun must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.scsiload_bps != -1) {
		create_msg(server, "option scsiload_bps must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.scsiload_ossparms != NULL) {
		create_msg(server, "option scsiload_ossparms must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.scsiload_bootrec[0]) {
		create_msg(server, "option scsiload_bootrec must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.image_op == STOP) {
		create_msg(server, "STOP operation must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.image_op == LOAD) {
		create_msg(server, "LOAD operation must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.image_op == DIALOG) {
		create_msg(server, "DIALOG operation must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.image_op == LIST) {
		create_msg(server, "LIST operation must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.image_op == SCSILOAD) {
		create_msg(server, "SCSILOAD operation must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}
	if (server->parms.image_op == SCSIDUMP) {
		create_msg(server, "SCSIDUMP operation must not be "
			"specified for a VM-type server\n");
		return CONFLICTING_OPTIONS;
	}

	server->problem_class = OK;
	return 0;
}
