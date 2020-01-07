/*
 * sncapjob.c : API for snCAP Job object
 *
 * Copyright IBM Corp. 2012, 2016
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */
#include "sncapjob.h"
static const char *cpu_type[5] = {
	[CPU_TYPE_ICF] "ICF",
	[CPU_TYPE_IFL] "IFL",
	[CPU_TYPE_SAP] "SAP",
	[CPU_TYPE_ZAAP] "AAP",
	[CPU_TYPE_ZIIP] "IIP"
};

/*
 * Command line option definitions for the getopt_long function
 */
static struct option long_options[] = {
	{"activate",		1, NULL, 'a'},
	{"configfilename",	1, NULL, 'f'},
	{"pu_configuration",	0, NULL, 'c'},
	{"deactivate",		1, NULL, 'd'},
	{"no_record_changes",	0, NULL, 'n'},
	{"help",		0, NULL, 'h'},
	{"icf",			1, NULL, 'C'},
	{"ifl",			1, NULL, 'F'},
	{"list_cpcs",		0, NULL, 'x'},
	{"list_records",	0, NULL, 'l'},
	{"password",		1, NULL, 'p'},
	{"promptpassword",	0, NULL, 'P'},
	{"query",		1, NULL, 'q'},
	{"sap",			1, NULL, 'A'},
	{"se",			1, NULL, 'S'},
	{"model_capacity",	1, NULL, 'm'},
	{"test",		0, NULL, 't'},
	{"timeout",		1, NULL, 'T'},
	{"verbose",		0, NULL, 'V'},
	{"version",		0, NULL, 'v'},
	{"zaap",		1, NULL, 'z'},
	{"ziip",		1, NULL, 'I'},
	{"userid",		1, NULL, 'u'},
	{"noencryption",	0, NULL, 'e'},
	{NULL,			0, NULL, 0}
};

/*
 *	function: handle_CPC
 *
 *	purpose: CPC_id command line option handler.
 */
