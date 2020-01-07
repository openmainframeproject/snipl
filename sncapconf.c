/*
 * sncapconf.c : API for snCAP configuration file access
 *
 * Copyright IBM Corp. 2012, 2013
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */
#include "sncapconf.h"
/*
 *      function: config_section_find_property
 *
 *      purpose: returns the value of the specified property key
 */
struct config_property *
config_section_find_property(const struct config_section *section,
				const char *key)
{
	struct config_property *c = NULL;

	for (c = section->properties; c != NULL; c = c->next)
		if (strcasecmp(c->key, key) == 0)
			return c;
	return NULL;
}

/*
 *	function: config_is_section_unique
 *
 *	purpose: checks multiple section definitions case in configuration
 *		 structures.
 */
_Bool config_is_section_unique(const struct sncap_config *cfg,
				const struct config_section *section)
{
	struct config_section *cs = NULL;

	for (cs = cfg->sections; cs != NULL; cs = cs->next)
		if (strcasecmp(cs->value, section->value) == 0
		   && cs != section)
			return 0;
	return 1;
}

/*
 *      function: config_section_propery_is_unique
 *
 *      purpose: checks for multiple property definition in property list
 *               of configuration section.
 */
_Bool config_section_property_is_unique(const struct config_section *section,
					const struct config_property *property)
{
	struct config_property *cp = NULL;

	for (cp = section->properties; cp != NULL; cp = cp->next)
		if (cp != property && strcasecmp(cp->key, property->key) == 0)
			return 0;
	return 1;
}

/*
 *      function: config_section_print
 *
 *      purpose: prints the contents of configuration file section to the stdout
 *               stream.
 */
void config_section_print(const struct config_section *section)
{
	struct config_property *cp = NULL;
	char *value = NULL;

	printf("%s = %s\n", section->key, section->value);
	for (cp = section->properties; cp != NULL; cp = cp->next) {
		if (strcasecmp(cp->key, "password") != 0)
			value = cp->value;
		else
			value = SNCAP_PASSWORD_MASK;
		printf("%s = %s\n", cp->key, value);
	}

	printf("*** End of Section Data ***\n");
}

/*
 *	function: config_init
 *
 *	purpose: set initial values to the strucutre fields.
 */
void config_init(struct sncap_config *cfg)
{
	cfg->file_name = NULL;
	cfg->sections = NULL;
	cfg->tail = NULL;
}

/*
 *	function: config_add_key
 *
 *	purpose: adds a new key to the tail of the configuration key
 *		 list.
 */
static int config_add_property(struct sncap_config *cfg,
			struct config_property *property,
			char **problem)
{
	struct config_section *section = NULL;

	section = cfg->tail;

	if (section == NULL) {
		*problem = "undefined section";
		return FATAL;
	}

	if (section->tail == NULL) {
		section->properties = property;
		section->tail = property;
	} else {
		property->prev = section->tail;
		section->tail->next = property;
		property->next = NULL;
		section->tail = property;
	}
	return SNCAP_OK;
}

/*
 *	function: config_get_file_name
 *
 *	purpose: returns the name of the current configuration file.
 */
int config_get_file_name(const char *curr_name, char **file_name)
{
	char *home_dir = getenv("HOME");

	if (curr_name) {
		if (0 == access(curr_name, R_OK)) {
			*file_name = strdup(curr_name);
			return SNCAP_OK;
		}
	}
	*file_name = calloc(1, strlen(home_dir) + strlen(SNCAP_HOME_CFG) + 2);
	if (*file_name == NULL)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	strcpy(*file_name, home_dir);
	strcat(*file_name, "/");
	strcat(*file_name, SNCAP_HOME_CFG);
	if (0 == access(*file_name, R_OK))
		return SNCAP_OK;

	free(*file_name);
	*file_name = calloc(1, strlen(SNCAP_ETC_CFG) + 1);
	strcpy(*file_name, SNCAP_ETC_CFG);
	if (0 == access(*file_name, R_OK))
		return SNCAP_OK;

	return SNCAP_CONFIG_FILE_ERROR;
}

/*
 *      function: config_file_exists
 *
 *      purpose: verifies configuration file existence.
 */
int config_file_exists(const char *file_name)
{
	char *file_path = NULL;
	int ret;

	ret = config_get_file_name(file_name, &file_path);
	free(file_path);

	return (ret == SNCAP_OK);
}

/*
 *	function: config_read
 *
 *	purpose: reads cofiguration file into memory buffer.
 */
