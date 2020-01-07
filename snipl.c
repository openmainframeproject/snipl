/*
   snIPL - simple network IPL

   Copyright IBM Corp. 2001, 2016
   Author(s): Raimund Schroeder  (raimund.schroeder@de.ibm.com)
              Peter Tiedemann    (ptiedem@de.ibm.com)
              Ursula Braun       (ursula.braun@de.ibm.com)
   Published under the terms and conditions of the CPL (common public license)

   PLEASE NOTE:
   snIPL is provided under the terms of the enclosed common public license
   ("agreement"). Any use, reproduction or distribution of the program
   constitutes recipient's acceptance of this agreement.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <dlfcn.h>
#include <termios.h>
#include "snipl.h"

#define DONE -1;

static struct option long_options[] =
{
	{"stop",                   0, NULL, 'o'},
	{"load",                   0, NULL, 'l'},
	{"scsiload",               0, NULL, 's'},
	{"scsidump",               0, NULL, 'D'},
	{"activate",               0, NULL, 'a'},
	{"deactivate",             0, NULL, 'd'},
	{"reset",                  0, NULL, 'r'},
	{"dialog",                 0, NULL, 'i'},
	{"getstatus",              0, NULL, 'g'},
	{"listimages",             0, NULL, 'x'},
	{"vmserver",               1, NULL, 'V'},
	{"lparserver",             1, NULL, 'L'},
	{"userid",                 1, NULL, 'u'},
	{"password",               1, NULL, 'p'},
	{"promptpassword",         0, NULL, 'P'},
	{"timeout",                1, NULL, 't'},
	{"msgtimeout",             1, NULL, 'm'},
	{"configfilename",         1, NULL, 'f'},
	{"help",                   0, NULL, 'h'},
	{"version",                0, NULL, 'v'},
	{"force",                  0, NULL, 'F'},
	{"address_load",           1, NULL, 'A'},
	{"parameters_load",        1, NULL, 'R'},
	{"noclear",                0, NULL, 'C'},
	{"load_timeout",           1, NULL, 'T'},
	{"storestatus",            0, NULL, 'S'},
	{"wwpn_scsiload",          1, NULL, 'W'},
	{"lun_scsiload",           1, NULL, 'I'},
	{"bps_scsiload",           1, NULL, 'B'},
	{"ossparms_scsiload",      1, NULL, 'O'},
	{"bootrecord_scsiload",    1, NULL, 'E'},
	{"profilename",            1, NULL, 'N'},
	{"msgfilename",            1, NULL, 'M'},
	{"port",                   1, NULL, 'z'},
	{"shutdowntime",           1, NULL, 'X'},
	{"noencryption",	   0, NULL, 'e'},
	{NULL, 0, NULL, 0}
};


/*
 *	function: find_long_option_name
 *
 *	purpose: map val in option to its long name
 */
static char * find_long_option_name(const int val)
{
	int i;
	for (i=0; long_options[i].val; i++) {
		if (long_options[i].val == val)
			return (char *)long_options[i].name;
	}
	return NULL;
}


/*
 *	function: set_invalid_combinations
 *
 *	purpose: mark invalid combinations of options
 */
static const char *invalid_option_combinations[256] = {
	['o'] "lsDadrixg",
	['l'] "sDadrixg",
	['s'] "Dadrixg",
	['D'] "adrixg",
	['a'] "drixgX",
	['d'] "rixg",
	['r'] "ixgX",
	['i'] "xg",
	['x'] "gX",
	['g'] "X",
	['V'] "LolsDimNARCTSUWIBOE",
	['p'] "P",
	['L'] "zX",
	['F'] "X",
};

/*
 * check if parameter is a positiv hex number
 */
static int ishexnum(char *s)
{
	while(*s)
		if (!isxdigit(*s++)) return 0;
	return 1;
}

/*
 *	function: print_usage
 *
 *	purpose: prints usage information
 */