static int handle_CPC(const char *argument, struct sncap_job *job)
{
	job->CPC_id = strdup(argument);
	if (job->CPC_id == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	return SNCAP_OK;
}

/*
 *	function: handle_activate
 *
 *	purpose: -a, --activate command line option handler.
 */
static int handle_activate(const char *argument, struct sncap_job *job)
{
	if (argument[0] == '-')
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;

	job->TCR_id = strdup(argument);
	if (job->TCR_id == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	job->operation = ACTIVATE;

	return SNCAP_OK;
}

/*
 *	function: handle_configfilename
 *
 *	purpose: -f, --configfilename command line option handler.
 */
static int handle_configfilename(const char *argument,
				struct sncap_job *job)
{
	job->config_file = strdup(argument);
	if (job->config_file == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	return SNCAP_OK;
}

/*
 *	function: handle_cpu_configuration
 *
 *	purpose: -c, --cpu_configuration command line option handler.
 */
static int handle_cpu_configuration(const char *argument,
				   struct sncap_job *job)
{
	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;

	job->operation = QUERY_CPUS;

	return SNCAP_OK;
}

/*
 *	function: handle_deactivate
 *
 *	purpose: -d, --deactivate command line option handler.
 */
static int handle_deactivate(const char *argument, struct sncap_job *job)
{
	if (argument[0] == '-')
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;

	job->TCR_id = strdup(argument);
	if (job->TCR_id == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	job->operation = DEACTIVATE;

	return SNCAP_OK;
}

/*
 *	function: handle_dryrun
 *
 *	purpose: -n, --dry-run command line option handler.
 */
static int handle_dryrun(const char *argument, struct sncap_job *job)
{
	job->operation_mode |= SNCAP_OP_MODE_DRYRUN;
	return SNCAP_OK;
}

/*
 *	function: handle_help
 *
 *	purpose: -h, --help command line option handler.
 */
static int handle_help(const char *argument, struct sncap_job *job)
{
	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;
	job->operation = HELP;

	return SNCAP_OK;
}

/*
 *	function: handle_icf
 *
 *	purpose: --icf command line option handler.
 */
static int handle_icf(const char *argument, struct sncap_job *job)
{
	if (sscanf(argument,
			"%d",
			&job->cpu_quantity[CPU_TYPE_ICF]) != 1)
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->cpu_quantity[CPU_TYPE_ICF] <= 0)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 *	function: handle_ifl
 *
 *	purpose: --ifl command line option handler.
 */
static int handle_ifl(const char *argument, struct sncap_job *job)
{
	if (sscanf(argument,
			"%d",
			&job->cpu_quantity[CPU_TYPE_IFL]) != 1)
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->cpu_quantity[CPU_TYPE_IFL] <= 0)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 *	function: handle_list_cpcs
 *
 *	purpose: -x, --list_cpcs command line option handler.
 */
static int handle_list_cpcs(const char *argument, struct sncap_job *job)
{
	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;

	job->operation = LIST_CPCS;

	return SNCAP_OK;
}

/*
 *	function: handle_list_records
 *
 *	purpose: -l, --list_records command line option handler.
 */
static int handle_list_records(const char *argument, struct sncap_job *job)
{
	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;

	job->operation = QUERY_LIST;

	return SNCAP_OK;
}

/*
 *	function: handle_password
 *
 *	purpose: -p, --password command line option handler.
 */
static int handle_password(const char *argument, struct sncap_job *job)
{
	job->connection.password = strdup(argument);
	if (job->connection.password == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	return SNCAP_OK;
}

/*
 *	function: handle_username
 *
 *	purpose: -u, --username command line option handler.
 */
static int handle_username(const char *argument, struct sncap_job *job)
{
	job->connection.username = strdup(argument);
	if (!job->connection.username)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	return SNCAP_OK;
}

/*
 *	function: handle_no_encryption
 *
 *	purpose: -e, --no_encryption command line option handler.
 */
static int handle_no_encryption(const char *argument, struct sncap_job *job)
{
	job->connection.encryption = 0;
	return SNCAP_OK;
}

/*
 *	function: handle_propmtpassword
 *
 *	purpose: -P, --promptpassword command line option handler.
 */
static int handle_promptpassword(const char *argument, struct sncap_job *job)
{
	job->operation_mode |= SNCAP_OP_MODE_PROMPTPW;
	return SNCAP_OK;
}

/*
 *	function: handle_query
 *
 *	purpose: -q, --query command line option handler.
 */
static int handle_query(const char *argument, struct sncap_job *job)
{
	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;

	if (!argument)
		return SNCAP_INVALID_OPTION_VALUE;

	job->TCR_id = strdup(argument);
	if (job->TCR_id == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	job->operation = QUERY_RECORD;

	return SNCAP_OK;
}

/*
 *	function: handle_sap
 *
 *	purpose: --sap command line option handler.
 */
static int handle_sap(const char *argument, struct sncap_job *job)
{
	if (sscanf(argument,
			"%d",
			&job->cpu_quantity[CPU_TYPE_SAP]) != 1)
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->cpu_quantity[CPU_TYPE_SAP] <= 0)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 *	function: handle_se
 *
 *	purpose: -S, --se command line option handler.
 */
static int handle_se(const char *argument, struct sncap_job *job)
{
	job->connection.address = strdup(argument);
	if (job->connection.address == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;
	return SNCAP_OK;
}

/*
 *	function: handle_softwaremodel
 *
 *	purpose: -s, --softwaremodel command line option handler.
 */
static int handle_modelcapacity(const char *argument,
				struct sncap_job *job)
{
	if (argument[0] == '-')
		return SNCAP_INVALID_OPTION_VALUE;

	job->software_model = strdup(argument);
	if (job->software_model == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;
	return SNCAP_OK;
}

/*
 *	function: handle_test
 *
 *	purpose: -t, --test command line option handler.
 */
static int handle_test(const char *argument, struct sncap_job *job)
{
	job->operation_mode |= SNCAP_OP_MODE_TEST;
	return SNCAP_OK;
}

/*
 *	function: handle_timeout
 *
 *	purpose: --timeout command line option handler.
 */
static int handle_timeout(const char *argument, struct sncap_job *job)
{
	if (sscanf(argument,
			"%d",
			&job->connection.timeout) != 1)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 *	function: handle_verbose
 *
 *	purpose: -V, --verbose command line option handler.
 */
static int handle_verbose(const char *argument, struct sncap_job *job)
{
	job->operation_mode |= SNCAP_OP_MODE_VERBOSE;
	return SNCAP_OK;
}

/*
 *	function: handle_version
 *
 *	purpose: -v, --version command line option handler.
 */
static int handle_version(const char *argument, struct sncap_job *job)
{
	if (job->operation != UNDEFINED)
		return SNCAP_CONFLICTING_OPTIONS;
	job->operation = VERSION;

	return SNCAP_OK;
}

/*
 *	function: handle_zaap
 *
 *	purpose:  --zaap command line option handler.
 */
static int handle_zaap(const char *argument, struct sncap_job *job)
{
	if (sscanf(argument,
			"%d",
			&job->cpu_quantity[CPU_TYPE_ZAAP]) != 1)
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->cpu_quantity[CPU_TYPE_ZAAP] <= 0)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 *	function: handle_ziip
 *
 *	purpose: --ziip command line option handler.
 */
static int handle_ziip(const char *argument, struct sncap_job *job)
{
	if (sscanf(argument,
			"%d",
			&job->cpu_quantity[CPU_TYPE_ZIIP]) != 1)
		return SNCAP_INVALID_OPTION_VALUE;

	if (job->cpu_quantity[CPU_TYPE_ZIIP] <= 0)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 * Option handler table entry definition
 */
struct option_handler {
	char option;
	int (*function)(const char *argument, struct sncap_job *job);
};

/*
 * Option handler table
 */
struct option_handler handler[] = {
	{1, handle_CPC},
	{'a', handle_activate},
	{'f', handle_configfilename},
	{'c', handle_cpu_configuration},
	{'d', handle_deactivate},
	{'h', handle_help},
	{'C', handle_icf},
	{'F', handle_ifl},
	{'x', handle_list_cpcs},
	{'l', handle_list_records},
	{'n', handle_dryrun},
	{'p', handle_password},
	{'P', handle_promptpassword},
	{'q', handle_query},
	{'A', handle_sap},
	{'S', handle_se},
	{'m', handle_modelcapacity},
	{'t', handle_test},
	{'T', handle_timeout},
	{'V', handle_verbose},
	{'v', handle_version},
	{'z', handle_zaap},
	{'I', handle_ziip},
	{'u', handle_username},
	{'e', handle_no_encryption},
	{'\0', NULL}
};

enum job_config_keyword_class {
	UNKNOWN,
	PASSWORD,
	TYPE,
	USER,
	PORT,
	IMAGE,
	CPCID,
	ENC,
	SSLFINGERPRINT
};

/*
 *	function: is_id
 *
 *	purpose: checks property value syntax.
 */
static _Bool is_id(const char *str)
{
	char c = '\0';

	if (str == NULL)
		return 0;
	if (strlen(str) == 0)
		return 0;

	while ((c = *str++))
		if (!isalnum(c))
			return 0;
	return 1;
}

/*
 *	function: get_CPC_id_alias
 *
 *	purpose: returns CPC identifier alias value from the CPC identifier
 *		 string.
 */
static char *get_CPC_id_alias(const char *id)
{
	char *s = NULL;
	char *slash = NULL;
	char *alias = NULL;

	s = strdup(id);
	if (!s)
		return NULL;

	slash = strrchr(s, '/');
	if (!slash) {
		free(s);
		return NULL;
	}

	alias = strdup(&slash[1]);
	free(s);

	return alias;
}

/*
 *	fucntion: get_CPC_id
 *
 *	purpose: returns CPC identifier part from the CPC identifier string
 *		 which possibly can contain an alias.
 */
static char *get_CPC_id(const char *id)
{
	char *s = NULL;
	char *slash = NULL;
	char *CPC_id = NULL;

	s = strdup(id);
	if (!s)
		return NULL;

	slash = strrchr(s, '/');
	if (slash)
		*slash = '\0';
	CPC_id = strdup(s);
	free(s);

	return CPC_id;
}

/*
 *	function: is_cpcid_valid
 *
 *	purpose: checks if given CPCID is valid.
 */
static _Bool is_cpcid_valid(const char *id)
{
	char *alias = NULL;
	char *s = NULL;
	_Bool ret = 0;

	if (!id)
		return 0;
	s = strdup(id);
	if (!s)
		return 0;

	alias = strrchr(s, '/');
	if (alias)
		*alias = '\0';

	ret = is_id(s);
	if (alias)
		ret = (ret && is_id(&alias[1]));

	free(s);
	return ret;
}

/*
 *	function: identify_property
 *
 *	purpose: returns the configuration file property keyword class.
 */
static enum job_config_keyword_class identify_property(const char *keyword)
{
	if (strcasecmp(keyword, "password") == 0)
		return PASSWORD;
	if (strcasecmp(keyword, "type") == 0)
		return TYPE;
	if (strcasecmp(keyword, "user") == 0)
		return USER;
	if (strcasecmp(keyword, "port") == 0)
		return PORT;
	if (strcasecmp(keyword, "image") == 0)
		return IMAGE;
	if (strcasecmp(keyword, "cpcid") == 0)
		return CPCID;
	if (strcasecmp(keyword, "encryption") == 0)
		return ENC;
	if (strcasecmp(keyword, "sslfingerprint") == 0)
		return SSLFINGERPRINT;
	return UNKNOWN;
}
/*
 *	function: verify_config
 *
 *	purpose: verifies the correctness of parsed configuration file data
 */
static int verify_config(const struct sncap_config *config)
{
	struct config_section *cs;
	struct config_property *cp;
	struct config_property *cpcid;
	struct config_property *type;
	char *prb;
	enum job_config_keyword_class key;

	for (cs = config->sections; cs != NULL; cs = cs->next) {
		/*
		 * Check the section key value.
		 */
		if (cs->value == NULL)
			return SNCAP_CONFIG_FILE_ERROR;
		if (strlen(cs->value) == 0)
			return SNCAP_CONFIG_FILE_ERROR;
		/*
		 * Issue a warning message if the section key value is
		 * not unique within the configuration file.
		 */
		if (!config_is_section_unique(config, cs)) {
			fprintf(stderr,
				"WARNING %s:%d %s: '%s'\n",
				config->file_name,
				cs->lineno,
				"multiple servers with the same address",
				cs->value);
		}

		/*
		 * Check if the "cpcid" keyword is not specified in a VM
		 * type section.
		 */
		cpcid = config_section_find_property(cs, "cpcid");
		if (cpcid != NULL) {
			type = config_section_find_property(cs, "type");
			if (type != NULL) {
				if (strcasecmp(type->value, "LPAR") != 0) {
					prb = "CPCID has been "
						"specified for VM server";
					fprintf(stderr,
						"FATAL: %s:%d: %s: '%s'\n",
						config->file_name,
						cpcid->lineno,
						prb,
						cpcid->value);
					return SNCAP_CONFIG_FILE_ERROR;
				}
			}
		}
		/*
		 * Check if the section has valid property names with valid
		 * values.
		 */
		for (cp = cs->properties; cp != NULL; cp = cp->next) {
			key = identify_property(cp->key);
			switch (key) {
			case PASSWORD:
				if (!config_section_property_is_unique(cs,
								cp)) {
					prb = "multiple passwords";
					fprintf(stderr,
						"WARNING: %s:%d %s\n",
						config->file_name,
						cp->lineno,
						prb);
				}
				break;
			case CPCID:
				if (!is_cpcid_valid(cp->value)) {
					prb = "non-proper CPC identifier";
					fprintf(stderr,
						"FATAL: %s:%d %s: '%s'\n",
						config->file_name,
						cp->lineno,
						prb,
						cp->value);
					return SNCAP_CONFIG_FILE_ERROR;
				}
				break;
			case ENC:
				break;
			case TYPE:	/* ignore the TYPE keyword */
				break;
			case USER:	/* ignore the USER keyword */
				break;
			case PORT:	/* ignore the port keyword */
				break;
			case IMAGE:	/* ignore the image keyword */
				break;
			case SSLFINGERPRINT: /* ignore the snipl's keyword */
				break;
			default:
				fprintf(stderr,
					"WARNING: %s:%d %s: '%s'\n",
					config->file_name,
					cp->lineno,
					"invalid keyword",
					cp->key);
			}
		}
	}
	return SNCAP_OK;
}
/*
 *	function: find_CPC_id_property
 *
 *	purpose: searches a config section for a CPCID property which has the
 *		 specified value.
 */
static struct config_property *
find_CPC_id_property(const struct config_section *section,
			const char *CPC_id)
{
	struct config_property *c = NULL;
	struct config_property *found = NULL;
	char *id = NULL;
	char *alias = NULL;

	found = NULL;
	for (c = section->properties; c != NULL; c = c->next) {
		if (strcasecmp(c->key, "cpcid") == 0) {
			id = get_CPC_id(c->value);
			alias = get_CPC_id_alias(c->value);
			if (id != NULL) {
				if (strcasecmp(id, CPC_id) == 0) {
					found = c;
					break;
				}
			}
			if (alias != NULL) {
				if (strcasecmp(alias, CPC_id) == 0) {
					found = c;
					break;
				}
			}
			free(id);
			free(alias);
		}
	}
	if (found != NULL) {
		free(id);
		free(alias);
	}
	return found;
}

/*
 *	function: job_create
 *
 *	purpose: parse command line arguments and set the user job
 *		 attributes
 */
int job_create(struct sncap_job *job, int argc,	char **argv)
{
	int i = 0;
	int ret = SNCAP_OK;
	int option_index = 0;
	int n_options = 0;
	_Bool option_specified[256];
	struct sncap_config config;
	struct config_section *server = NULL;
	int option_code = 0;
	int (*opt_handler)(const char *argument, struct sncap_job *job) = NULL;
	const char *option_string = "-:chnlPtvVxea:d:C:F:f:I:m:p:q:S:z:u:";
	struct config_property *pw_property = NULL;
	struct config_property *enc_property = NULL;
	struct config_property *user_property = NULL;
	struct config_property *CPC_id_property = NULL;
	char *CPC_id_alias = NULL;
	char password[81];
	char *c;
	struct termios initialrsettings;
	struct termios newrsettings;

	job->connection.encryption = UNDEFINED_ENC;
	/*
	 * Parse command line to get the requested operation parameters.
	 */
	memset(option_specified, 0, sizeof(option_specified));
	memset(&config, 0, sizeof(config));
	while (1) {
		/*
		 * Get the option code from the command line.
		 */
		option_code = getopt_long(argc,
				argv,
				option_string,
				long_options,
				&option_index);
		if (option_code == EOF)
			break;
		if (option_code == '?') {
			ret = SNCAP_UNKNOWN_OPTION;
			goto cleanup;
		}
		if (option_code == ':') {
			ret = SNCAP_INVALID_OPTION_VALUE;
			goto cleanup;
		}
		/*
		 * Check if a duplicate option was specified.
		 */
		if (option_specified[option_code]) {
			ret = SNCAP_DUPLICATE_OPTION;
			goto cleanup;
		}
		option_specified[option_code] = 1;
		/*
		 * Find and call the option handling function.
		 */
		opt_handler = NULL;
		for (i = 0; handler[i].option; i++) {
			if (handler[i].option == option_code) {
				opt_handler = handler[i].function;
				n_options++;
				break;
			}
		}
		/*
		 * Process the option.
		 */
		if (opt_handler == NULL) {
			ret = SNCAP_PROGRAM_ERROR;
			goto cleanup;
		}
		ret = (*opt_handler)(optarg, job);
		if (ret != SNCAP_OK)
			goto cleanup;
	}

	if (n_options == 0) {
		ret = SNCAP_NO_OPTIONS;
		goto cleanup;
	}
	/*
	 * If -P (--promptpassword) option has been specified try to prompt
	 * the password from the user.
	 */
	if (job->operation_mode & SNCAP_OP_MODE_PROMPTPW) {
		if (job->connection.password != NULL) {
			ret = SNCAP_CONFLICTING_OPTIONS;
			goto cleanup;
		}
		tcgetattr(fileno(stdin), &initialrsettings);
		newrsettings = initialrsettings;
		newrsettings.c_lflag &= ~ECHO;
		if (tcsetattr(fileno(stdin),
			TCSAFLUSH | TCSANOW,
			&newrsettings) != 0) {
			fprintf(stderr, "Could not set attributes.\n");
			ret = SNCAP_PROGRAM_ERROR;
			goto cleanup;
		} else {
			printf("Enter password: ");
			c = fgets(password, 80, stdin);
			tcsetattr(fileno(stdin),
				TCSAFLUSH | TCSANOW,
				&initialrsettings);
			if (password[strlen(password) - 1] == '\n')
				password[strlen(password) - 1] = '\0';
			job->connection.password = strdup(password);
			printf("\n");
		}
	}
	if (job->operation == LIST_CPCS && !job->connection.address) {
		JOBDEBUG("Missing server address for LIST_CPCS operation.\n");
		ret = SNCAP_NO_SERVER;
		goto cleanup;
	}
	/*
	 * If the server connection parameters have not been specified in the
	 * command line, try to get them from the configuration file.
	 */
	if ((job->connection.address == NULL ||
		!job->connection.password ||
		job->connection.encryption == UNDEFINED_ENC)
		&& (job->operation != VERSION
		&& job->operation != HELP)) {
		config_init(&config);

		JOBDEBUG("Connection parameters have not been fully "
			"specified in the command line.\n");
		JOBDEBUG("Trying to get them from configuration file.\n");

		if (!config_file_exists(job->config_file)) {
			job->connection.encryption = 1;
			ret = SNCAP_OK;
			if (!job->connection.address)
				ret = SNCAP_NO_SERVER;
			else if (!job->connection.password)
				ret = SNCAP_NO_PASSWORD;
			goto cleanup;
		}

		ret = config_load(IS_VERBOSE(job),
				job->config_file,
				"server",
				&config);
		if (ret != SNCAP_OK)
			goto cleanup;
		JOBDEBUG("Parameters loaded from config file '%s'.\n",
			config.file_name);

		ret = verify_config(&config);
		if (ret != SNCAP_OK) {
			JOBDEBUG("Problems with config file has been found.\n");
			goto cleanup;
		}

		if (!job->connection.address) {
			if (!job->CPC_id) {
				JOBDEBUG("Missing CPC identifier.\n");
				ret = SNCAP_MISSING_MANDATORY_OPTION;
				goto cleanup;
			}
			if (config_find_section(&config,
						"cpcid",
						job->CPC_id,
						&server) != SNCAP_OK) {
				ret = SNCAP_NO_SERVER;
				goto cleanup;
			}
			job->connection.address = strdup(server->value);

			if (!job->connection.address) {
				ret = SNCAP_MEMORY_ALLOC_FAILURE;
				goto cleanup;
				}
			JOBDEBUG("Server address from config file "
				"'%s' is '%s'\n",
				config.file_name,
				job->connection.address);
			/*
			 * Check if an alias has been specified in command line
			 * and if it has, translate it to CPC identifier.
			 */
			CPC_id_property = find_CPC_id_property(server,
							job->CPC_id);
			if (CPC_id_property == NULL)
				return SNCAP_PROGRAM_ERROR;
			CPC_id_alias = get_CPC_id_alias(CPC_id_property->value);
			if (CPC_id_alias != NULL) {
				free(job->CPC_id);
				job->CPC_id =
					get_CPC_id(CPC_id_property->value);
				free(CPC_id_alias);
			}
		}
		if (!job->connection.password) {
			if (!server)
				if (config_find_section(&config,
						"server",
						job->connection.address,
						&server) != SNCAP_OK) {
					ret = SNCAP_NO_PASSWORD;
					goto cleanup;
				}

			pw_property = config_section_find_property(server,
							"password");
			if (!pw_property) {
				ret = SNCAP_NO_PASSWORD;
				goto cleanup;
			}
			job->connection.password = strdup(pw_property->value);
			if (!job->connection.password) {
				ret = SNCAP_MEMORY_ALLOC_FAILURE;
				goto cleanup;
			}
			JOBDEBUG("Server password from config file "
				"'%s' is '%s'\n",
				config.file_name,
				SNCAP_PASSWORD_MASK);
		}
		if (job->connection.encryption == UNDEFINED_ENC) {
			if (!server)
				if (config_find_section(&config,
							"server",
							job->connection.address,
							&server) != SNCAP_OK) {
					job->connection.encryption = 1;
					goto get_username;
					}
			enc_property = config_section_find_property(server,
								    "encryption");
			if (enc_property) {
				if (strcasecmp("yes", enc_property->value) == 0)
					job->connection.encryption = 1;
				else if (strcasecmp("no", enc_property->value) == 0)
					job->connection.encryption = 0;
				else {
					ret = SNCAP_WRONG_ENC;
					goto cleanup;
				}
				JOBDEBUG("Server encryption from config file "
					 "'%s' is '%s'\n",
					 config.file_name, enc_property->value);
			} else {
				job->connection.encryption = 1;
			}
		}
get_username:
		if (!job->connection.username) {
			if (!server)
				if (config_find_section(&config,
							"server",
							job->connection.address,
							&server) != SNCAP_OK) {
					ret = SNCAP_NO_USERNAME;
					goto cleanup;
				}

			user_property = config_section_find_property(server,
								     "user");
			if (user_property && !job->connection.encryption) {
				ret = SNCAP_USERNAME_ENC_OFF;
				goto cleanup;
			}
			if (!user_property && job->connection.encryption) {
				ret = SNCAP_NO_USERNAME;
				goto cleanup;
			}
			if (!user_property && !job->connection.encryption)
				goto cleanup;

			job->connection.username = strdup(user_property->value);
			if (!job->connection.username) {
				ret = SNCAP_MEMORY_ALLOC_FAILURE;
				goto cleanup;
			}
			JOBDEBUG("Server userid from config file "
				 "'%s' is '%s'\n",
				 config.file_name, user_property->value);
		}
	}
cleanup:
	if (server && IS_VERBOSE(job)) {
		JOBDEBUG("\nCurrent configuration file section is:\n");
		config_section_print(server);
		printf("\n");
	}
	config_release(&config);
	return ret;
}

/*
 *	function: job_procinfo_specified
 *
 *	purpose: verifies if the processor information parameters have been
 *		 specified.
 */
static _Bool job_procinfo_specified(const struct sncap_job *job)
{
	int i;

	for (i = 0;
	    i < sizeof(job->cpu_quantity) / sizeof(job->cpu_quantity[0]);
	    i++)
		if (job->cpu_quantity[i] > 0)
			return 1;
	return 0;
}

/*
 *	function: job_verify_aux_action
 *
 *	purpose: check the parameter integrity for the auxilary operations
 *		 like HELP and VERSION.
 */
static int job_verify_aux_action(const struct sncap_job *job)
{
	if (job->CPC_id != NULL)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->TCR_id != NULL)
		return SNCAP_CONFLICTING_OPTIONS;
	if (IS_VERBOSE(job) && job->operation_mode != 0)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->config_file != NULL)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->software_model != NULL)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->connection.address != NULL)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->connection.password != NULL)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->connection.username)
		return SNCAP_CONFLICTING_OPTIONS;
	if (!job->connection.encryption)
		return SNCAP_CONFLICTING_OPTIONS;
	if (job->connection.timeout >= 0)
		return SNCAP_CONFLICTING_OPTIONS;

	if (job_procinfo_specified(job))
		return SNCAP_CONFLICTING_OPTIONS;

	return SNCAP_OK;
}

/*
 *	function: job_verify_connection
 *
 *	purpose: verify job connection parameters.
 */
static int job_verify_connection(const struct sncap_connection *connection)
{
	if (connection->address == NULL)
		return SNCAP_NO_SERVER;
	if (strlen(connection->address) == 0)
		return SNCAP_NO_SERVER;

	if (connection->password == NULL)
		return SNCAP_NO_PASSWORD;
	if (!connection->username && connection->encryption)
		return SNCAP_NO_USERNAME;
	if (connection->username && !connection->encryption)
		return SNCAP_USERNAME_ENC_OFF;
	if (connection->timeout < -1)
		return SNCAP_INVALID_OPTION_VALUE;

	return SNCAP_OK;
}

/*
 *	function: job_verify_action
 *
 *	purpose: verify parameters for the program action operation like
 *		 ACTIVATE and DEACTIVATE.
 */
static int job_verify_action(const struct sncap_job *job)
{
	if (job->CPC_id) {
		if (!is_id(job->CPC_id))
			return SNCAP_INVALID_OPTION_VALUE;
	} else {
		JOBDEBUG("Missing CPC identifier.\n");
		return SNCAP_MISSING_MANDATORY_OPTION;
	}

	if (!is_id(job->TCR_id))
		return SNCAP_INVALID_OPTION_VALUE;

	return job_verify_connection(&job->connection);
}

/*
 *	function: job_verify_query
 *
 *	purpose: verify query operation parameters.
 */
static int job_verify_query(const struct sncap_job *job)
{
	int ret = SNCAP_OK;

	if (job->CPC_id) {
		if (!is_id(job->CPC_id))
			return SNCAP_INVALID_OPTION_VALUE;
	} else {
		JOBDEBUG("Missing CPC identifier.\n");
		return SNCAP_MISSING_MANDATORY_OPTION;
	}

	ret = job_verify_connection(&job->connection);
	if (ret != SNCAP_OK)
		return ret;

	if (job_procinfo_specified(job))
		return SNCAP_CONFLICTING_OPTIONS;

	if ((job->operation_mode & SNCAP_OP_MODE_TEST)
	   || (job->operation_mode & SNCAP_OP_MODE_DRYRUN))
		return SNCAP_CONFLICTING_OPTIONS;

	return SNCAP_OK;
}

/*
 *	function: job_verify_list_cpcs
 *
 *	purpose: verify the list_cpcs operation parameters.
 */
static int job_verify_list_cpcs(const struct sncap_job *job)
{
	if (job->CPC_id)
		return SNCAP_CONFLICTING_OPTIONS;

	if (job_procinfo_specified(job))
		return SNCAP_CONFLICTING_OPTIONS;

	if ((job->operation_mode & SNCAP_OP_MODE_TEST)
	   || (job->operation_mode & SNCAP_OP_MODE_DRYRUN))
		return SNCAP_CONFLICTING_OPTIONS;

	return job_verify_connection(&job->connection);
}

/*
 *	function: job_verify
 *
 *	purpose: verify the arguments for the operation specified in the
 *		 command line.
 */
int job_verify(const struct sncap_job *job)
{
	int ret = SNCAP_OK;
	int i = 0;
	int pu_count = 0;

	switch (job->operation) {
	case ACTIVATE:
		if (job->software_model == NULL) {
			pu_count = 0;
			for (i = 0; i < 5; i++)
				if (job->cpu_quantity[i] > 0)
					pu_count += 1;
			if (pu_count == 0) {
				JOBDEBUG("No Model defined and no available PU found.\n");
				return SNCAP_MISSING_MANDATORY_OPTION;
			}
		}
		return job_verify_action(job);
		break;
	case DEACTIVATE:
		ret = job_verify_action(job);
		if (ret != SNCAP_OK)
			return ret;
		if (job->operation_mode & SNCAP_OP_MODE_TEST)
			return SNCAP_CONFLICTING_OPTIONS;
		break;
	case LIST_CPCS:
		return job_verify_list_cpcs(job);
		break;
	case QUERY_CPUS:
		return job_verify_query(job);
		break;
	case QUERY_LIST:
		ret = job_verify_query(job);
		if (ret != SNCAP_OK)
			return ret;
		if (is_id(job->TCR_id))
			return SNCAP_PROGRAM_ERROR;
		break;
	case QUERY_RECORD:
		if (!is_id(job->TCR_id))
			return SNCAP_INVALID_OPTION_VALUE;
		return job_verify_query(job);
		break;
	case HELP:
		return job_verify_aux_action(job);
		break;
	case VERSION:
		return job_verify_aux_action(job);
		break;
	case UNDEFINED:
		return SNCAP_OPERATION_MISSED;
		break;
	default:
		return SNCAP_PROGRAM_ERROR;
	}
	return SNCAP_OK;
}

/*
 *      function: job_generate_xml
 *
 *      purpose: creates the processor information XML description for
 *               the HwmcaCommand API call for the temporary capacity
 *               record activation/deactivation.
 */
int job_generate_xml(const struct sncap_job *job,
			const char *actual_software_model,
			const int *cpu_quantity,
			char **xml)
{
	int i = 0;
	int cpu_count = 0;
	int xml_length = 0;
	const char *mode = NULL;

	const char *xml_head = NULL;
	const char *xml_tail = NULL;

	static const char record_id_xml[] = {"<recordid>%s</recordid>\n"};
	static const char soft_model_xml[] = {
		"<softwaremodel>%s</softwaremodel>\n"
	};
	static const char test_xml[] = {"<test>%s</test>\n"};
	static const char procinfo_xml[] = {
		"<processorinfo>"
		"<type>%s</type>"
		"<procstep>%d</procstep>"
		"</processorinfo>\n"
	};

	assert(job);
	switch (job->operation) {
	case ACTIVATE:
		xml_head = "<add>\n";
		xml_tail = "</add>\n";
		break;
	case DEACTIVATE:
		xml_head = "<remove>\n";
		xml_tail = "</remove>\n";
		break;
	default:
		JOBDEBUG("Invalid job operation: %d.\n", job->operation);
		return SNCAP_PROGRAM_ERROR;
	}
	/*
	 * Calculate length of XML string.
	 */
	xml_length = strlen(xml_head) + strlen(record_id_xml)
		+ strlen(job->TCR_id) - 2;
	if (actual_software_model)
		xml_length += strlen(soft_model_xml)
			+ strlen(actual_software_model) - 2;
	cpu_count = 0;
	for (i = 0; i < 5; i++)
		if (cpu_quantity[i] > 0) {
			xml_length += strlen(procinfo_xml) + 1 + 7;
			cpu_count += 1;
		}
	if (job->operation == ACTIVATE) {
		if (job->operation_mode & SNCAP_OP_MODE_TEST)
			xml_length += strlen(test_xml) + 2;
		else
			xml_length += strlen(test_xml) + 3;
	}
	xml_length += strlen(xml_tail);
	JOBDEBUG("Calculated XML length is: %d\n", xml_length);
	*xml = calloc(xml_length + 1, sizeof(**xml));
	if (!*xml)
		return SNCAP_MEMORY_ALLOC_FAILURE;
	/*
	 * Generate the XML string.
	 */
	strcpy(*xml, xml_head);
	sprintf(&(*xml)[strlen(*xml)], record_id_xml, job->TCR_id);

	if (actual_software_model)
		sprintf(&(*xml)[strlen(*xml)],
			soft_model_xml,
			actual_software_model);

	if (cpu_count)
		for (i = 0; i < 5; i++)
			if (cpu_quantity[i] > 0)
				sprintf(&(*xml)[strlen(*xml)],
					procinfo_xml,
					cpu_type[i],
					cpu_quantity[i]);
	if (job->operation == ACTIVATE) {
		if (job->operation_mode & SNCAP_OP_MODE_TEST)
			mode = "true";
		else
			mode = "false";
		sprintf(&(*xml)[strlen(*xml)], test_xml, mode);
	}

	strcat(*xml, xml_tail);

	JOBDEBUG("The actual XML length is: %d\n", (int)strlen(*xml));
	JOBDEBUG("The following XML has been generated:\n'%s'.\n", *xml);

	return SNCAP_OK;
}

/*
 *	function: sncap_job_print
 *
 *	purpose: print a Job structure attributes to the stdout stream
 */
void job_print(const struct sncap_job *job)
{
	char *s = NULL;

	printf("The results of parsing the command line:\n\n");
	s = job->CPC_id;
	if (!s)
		s = "not specified";
	printf("CPC identifier......................: '%s'\n", s);

	s = job->TCR_id;
	if (!s)
		s = "not specified";
	printf("Temporary capacity record identifier: '%s'\n", s);

	if (job->operation == ACTIVATE) {
		if (job->operation_mode & SNCAP_OP_MODE_TEST)
			printf("A TEST activation has been requested.\n");
		else
			printf("A REAL activation has been requested.\n");
	}

	if (IS_VERBOSE(job))
		printf("Running in verbose mode.\n");
	else
		printf("Running in normal mode.\n");

	if (job->operation_mode & SNCAP_OP_MODE_DRYRUN) {
		printf("Running in the 'No Record Changes' mode.\n");
		printf("The record state will not be changed by activation");
		printf("/deactivation.\n");
	}

	if (job->operation_mode & SNCAP_OP_MODE_PROMPTPW)
		printf("The password has been prompted.\n");

	printf("Requested operation is..............: ");
	switch (job->operation) {
	case ACTIVATE:
		printf("Record activation");
		break;
	case DEACTIVATE:
		printf("Record deactivation");
		break;
	case LIST_CPCS:
		printf("Print CPC list");
		break;
	case QUERY_CPUS:
		printf("Query CPC CPU configuration");
		break;
	case QUERY_LIST:
		printf("Query record list");
		break;
	case QUERY_RECORD:
		printf("Query record data");
		break;
	case HELP:
		printf("Help");
		break;
	case VERSION:
		printf("Query version");
		break;
	default:
		printf("UNDEFINED");
	}
	printf("\n");

	if (job->config_file)
		printf("Specified configuration file name...: '%s'\n",
			job->config_file);
	else
		printf("Configuration file has not been specified.\n");

	if (job->software_model)
		printf("Specified software model............: '%s'\n",
			job->software_model);
	else
		printf("Software model has not been specified.\n");

	connection_print(&job->connection);

	printf("Requested number of ICF processors..: %d\n",
		job->cpu_quantity[CPU_TYPE_ICF]);
	printf("Requested number of IFL processors..: %d\n",
		job->cpu_quantity[CPU_TYPE_IFL]);
	printf("Requested number of SAP processors..: %d\n",
		job->cpu_quantity[CPU_TYPE_SAP]);
	printf("Requested number of ZAAP processors.: %d\n",
		job->cpu_quantity[CPU_TYPE_ZAAP]);
	printf("Requested number of ZIIP processors.: %d\n",
		job->cpu_quantity[CPU_TYPE_ZIIP]);
	printf("*** End of Job Data ***\n\n");
}

/*
 *      function: job_release
 *
 *      purpose: releases memory used by sncap_job object data.
 */
void job_release(struct sncap_job *job)
{
	free(job->CPC_id);
	job->CPC_id = NULL;

	free(job->TCR_id);
	job->TCR_id = NULL;

	free(job->config_file);
	job->config_file = NULL;

	free(job->software_model);
	job->software_model = NULL;

	connection_release(&job->connection);
}