static int config_read(const char *file_name, char **buffer)
{
	char *pos;
	struct stat statbuf;
	int fd;
	int ret;
	size_t size;

	fd = open(file_name, O_RDONLY);
	if (fd == -1)
		return SNCAP_CONFIG_FILE_ERROR;

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		ret = SNCAP_CONFIG_FILE_ERROR;
		goto cleanup;
	}

	size = statbuf.st_size;
	*buffer = calloc(1, size + 1);
	if (!*buffer) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}

	pos = *buffer;
	do {
		ret = read(fd, pos, size);
		if (ret < 0) {
			ret = SNCAP_CONFIG_FILE_ERROR;
			goto cleanup;
		}
		pos += ret;
		size -= ret;
	} while (ret);
	*pos = '\0';
cleanup:
	close(fd);
	return ret;
}

/*
 *	function: config_property_release
 *
 *	purpose: releases memory used by key fields.
 */
static void config_property_release(const struct config_property *property)
{
	if (!property)
		return;
	free(property->key);
	free(property->value);
}
/*
 *	function: config_section_release
 *
 *	purpose: releases memory used by the section data.
 */
static void config_section_release(struct config_section *section)
{
	struct config_property *c = NULL;

	if (section == NULL)
		return;

	free(section->value);
	free(section->key);

	c = section->properties;
	while (c != NULL)  {
		section->properties = c->next;
		config_property_release(c);
		free(c);
		c = section->properties;
	}
}
/*
 *	function: config_create_section
 *
 *	purpose: creates the memory structure for a configuration file
 *		 section.
 */
int config_create_section(struct sncap_config *cfg,
			const int lineno,
			const char *key,
			const char *value)
{
	int ret = SNCAP_OK;
	struct config_section *new_section = NULL;

	/*
	 * Create a new section
	 */
	new_section = calloc(1, sizeof(*new_section));
	if (!new_section)
		return SNCAP_MEMORY_ALLOC_FAILURE;

	new_section->key = strdup(key);
	new_section->value = strdup(value);
	if (!new_section->key || !new_section->value) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	/*
	 * Add the created section to the section list.
	 */
	if (cfg->sections == NULL) {
		cfg->sections = new_section;
		cfg->tail = new_section;
	} else {
		cfg->tail->next = new_section;
		new_section->prev = cfg->tail;
		cfg->tail = new_section;
	}
cleanup:
	if (ret != SNCAP_OK && new_section) {
		config_section_release(new_section);
		free(new_section);
	}
	return ret;
}
/*
 *	function: config_parse_line
 *
 *	purpose: parses a string as a configuration file line.
 */
static int config_parse_line(struct sncap_config *cfg,
			const char *section_key,
			const int lineno,
			char *buffer,
			regex_t *re,
			char **problem)
{
	regmatch_t pmatch[3];
	char *key;
	char *value;
	struct config_property *new_property = NULL;
	int ret = 0;

	if (regexec(re, buffer, 3, pmatch, 0) == REG_NOMATCH
	    || pmatch[1].rm_so == -1
	    || pmatch[2].rm_so == -1) {
		*problem = "syntax error";
		return FATAL;
	}

	*problem = "";
	key = buffer + pmatch[1].rm_so;
	buffer[pmatch[1].rm_eo] = '\0';
	value = buffer + pmatch[2].rm_so;
	buffer[pmatch[2].rm_eo] = '\0';

	if (strcasecmp(key, section_key) == 0) {
		if (config_create_section(cfg,
					lineno,
					key,
					value) != SNCAP_OK) {
			*problem = "cannot create section";
			ret = FATAL;
			goto cleanup;
		}
	} else {
		ret = SNCAP_OK;
		new_property = calloc(1, sizeof(*new_property));
		if (!new_property) {
			*problem = "out of memory";
			ret = FATAL;
			goto cleanup;
		}
		new_property->key = strdup(key);
		if (!new_property) {
			*problem = "out of memory";
			ret = FATAL;
			goto cleanup;
		}
		new_property->value = strdup(value);
		if (!new_property) {
			*problem = "out of memory";
			ret = FATAL;
			goto cleanup;
		}
		new_property->lineno = lineno;
		config_add_property(cfg, new_property, problem);
	}
cleanup:
	if (ret != SNCAP_OK && new_property) {
		config_property_release(new_property);
		free(new_property);
	}
	return ret;
}

/*
 *	function: config_parse_buffer
 *
 *	purpose: parses given charater string and creates internal sncap
 *		 configuration memory structures.
 */