static void print_usage(const char *name)
{
	printf("Call simple network IPL (Linux Image Control)\n");
	printf("Usage: %s [options] [image_name ...]\n", name);
	printf(" image_name                      name of image to manage\n");
	printf(" -V --vmserver <ipaddr>          address of VMserver to connect to\n");
	printf(" -L --lparserver <ipaddr>        address of HMC/SE to connect to\n");
	printf(" -f --configfilename <filename>  name of configuration file\n");
	printf(" -z --port <portnumber>          SMAPI-server port\n");
	printf(" -u --userid <userid>            userid for VMserver login\n");
	printf(" -p --password <password>        non-default password/community for server login\n");
	printf(" -P --promptpassword             request password prompting\n");
	printf(" -e --noencryption               deactivates openssl / SNMPv3");
	printf("\n\n");
	printf(" -a --activate                   activate an image\n");
	printf(" -d --deactivate                 deactivate an image\n");
	printf(" -r --reset                      reset an image\n");
	printf(" -o --stop                       stop all cpus of an image\n");
	printf(" -l --load                       perform a load operation (LPAR only)\n");
	printf(" -s --scsiload                   perform a SCSI load operation (LPAR only)\n");
	printf(" -D --scsidump                   perform a SCSI dump operation (LPAR only)\n");
	printf(" -i --dialog                     start operating system messages dialog (LPAR)\n");
	printf(" -g --getstatus                  get status information\n");
	printf(" -x --listimages                 list all images of a given server\n");
	printf("\n");
	printf(" -F --force                      non-graceful execution\n");
	printf("    --timeout <timeout>          Timeout (in milliseconds) for "
	  "LPAR command\n                                 completion (default 60000ms)\n");
	printf(" -X --shutdowntime               delay for z/VM guest shutdown (default 300s)\n");
	printf("    --msgtimeout <interval>      Interval (in milliseconds) for "
	  "polling\n                                 LPAR operating system "
	  "messages (default 5000ms)\n");
	printf("                                 (for operating system messages dialog)\n");
	printf(" -M --msgfilename                file name for saving messages\n");
	printf("    --profilename <str>          profile name for LPAR activate\n");
	printf(" -A --address_load <hex_la>      hexadecimal address for load\n");
	printf("                                 (default: address of previous load)\n");
	printf("    --parameters_load <str>      parameter string for load\n");
	printf("                                 (default: string of previous load)\n");
	printf("    --noclear                    no clearing of memory before load\n");
	printf("    --load_timeout <timeout>     Timeout (in seconds) for load completion\n");
	printf("                                 (default: 60s)\n");
	printf("    --storestatus                store status before load\n");
	printf("    --wwpn_scsiload <hex_str>    world wide port name for scsiload / scsidump\n");
	printf("                                 (default: port name of previous scsiload)\n");
	printf("    --lun_scsiload <hex_str>     logical unit number for scsiload / scsidump\n");
	printf("                                 (default: lun of previous scsiload)\n");
	printf("    --bps_scsiload <int>         boot program selector for scsiload / scsidump\n");
	printf("                                 (default: selector of previous scsiload)\n");
	printf("    --ossparms_scsiload <str>    operating system specific scsiload / scsidump parameters\n");
	printf("                                 (default: parameters of previous scsiload)\n");
	printf("    --bootrecord_scsiload <str>  boot record logical block address for scsiload / scsidump\n");
	printf("                                 (default: boot record of previous scsiload)\n");
	printf("\n");
	printf(" -h --help                       print usage information\n");
	printf(" -v --version                    print version of %s\n", name);
	printf("\n");
	printf("Please report bugs to: linux390@de.ibm.com\n");
}


/*      function: replace_char
 *
 *      purpose: replace character "old" by character "new"
 */
static inline void
replace_char(char *buffer, unsigned char old, unsigned char new)
{
	unsigned char * character;

	if (buffer) {
		character = (unsigned char *)strchr(buffer,old);
		if (character)
			*character = new;
	}
}


/*
 *	function: list_images
 *
 *	purpose: list all images of a given server
 */
static void listimages(struct snipl_server *server)
{
	int j;
	struct snipl_image *image;

	fprintf(stdout, "\navailable images for server %s",
		server->address);
	if (server->user)
		fprintf(stdout, " and userid %s", server->user);
	fprintf(stdout, " :\n\n");

	j=0;
	snipl_for_each_image(server, image) {
		replace_char(image->name, 0x0a, '-');
		fprintf(stdout, "%16s ", image->name);
		if (j == 3) {
			j = 0;
			fprintf(stdout, "\n");
		} else
			j++;
		replace_char(image->name, '-', 0x0a);
	}
	fprintf(stdout, "\n");
}


/*
 *	function: print_server_message
 *
 *	purpose: print message
 */
/*****************************************************************/
static void print_server_message(struct snipl_server *server)
{
	if (server->problem) {
		if (server->problem_class == OK) {
			fprintf(stdout, "%s", server->problem);
		} else {
			fprintf(stderr, "%s", server->problem);
		}
		free(server->problem);
		server->problem = NULL;
	}
}


/*
 *	function: prompt_for_password
 *
 *	purpose: prompt for password on stdin
 */
/*****************************************************************/

