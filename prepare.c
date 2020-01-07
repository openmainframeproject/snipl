/*
   prepare.c - prepare server and image actions for chosen server type

   Copyright IBM Corp. 2003, 2011
   Author(s): Peter Tiedemann    (ptiedem@de.ibm.com)
   Maintainer: Ursula Braun      (ursula.braun@de.ibm.com)

   Published under the terms and conditions of the CPL (common public license)

   PLEASE NOTE:
   config is provided under the terms of the enclosed common public license
   ("agreement"). Any use, reproduction or distribution of the program
   constitutes recipient's acceptance of this agreement.

*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "snipl.h"

struct system_type_map {
	char *type_name;
	char *module_name;	/* for use with dlopen */
};
static struct system_type_map module_map[3] = {
	{"VM5",		"libvmsmapi.so"},
	{"VM",		"libvmsmapi6.so"},
	{"LPAR",	"libsniplapi.so"}
};

/*
 *	return the modulename corresponding to system_type
 */
static char *module_name(const char * system_type)
{
	unsigned int x;

	if (system_type) {
		for (x = 0; x < DIMOF(module_map); ++x) {
			if (strcasecmp(module_map[x].type_name, system_type) ==
			    0)
				return module_map[x].module_name;
		}
	}
	return NULL;
}


/*
 *	function: snipl_load_module
 *
 *	purpose: load module defined in module_map
 *               corresponding to server type name
 *
 *      returns _Bool : 1 if module was successful loaded
 *			0 in case of errors
 *	server->problem contains a detailed error-description
 */
static _Bool snipl_load_module(struct snipl_server *server)
{
	char *modulename;

	if (server == NULL)
		return 0;

	modulename = module_name(server->type);
	if (modulename == NULL){
		create_msg(server, "Modulename for type %s not found\n",
			server->type);
		server->problem_class = FATAL;
		return 0;
	}
	if (server->module_handle) {
		int rc = dlclose(server->module_handle);
		if (rc) {
			create_msg(server, "Loading API module fails with %s\n",
				dlerror());
			server->problem_class = FATAL;
			return 0;
		} else server->module_handle = NULL;
	}
	server->module_handle = dlopen(modulename, RTLD_NOW);
	/* (RTLD_NOW or RTLD_LAZY) [+ RTLD_GLOBAL] */
	if (!server->module_handle) {
		create_msg(server, "Loading API module fails with %s\n",
			dlerror());
		server->problem_class = FATAL;
		return 0;
	}
	return 1;
}


int snipl_prepare(struct snipl_server *server)
{
	char* error;
	void (*prep)(struct snipl_server *);

	if (!snipl_load_module(server))
		return LIB_LOAD_PROBLEM;
	prep = dlsym(server->module_handle, "prepare");
	error = dlerror();
	if (error) {
		create_msg(server, "Loading API module fails with %s\n", error);
		server->problem_class = FATAL;
		return LIB_LOAD_PROBLEM;
	}
	prep(server);
	return 0;
}


void create_msg(struct snipl_server *server, const char *fmt, ...)
{
	int n, size = 100;
	char *p;
	va_list ap;
	char *oldprob = server->problem;

	p = calloc(1, size);
	if (!p) {
		server->problem = strdup("cannot allocate storage for message");
		goto free_oldprob;
	}
	while (1) {
		va_start(ap, fmt);
		n = vsnprintf(p, size, fmt, ap);
		va_end(ap);
		if (n > -1 && n < size) {
			if (!memcmp(p, "(null)", 6)) {
				memmove(p, &p[6], strlen(p)-6);
				memmove(&p[strlen(p)-6], "\n\0", 2);
			}
			server->problem = p;
			goto free_oldprob;
		}
		size = n + 1;
		p = realloc(p, size);
		memset(p, 0, size);
		if (!p) {
			server->problem =
				strdup("cannot allocate storage for message");
			goto free_oldprob;
		}
	}
free_oldprob:
	free(oldprob);
}