static int config_parse_buffer(const _Bool verbose,
				struct sncap_config *cfg,
				const char *section_key,
				const char *pattern,
				char *buffer)
{
	static char regerror_string[64];
	static const char token_pattern[] =
			"^[[:blank:]]*([[:alpha:]]+)[[:blank:]]*"
			"=[[:blank:]]*([^[:blank:]]+)";
	char *pos;
	char *end;
	char *problem;
	regex_t line;
	regex_t token;
	regmatch_t pmatch[2];
	int lineno;
	int ret;
	FILE *msg_out = NULL;

	if (verbose)
		msg_out = stdout;
	else
		msg_out = stderr;
	/*
	 * Compile the expressions.
	 */
	ret = regcomp(&line, pattern, REG_EXTENDED | REG_NEWLINE);
	if (ret) {
		regerror(ret, &line, regerror_string, sizeof(regerror_string));
		fprintf(msg_out, "%s\n", regerror_string);
		return SNCAP_CONFIG_FILE_ERROR;
	}

	ret = regcomp(&token, token_pattern, REG_EXTENDED | REG_NEWLINE);
	if (ret) {
		regerror(ret, &token, regerror_string, sizeof(regerror_string));
		fprintf(msg_out, "%s\n", regerror_string);
		return SNCAP_CONFIG_FILE_ERROR;
	}

	lineno = 1;
	pos = buffer;
	end = pos + strlen(buffer);
	/*
	 * Scan input line by line.
	 */
	for (; pos < end; pos += pmatch[0].rm_eo + 1) {
		if (regexec(&line, pos, 2, pmatch, 0) == REG_NOMATCH)
			break;
		if (pmatch[1].rm_so != -1) {
			pos[pmatch[1].rm_eo] = '\0';
			ret = config_parse_line(cfg,
					section_key,
					lineno,
					pos,
					&token,
					&problem);
		}
		if (ret != SNCAP_OK) {
			fprintf(msg_out, "FATAL: %s:%d: %s: '%s'\n",
				cfg->file_name,
				lineno,
				problem,
				pos);
			goto cleanup;
		}
		lineno++;
	}
cleanup:
	regfree(&line);
	regfree(&token);
	if (ret != FATAL)
		return SNCAP_OK;

	return SNCAP_CONFIG_FILE_ERROR;
}

/*
 *	function: config_load
 *
 *	purpose: read disk file data, parse the data and create the internal
 *		 memory structures.
 */
int config_load(const _Bool verbose,
		const char *file_path,
		const char *section_key,
		struct sncap_config *cfg)

{
	int ret = 0;
	char *buffer = NULL;

	ret = config_get_file_name(file_path, &cfg->file_name);
	if (ret != SNCAP_OK)
		goto cleanup;

	ret = config_read(cfg->file_name, &buffer);
	if (ret != SNCAP_OK)
		goto cleanup;

	ret = config_parse_buffer(verbose,
				cfg,
				section_key,
				"([^[:space:]#]+[[:blank:]]*)?(#.*)?$",
				buffer);
cleanup:
	free(buffer);
	return ret;
}

/*
 *	function: value_is_equal
 *
 *	purpose: comapres the property value with value, taking into
 *		 account the property value alias part.
 */
static int value_is_equal(const char *property_value, const char *value)
{
	char *alias = NULL;

	alias = strrchr(property_value, '/');
	if (alias == NULL)
		return strcasecmp(property_value, value);
	else {
		if (strncasecmp(property_value,
				value,
				alias - property_value) == 0)
			return 0;
		return strcasecmp(alias + 1, value);
	}
}

/*
 *	function: config_find_section
 *
 *	purpose: returns the address of configuration section containing the
 *		 specified key-value pair. Key value is case insensitive, and
 *		 value can contain the value alias. If the section does not
 *		 exsist returns SNCAP_NOT_FOUND.
 */
int config_find_section(const struct sncap_config *cfg,
			const char *key,
			const char *value,
			struct config_section **section)
{
	struct config_section *c = NULL;
	struct config_property *cp = NULL;

	for (c = cfg->sections; c != NULL; c = c->next) {
		/*
		 * Check if the section key is being searched.
		 */
		if (strcasecmp(c->key, key) == 0)
			if (strcasecmp(c->value, value) == 0) {
				*section = c;
				return SNCAP_OK;
			}
		/*
		 * No, that is not a key, look for an appropriate property.
		 */
		for (cp = c->properties; cp != NULL; cp = cp->next) {
			if (strcasecmp(cp->key, key) == 0) {
				if (value_is_equal(cp->value, value) == 0) {
					*section = c;
					return SNCAP_OK;
				}
			}
		}
	}
	return SNCAP_NOT_FOUND;
}

/*
 *	function: config_release
 *
 *	purpose: releases memory used by the configuration data strucutres.
 */
void config_release(struct sncap_config *cfg)
{
	struct config_section *to_free = NULL;

	free(cfg->file_name);
	cfg->file_name = NULL;

	to_free = cfg->sections;
	while (to_free != NULL) {
		cfg->sections = to_free->next;
		config_section_release(to_free);
		free(to_free);
		to_free = cfg->sections;
	}
}