static void prompt_for_password (char *password)
{
	struct termios initialrsettings, newrsettings;
	char *c;

	tcgetattr(fileno(stdin), &initialrsettings);
	newrsettings = initialrsettings;
	newrsettings.c_lflag &= ~ECHO;
	if (tcsetattr(fileno(stdin), TCSAFLUSH | TCSANOW, &newrsettings) != 0) {
		fprintf(stderr, "Could not set attributes\n");
	} else {
		printf("Enter password: ");
		c = fgets(password, 80, stdin);
		tcsetattr(fileno(stdin), TCSAFLUSH | TCSANOW,
				&initialrsettings);
		printf("\n");
		if (password[strlen(password)-1] == '\n') {
			password[strlen(password)-1] = '\0';
		}
		DEBUG_PRINT("\nPassword entered %s\n", password);
	}
	return;
}

/*
 *	function alias_handling
 *
 *	purpose: switch from alias to real image name
 */
/*****************************************************************/

static void
alias_handling(struct snipl_server *input_server,
	       struct snipl_server *config_server)
{
	struct snipl_image *input_image;
	struct snipl_image *config_image;

	snipl_for_each_image(input_server, input_image) {
		snipl_for_each_image(config_server, config_image) {
			if (!strcasecmp(input_image->name, config_image->alias))
				input_image->name = config_image->name;
		}
	}
}

/*
 *	function: find_server_in_conf_with_image
 *
 *	purpose: find a server in the configuration file
 *               for a given image
 */
/*****************************************************************/

static struct snipl_server *
find_server_in_conf_with_image(struct snipl_configuration *conf,
			       struct snipl_server *server)
{
	struct snipl_server *serv;
	struct snipl_server *serv2;

	serv = find_next_server(conf, server->_images->name, server->user,
				NULL);
	if (serv) {
		serv2 = find_next_server(conf, server->_images->name,
					 server->user, serv);
		if (serv2) {
			/* another server found ==>       */
			/*    config file info not usable */
			fprintf(stderr, "more than one ");
			if (strcasecmp(serv->address, serv2->address)) {
				/* warn but continue (preserving serv) */
				/* to allow diff. servers for an image */
				fprintf(stderr, "server found in config file "
					"in %s for image %s\n",
					conf->filename, server->_images->name);
			} else {
				/* don't allow diff. users */
				/* with same server and image */
				fprintf(stderr, "user found in config file "
					"%s for server %s and image %s\n",
					conf->filename, serv->address,
					server->_images->name);
				serv = NULL;
			}
		}
	} else {
		fprintf(stderr, "image %s not found in config file %s",
			server->_images->name, conf->filename);
		if (server->user)
			fprintf(stderr, " for userid %s", server->user);
		fprintf(stderr, "\n");
	}
	if (serv) { /* server determined from config file */
		if (strcasecmp(serv->type, "VM") &&
		    strcasecmp(serv->type, "LPAR")) {
			fprintf(stderr, "type of server %s in config file %s "
				"is neither VM nor LPAR: %s\n",
				server->address, conf->filename, serv->type);
			serv = NULL;
		} else {
			server->address = serv->address;
			server->type = serv->type;
			fprintf(stdout, "Server %s ", server->address);
			if (!strcasecmp(serv->type, "VM"))
				fprintf(stdout, "with userid %s ", serv->user);
			if (serv->port != UNDEFINED)
				fprintf(stdout, "with port %i ", serv->port);
			fprintf(stdout, "from config file %s is used\n",
				conf->filename);
		}
	}
	return serv;
}

/*
 *	function: find_server_in_conf_with_address
 *
 *	purpose: find a server in the configuration file
 *               for a given address
 */
/*****************************************************************/

static struct snipl_server *
find_server_in_conf_with_address(struct snipl_configuration *conf,
				 struct snipl_server *server)
{
	struct snipl_server *serv;
	struct snipl_server *serv2;

	serv = NULL;
	snipl_for_each_server(conf, serv2) {
		if (!strcasecmp(serv2->address,
				server->address)) {
			if (!strcasecmp(serv2->type, "LPAR")) {
				serv = serv2;
				break;
			}
			if ((server->user && serv2->user) &&
			    !strcasecmp(server->user, serv2->user)) {
				serv = serv2;
				break;
			}
			if (serv && !server->user) {
				fprintf(stderr, "more than one user found in "
					"config file %s for server %s\n",
					conf->filename,
					server->address);
				serv2 = NULL;
				break;
			}
		if (!server->user)
			serv = serv2;
		}
	}
	return serv;
}

/*
 *	function: configfile_handling
 *
 *	purpose: read and check configuration file
 *               determine server, password, userid if missing
 */
/*****************************************************************/

