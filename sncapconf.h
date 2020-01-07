/*
 * sncapconf.h : header file for snCAP configuration file access API
 *
 * Copyright IBM Corp. 2012, 2016
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reprduction or distribution of the program
 *   constitutes recipient;s acceptance of this agreement.
 */
#ifndef SNCAPCONFH
#define SNCAPCONFH

#include <stdio.h>
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
#include "sncaputil.h"

#define SNCAP_HOME_CFG ".snipl.conf"
#define SNCAP_ETC_CFG "/etc/snipl.conf"

#define UNDEFINED_ENC 3
struct sncap_config;
/*
 * Problem severity ranks
 */
enum problem_class {
	OK,
	WARNING,
	FATAL,
};

/*
 * Configuration file data item
 */
struct config_property {
	struct config_property *next;
	struct config_property *prev;
	int  lineno;
	char *key;
	char *value;
};
/*
 * Configuration file section
 */
struct config_section {
	struct config_section *next;
	struct config_section *prev;
	int lineno;
	char *key;
	char *value;
	struct config_property *properties;
	struct config_property *tail;
};

/*
 *	function: config_section_find_property
 *
 *	purpose: returns the value of the specified property key
 */
struct config_property *
config_section_find_property(const struct config_section *section,
			const char *key);

/*
 *	function: config_is_section_unique
 *
 *	purpose: checks for multiple section definition in configuration
 *		 structure.
 */
_Bool config_is_section_unique(const struct sncap_config *cfg,
				const struct config_section *section);

/*
 *	function: config_section_propery_is_unique
 *
 *	purpose: checks for multiple property definitions in property list
 *		 of configuration section.
 */
_Bool config_section_property_is_unique(const struct config_section *section,
					const struct config_property *property);

/*
 *	function: config_section_print
 *
 *	purpose: prints the contents of configuration file section to the stdout
 *		 stream.
 */
void config_section_print(const struct config_section *section);

/*
 * Configuration data memory sturcture
 */
struct sncap_config {
	char *file_name;
	struct config_section *sections;
	struct config_section *tail;
};

/*
 *	function: config_init
 *
 *	purpose: initializes configuration file strucutre fields.
 */
void config_init(struct sncap_config *cfg);

/*
 *	function: config_file_exists
 *
 *	purpose: verifies configuration file existence.
 */
int config_file_exists(const char *file_name);

/*
 *	function: config_load
 *
 *	purpose: open a disk config file and read it contents into memory
 *		 building the internal configuration structure.
 */
int config_load(const _Bool verbose,
		const char *file_path,
		const char *section_key,
		struct sncap_config *cfg);

/*
 *	function: config_find_section
 *
 *	purpose: returns the address of a section containing the specified
 *		 key-value pair. The keys are searched using the value or
 *		 the value alias.
 */
int config_find_section(const struct sncap_config *cfg,
			const char *key,
			const char *value,
			struct config_section **section);
/*
 *	function: config_release
 *
 *	purpose: releases memory used by the sncap configuration data
 *		 structures.
 */
void config_release(struct sncap_config *cfg);
#endif