static struct snipl_configuration*
configfile_handling (char *cfgname, struct snipl_server *server)
{
	struct snipl_configuration* conf = NULL;
	struct snipl_server *serv  = NULL;
	struct snipl_server *serv2 = NULL;
	struct snipl_image  *image = NULL;
	struct snipl_image  *imag  = NULL;
	char *used_cfgname = NULL;
	_Bool found;

	used_cfgname = get_config_file_name(cfgname);
	if (!used_cfgname) {
		if (cfgname)
			fprintf(stderr, "Warning : No default configuration "
				"file and no %s could be found/opened.\n",
				cfgname);
		else
			fprintf(stderr, "Warning : No default configuration "
				"file could be found/opened.\n");
		return NULL;
	}

	/* config file found */
	conf = snipl_configuration_from_file(used_cfgname);
	if (!conf) {
		fprintf(stderr, "Error while reading configuration file %s\n",
			used_cfgname);
		goto exit;
	}
	if (conf->problem_class != OK) {
		fprintf(stderr, "Error while reading configuration file %s :\n",
			used_cfgname);
		fprintf(stderr, "%s\n", conf->problem);
		/* snipl_configuration_free(conf); */
		/* done by caller after processing of problem message */
		goto exit;
	}
	if (server == NULL) {
		/* here we have no preconstructed server to check with */
		/* so we just loaded and are ready */
		goto exit;
	}
	if (server->_images) { /* images given, use first one */
		/* look for an occurrence of image and determine its server */
		/* mult. occurrences of one image in config file not handled! */
		if (server->address) {
			serv = find_server_with_address(conf, server, NULL);
			if (serv) {
/*	alternativ:
			// more than one definition only for VM possible
			if (serv && !strcasecmp(server->type, "VM")) {
*/
				serv2 = find_server_with_address(conf, server,
								 serv);
				if (serv2) {
					/* another definition found */
					fprintf(stderr, "more than one user "
						"found in config file %s for "
						"server %s and image %s\n",
						conf->filename, server->address,
						server->_images->name);
					serv = NULL;
				} else
					fprintf(stdout, "Server %s ",
						server->address);
				if (serv->user)
					fprintf(stdout, "with userid %s",
						serv->user);
				fprintf(stdout, " from config file "
					"%s is used\n", conf->filename);
			} else {
				fprintf(stderr, "image %s not found in config "
					"file %s for server %s",
					server->_images->name, conf->filename,
					server->address);
				if (server->user)
					fprintf(stderr, " and userid %s",
						server->user);
				fprintf(stderr, "\n");
			}
		} else
			/* server unknown, try to determine from config file */
			serv = find_server_in_conf_with_image(conf, server);
	} else { /* no images given */
		if (server->address) /* server known */
			/* look for an occurr. of server with given server addr.
			 * multiple occurrences of servers with same server
			 * address and userid in config file are forbidden
			 * (check in config.c)
			 */
			serv = find_server_in_conf_with_address(conf, server);
	}
	if (serv && strcasecmp(server->type, serv->type)) {
		fprintf(stderr,
			"Server %s has different type %s in config file %s\n",
			server->address,
			serv->type,
			conf->filename);
		serv = NULL;
	}
	if (serv) { /* server found in conf */
		alias_handling(server, serv);
		if (!server->password)
			/* password not specified, use pw from config file */
			server->password = serv->password;
		if (server->port == UNDEFINED)
			/* port not specified, use port from config file */
			server->port = serv->port;
		if (!server->sslfingerprint)
			/* it is not possible to define the sslfingerprint */
			/* via commandline, thus use from config file */
			server->sslfingerprint = serv->sslfingerprint;
		if (!server->user)
			/* user id not specified, use userid from config file */
			server->user = serv->user;
		if (server->enc == UNDEFINED)
			server->enc = serv->enc;
		if (!strcasecmp(server->type, "VM")) { /* VM-type server */
			if (server->parms.image_op == LIST) {
				/* use images from config file */
				imag = NULL;
				snipl_for_each_image(server, image) {
					free(imag);
					imag = image;
				}
				server->_images = serv->_images;
				serv->_images = NULL;
			} else {
			/* check if given image-chain belongs to serv */
				snipl_for_each_image(server, image) {
					found = 0;
					snipl_for_each_image(serv, imag) {
						if (!strcasecmp(image->name,
								imag->name)) {
							found=1;
							break;
						}
					}
					if (!found) {
						fprintf(stderr, "Image %s not "
							"contained in image list"
							" of selected server %s "
							"within config file %s\n",
							image->name,
							serv->address,
							conf->filename);
					}
				}
			}
		}
	} else {   /* no server found in conf */
		if (!serv && server->type && !strcasecmp(server->type, "VM") &&
		    server->parms.image_op == LIST) {
			fprintf(stderr, "server %s ",
					server->address);
			if (server->user)
				fprintf(stderr, "with userid %s ", server->user);
			fprintf(stderr, "not found in config file %s\n",
				conf->filename);
			imag = NULL;
			snipl_for_each_image(server, image) {
				free(imag);
				imag = image;
			}
			server->_images = NULL;
		}
	}

exit:
	free(used_cfgname);
	return conf;
}


/*
 *	function: parse_command_input
 *
 *	purpose: parameter parsing and checking
 */

/*****************************************************************/
static int parse_command_input(int argc, char **argv,
			struct snipl_server *server,
			char **cfgname)
{
	int ret;
	int temp_ret, temp_len;
	int i;
	size_t j;
	_Bool option_specified[256] = {0};
	int   option;
	int   option_index;
	char  next_char;
	const char * fill_string = "0000000000000000";
	struct snipl_image *new_image;

	/* parse command line arguments */
	ret = 0;
	temp_ret = 0;
	option_index = 0;
	server->enc = UNDEFINED;
	server->port = UNDEFINED;
	while (1) {
		option = getopt_long(argc, argv,
				"ehvaolsDdrigxV:Z:L:u:p:Pf:FiX:A:M:z:",
				long_options, &option_index);
		if (option == EOF)
			break;
		if (option_specified[option]) {
			fprintf(stderr,
				"option %s specified more than once\n",
				find_long_option_name(option));
			ret = DUPLICATE_OPTION;
		}
		option_specified[option] = 1;
		switch (option) {
		case 'u':
			server->user = optarg;
			DEBUG_PRINT("userid is %s...\n", server->user);
			break;
		case 'p':
			server->password = optarg;
			DEBUG_PRINT("password/community is %s...\n",
					server->password);
			break;
		case 'P':
			server->password_prompt = 1;
			DEBUG_PRINT("password prompting requested\n");
			break;
		case 'V':
			server->address = optarg;
			server->type = "VM";
			break;
		case 'L':
			server->address = optarg;
			server->type = "LPAR";
			break;
		case 'e':
			server->enc = 0;
			DEBUG_PRINT("encryption is off\n");
			break;
		case 'h':
			print_usage(argv[0]);
			return DONE;
		case 'a':
			server->parms.image_op = ACTIVATE;
			break;
		case 'o':
			server->parms.image_op = STOP;
			break;
		case 'l':
			server->parms.image_op = LOAD;
			break;
		case 'g':
			server->parms.image_op = GETSTATUS;
			break;
		case 's':
			server->parms.image_op = SCSILOAD;
			break;
		case 'D':
			server->parms.image_op = SCSIDUMP;
			break;
		case 'd':
			server->parms.image_op = DEACTIVATE;
			break;
		case 'r':
			server->parms.image_op = RESET;
			break;
		case 'i':
			server->parms.image_op = DIALOG;
			break;
		case 'x':
			server->parms.image_op = LIST;
			break;
		case 'f':
			*cfgname = optarg;
			DEBUG_PRINT("cfgname is %s...\n", *cfgname);
			break;
		case 'F':
			server->parms.force = 1;
			break;
		case 'X':
			temp_ret = sscanf(optarg, "%i%1c",
					&server->parms.shutdown_time, &next_char);
			if ((!isscanf_ok(temp_ret, next_char, optarg)) ||
			    (server->parms.shutdown_time < 1) ||
			    (server->parms.shutdown_time > 32767)) {
				fprintf(stderr,
					"invalid shutdowntime: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				DEBUG_PRINT("shutdowntime is %i\n",
					    server->parms.shutdown_time);
			}
			break;
		case 't':
			temp_ret = sscanf(optarg, "%i%1c",
					&server->timeout, &next_char);
			if ((!isscanf_ok(temp_ret, next_char, optarg)) ||
			    (server->timeout < 1)) {
				fprintf(stderr,
					"invalid timeout: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				DEBUG_PRINT("generic timeout is %i\n",
					    server->timeout);
			}
			break;
		case 'm':
			temp_ret = sscanf(optarg, "%i%1c",
					&server->parms.msg_timeout,
					&next_char);
			if (!isscanf_ok(temp_ret, next_char, optarg)) {
				fprintf(stderr,
					"invalid msgtimeout: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				DEBUG_PRINT("polling interval is %i\n",
					    server->parms.msg_timeout);
			}
			break;
		case 'M':
			server->parms.msgfilename = optarg;
			DEBUG_PRINT("msgfile is %s...\n",
				    server->parms.msgfilename);
			break;
		case 'A':
			server->parms.load_address = optarg;
			temp_ret = sscanf(server->parms.load_address,
					"%*x%1c", &next_char);
			if ((temp_ret == 1) || !ishexnum(optarg)) {
				fprintf(stderr,
					"invalid load_address: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else if (strlen(server->parms.load_address) > 5) {
				fprintf(stderr, "load_address %s too long - "
					"maximum size is 5 characters\n",
					server->parms.load_address);
				ret = INVALID_PARAMETER_VALUE;
			} else if (strlen(server->parms.load_address) == 5 &&
				   server->parms.load_address[0] > '3') {
				fprintf(stderr, "Invalid subchannel-set for ");
				fprintf(stderr, "load_address %s. ",
					server->parms.load_address);
				fprintf(stderr, "Valid subchannel-sets are ");
				fprintf(stderr, "0, 1, 2, 3\n");
				ret = INVALID_PARAMETER_VALUE;
			} else {
				DEBUG_PRINT("load address is %s\n",
					    server->parms.load_address);
			}
			break;
		case 'R':
			server->parms.load_parms = optarg;
			DEBUG_PRINT("load parameter is %s\n",
				    server->parms.load_parms);
			break;
		case 'C':
			server->parms.clear = 0;
			break;
		case 'T':
			temp_ret = sscanf(optarg, "%hi%1c",
					&server->parms.load_timeout,
					&next_char);
			if (!isscanf_ok(temp_ret, next_char, optarg)) {
				fprintf(stderr,
					"invalid load_timeout: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				DEBUG_PRINT("timeout for load is %i\n",
					    server->parms.load_timeout);
			}
			break;
		case 'S':
			server->parms.store_stat = 1;
			break;
		case 'W':
			strncpy(server->parms.scsiload_wwpn, optarg, 16);
			temp_ret = sscanf(server->parms.scsiload_wwpn,
					"%*x%1c", &next_char);
			temp_len = strlen(server->parms.scsiload_wwpn);
			if ((temp_ret == 1) || !ishexnum(optarg)) {
				fprintf(stderr,
					"invalid wwpn_scsiload: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else if (strlen(optarg) > 16) {
				fprintf(stderr, "wwpn_scsiload %s too long - "
					"maximum size is 16 characters\n",
					server->parms.scsiload_wwpn);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				strncat(server->parms.scsiload_wwpn,
					fill_string, 16-temp_len);
				DEBUG_PRINT("wwpn_scsiload is %s\n",
					    server->parms.scsiload_wwpn);
			}
			break;
		case 'I':
			strncpy(server->parms.scsiload_lun, optarg, 16);
			temp_ret = sscanf(server->parms.scsiload_lun,
					"%*x%1c", &next_char);
			temp_len = strlen(server->parms.scsiload_lun);
			if ((temp_ret == 1) || !ishexnum(optarg)) {
				fprintf(stderr,
					"invalid lun_scsiload: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else if (strlen(optarg) > 16) {
				fprintf(stderr, "lun_scsiload %s too long - "
					"maximum size is 16 characters\n",
					server->parms.scsiload_lun);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				strncat(server->parms.scsiload_lun,
					fill_string, 16-temp_len);
				DEBUG_PRINT("lun_scsiload is %s\n",
					    server->parms.scsiload_lun);
			}
			break;
		case 'B':
			temp_ret = sscanf(optarg, "%hi%1c",
					&server->parms.scsiload_bps,
					&next_char);
			if (!isscanf_ok(temp_ret, next_char, optarg)) {
				fprintf(stderr,
					"invalid boot prog. sel.: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else if ((strlen(optarg) > 2) ||
				   (server->parms.scsiload_bps > 30)) {
					fprintf(stderr, "bps_scsiload %s too "
					"large - maximum is 30\n",
					server->parms.scsiload_lun);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				DEBUG_PRINT("boot prog. sel. is %i\n",
					    server->parms.scsiload_bps);
			}
			break;
		case 'O':
			server->parms.scsiload_ossparms = optarg;
			DEBUG_PRINT("OS specific parms are %s\n",
				    server->parms.scsiload_ossparms);
			break;
		case 'E':
			strncpy(server->parms.scsiload_bootrec, optarg, 16);
			temp_ret = sscanf(server->parms.scsiload_bootrec,
					"%*x%1c", &next_char);
			temp_len = strlen(server->parms.scsiload_bootrec);
			if ((temp_ret == 1) || !ishexnum(optarg)) {
				fprintf(stderr,
					"invalid bootrecord_scsiload: %s\n",
					optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else if (strlen(optarg) > 16) {
				fprintf(stderr, "bootrecord_scsiload %s too "
					"long - maximum size is 16 "
					"characters\n",
					server->parms.scsiload_bootrec);
				ret = INVALID_PARAMETER_VALUE;
			} else {
				strncat(server->parms.scsiload_bootrec,
					fill_string, 16-temp_len);
				DEBUG_PRINT("bootrecord_scsiload is %s\n",
					    server->parms.scsiload_bootrec);
			}
			break;
		case 'N':
			server->parms.profile = optarg;
			DEBUG_PRINT("profile name is %s\n",
				    server->parms.profile);
			break;
		case 'z':
			temp_ret = sscanf(optarg, "%5i%1c",
					&server->port,
					&next_char);
			if (!isscanf_ok(temp_ret, next_char, optarg)) {
				fprintf(stderr,
					"invalid port: %s\n", optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else if (server->port > 65535) {
				fprintf(stderr,
					"invalid port: %s\n", optarg);
				ret = INVALID_PARAMETER_VALUE;
			} else
				DEBUG_PRINT("SMAPI port: %i\n",
					server->port);
			break;
		case 'v':
			fprintf(stdout, "%s\n", SNIPL_VERSION);
			fprintf(stdout, "%s\n", SNIPL_COPYRIGHT);
			return DONE;
		default:
			print_usage(argv[0]);
			return UNKNOWN_PARAMETER;
		}
	}

	/* parameter checking */
	for (i=0; i<256; i++) {
		if (option_specified[i] && invalid_option_combinations[i]) {
			for (j = 0; j < strlen(invalid_option_combinations[i]);
			     j++) {
				if (option_specified[(int)invalid_option_combinations[i][j]]) {
					fprintf(stderr, "option --%s must not "
						"be specified together with "
						"option --%s\n",
						find_long_option_name(i),
						find_long_option_name(invalid_option_combinations[i][j]));
					ret = CONFLICTING_OPTIONS;
				}
			}
		}
	}

	/* read image names (non-optional params) and allocate image storage */
	while (optind < argc) {
		if (!(new_image = calloc(1, sizeof (*new_image)))) {
			fprintf(stderr,  "cannot allocate image buffer\n");
			return STORAGE_PROBLEM;
		}
		new_image->name = argv[optind++];
		new_image->alias = new_image->name;
		replace_char(new_image->name, '-', 0x0a);
		new_image->_next = server->_images;
		new_image->server = server;
		server->_images = new_image;
	}
	return ret;
}


/*
 *	function: command_processing
 *
 *	purpose: point of control
 */

/*****************************************************************/
static int command_processing(struct snipl_server *server)
{
	int ret;
	struct snipl_image *image;
	struct snipl_parms image_parms;
	int temp_ret;

	ret = 0;

	if (server->parms.image_op == LIST && !strcasecmp(server->type, "VM")) {
		listimages(server);
		return ret;
	}

	/* now we work on our own image list */
	image_parms = server->parms;   /* save info from parameter parsing*/
	ret = snipl_prepare(server);
	if (ret)
		goto out;
	server->parms = image_parms;   /* restore info from parameter parsing*/
	ret = snipl_prepare_check(server);
	if (ret)
		goto out;

	ret = snipl_login(server);
	if (ret && server->problem_class == CERTIFICATE_ERROR) {
		print_server_message(server);
		if (snipl_confirm(server) <= 0)
			goto logout;
	} else if (ret)  /* communication error (timeout) */ {
		if (!strcasecmp(server->type, "VM")) {
			snipl_logout(server);
			server->type = "VM5";
			/* try RPC interface */
			ret = snipl_prepare(server);
			if (ret) {
				server->type = "VM";
				create_msg(server, "Error: login fails for "
					   "VM server %s\n", server->address);
				goto out;
			}
			server->parms = image_parms;
			ret = snipl_prepare_check(server);
			if (ret)
				goto out;
			ret = snipl_login(server);
			if (!ret)
				DEBUG_PRINT("%s is a VSMSERVE server\n",
					server->address);
			else if (server->port == UNDEFINED)
				create_msg(server, "Error: missing Port for "
					   "VM server %s\n", server->address);
		}
		if (ret)
			goto out;
	} else
		DEBUG_PRINT("%s is a socket-based server\n", server->address);
	DEBUG_PRINT("Login to server %s successful\n", server->address);

	/*
	 * Don't check the server type because the LIST operation for
	 * VM type servers is handled and returned earlier.
	 */
	if (server->parms.image_op == LIST) {
		listimages(server);
		goto logout;
	}

	/* Remove newline from image name in case of VM */
	if (!strcasecmp(server->type, "VM")) {
		snipl_for_each_image(server, image) {
			replace_char(image->name, 0x0a, '-');
		}
	}

	snipl_for_each_image(server, image) {
		switch (server->parms.image_op) {
			/* same operation on every image */
		case ACTIVATE:
			ret = snipl_activate(image);
			break;
		case STOP:
			ret = snipl_stop(image);
			break;
		case LOAD:
			ret = snipl_load(image);
			break;
		case SCSILOAD:
			ret = snipl_scsiload(image);
			break;
		case SCSIDUMP:
			ret = snipl_scsidump(image);
			break;
		case DEACTIVATE:
			ret = snipl_deactivate(image);
			break;
		case RESET:
			ret = snipl_reset(image);
			break;
		case DIALOG:
			ret = snipl_dialog(image);
			break;
		case GETSTATUS:
			ret = snipl_getstatus(image);
			break;
		default:
			create_msg(server, "internal error: unknown op\n");
			ret = INTERNAL_ERROR;
		}
		print_server_message(server);
		if (!strcasecmp(server->type, "VM") && image->_next)
			ret = snipl_login(server);
	}

logout:
	print_server_message(server);
	temp_ret = snipl_logout(server);
	if (!ret)
		ret = temp_ret;
out:
	print_server_message(server);
	return ret;
}


/*
 *	function: main
 *
 *	purpose: point of control
 */

/*****************************************************************/
int main(int argc, char **argv)
{
	int ret, temp_ret;
	char* cfgname;
	char password[80];
	struct snipl_server *server;
	struct snipl_configuration *conf = NULL;
	struct snipl_image *imag = NULL;
	struct snipl_image *image = NULL;

	if (!(server = calloc(1, sizeof (*server)))) {
		fprintf(stderr, "cannot allocate buffer for server\n");
		return STORAGE_PROBLEM;
	}

	server->parms = (struct snipl_parms) {
		.force = UNDEFINED,
		.clear = UNDEFINED,
		.store_stat = UNDEFINED,
		.load_timeout = UNDEFINED,
		.scsiload_bps = UNDEFINED,
		.msg_timeout = UNDEFINED,
		.image_op = OPUNKNOWN,
	};

	ret = 0;
	temp_ret = 0;
	cfgname = NULL;
	ret = parse_command_input(argc, argv, server, &cfgname);
	if (ret < 0) {
		ret = 0;
		goto free_all;
	}

	if (!server->_images && server->parms.image_op != LIST &&
		ret != UNKNOWN_PARAMETER) {
		fprintf(stderr, "Missing image name(s)\n");
		ret = MISSING_IMAGENAME;
		goto free_all;
	}
	if (server->parms.image_op == DIALOG &&
		server->_images && server->_images->_next) {
		fprintf(stderr, "More than one image name specified for DIALOG "
			"operation\n");
		ret = MORE_THAN_ONE_IMAGE;
		goto free_all;
	}
	if (ret)
		goto free_all;

	if (server->password_prompt) {
		server->password = password;
		prompt_for_password(server->password);
	}

	conf = configfile_handling(cfgname, server);
        /* continue, even if CONFIG_FILE_ERROR */

	if (!server->address) {
		fprintf(stderr, "%s: missing server ipaddr\n\n", argv[0]);
		ret = MISSING_SERVER;
		goto free_all;
	}
	server->_next = NULL;

	if (server->parms.image_op == OPUNKNOWN) {
		fprintf(stderr, "Please specify one of the parameters:\n");
		fprintf(stderr, "   -a --activate\n   -r --reset\n");
		fprintf(stderr, "   -d --deactivate\n   -x --listimages\n");
		fprintf(stderr, "   -g --getstatus\n");
		if (!strcasecmp(server->type, "LPAR")) {
			fprintf(stderr,	"   -o --stop\n   -l --load\n");
			fprintf(stderr, "   -s --scsiload\n   -D --scsidump\n");
			fprintf(stderr,	"   -i --dialog\n");
		}
		ret = NO_COMMAND;
		goto free_all;
	}

	if (server->parms.image_op == LIST && !conf &&
		(!strcasecmp(server->type, "VM"))) {
		fprintf(stderr, "--listimages for VM server must not be "
			"specified without config file\n");
		ret = CONFLICTING_OPTIONS;
		goto free_all;
	}
	if (server->parms.image_op == LIST && !conf &&
		server->_images) {
		fprintf(stderr, "--listimages must not be specified "
			"together with an image name\n");
		ret = CONFLICTING_OPTIONS;
		goto free_all;
	}
	if (server->enc == UNDEFINED) {
		server->enc = 1;
		DEBUG_PRINT("set encryption on (default)\n");
	}

	ret = command_processing(server);
	if (server->module_handle) {
		temp_ret = dlclose(server->module_handle);
		if (temp_ret) {
			server->problem = dlerror();
			server->problem_class = FATAL;
			print_server_message(server);
		}
	}

free_all:
	/* free images */
	imag = NULL;
	snipl_for_each_image(server, image) {
		free(imag);
		imag = image;
	}
	free(server);
	snipl_configuration_free(conf);
	return ret;
}
