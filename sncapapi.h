/*
 * sncapapi.h : header file for the HWMCAAPI API interface.
 *
 * Copyright IBM Corp. 2012, 2016
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *  config is provided under the terms of the enclosed common public license
 *  ("agreement"). Any use, reproduction or distribution of the program
 *  constitutes recipient's acceptance of this agreement.
 */
#ifndef SNCAPAPIH
#define SNCAPAPIH

#include <assert.h>
#include <sys/types.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "hwmcaapi.h"
#include "sncaputil.h"
#include "sncaptcr.h"
#include "sncapcpc.h"

#define APIDEBUG(msg, ...) {\
	if (api->verbose) \
		printf(msg, ##__VA_ARGS__); \
}

#define APIDEBUGV(msg, ...) {\
	if (verbose) \
		printf(msg, ##__VA_ARGS__); \
}

#define RESPONSE_RC_POSITION 5
#define RESPONSE_CORRELATOR_POSITION 11

struct sncap_job;

/*
 * Support Element connection attributes
 */
struct sncap_connection {
	char *address;
	char *password;
	int  timeout;
	char *username;
	int encryption;
};

/*
 *	fucntion: connection_print
 *
 *	purpose: print connection atribute values.
 */
void connection_print(const struct sncap_connection *connection);

/*
 *      function: connection_release
 *
 *      purpose: release memory used by connection data.
 */
void connection_release(struct sncap_connection *connection);

/*
 * HWMCAAPI API interface session data
 */
struct sncap_api {
	HWMCA_INITIALIZE_T session;
	ULONG timeout;
	_Bool verbose;
	char *server;
	ULONG ret;
};

/*
 *      function: call_HwmcaGet
 *
 *      purpose: retrieve data from the serer using HwmcaGet HWMCAAPI API.
 */
int call_HwmcaGet(struct sncap_api *api,
			char *snmp_object,
			char **buffer);

/*
 *      function: call_HwmcaCommand
 *
 *      purpose: calls the HwmcaCommand API for temporary capacity record
 *               activation/deactivation.
 */
int call_HwmcaCommand(struct sncap_api *api,
			char *command,
			char *cpc_snmp_id,
			char *xml);

/*
 *      function: sncap_get_cpc_attribute_id
 *
 *      purpose: create SNMP attribute identifier for given CPC SNMP
 *               identifier.
 */
void sncap_get_cpc_attribute_id(const char *attr_name,
				const char *cpc_id,
				const int attr_len,
				char *attr_id);

/*
 *      function: sncap_get_cpc_smmp_id
 *
 *      purpose: resolve the hardware CPC identifier to the CPC SNMP
 *               identifier.
 */
int sncap_get_cpc_snmp_id(struct sncap_api *api,
			const char *CPC_id,
			char **cpc_snmp_id);

/*
 *	function: sncap_list_cpcs
 *
 *	purpose: print the list of CPCs defined on the server to stdout.
 */
int sncap_list_cpcs(struct sncap_api *api);

/*
 *	function: sncap_get_tcr_xml
 *
 *	purpose: retrieve pseudo-XML definition for the temporary capacity
 *		 record
 */
int sncap_get_tcr_xml(struct sncap_api *api, const char *tcr_id, char **xml);

/*
 *      function: sncap_get_cpc_tcr_list
 *
 *      purpose: returns the array of pointers to the temporary capacity
 *               record objects installed on CPC with given SNMP ID.
 */
int sncap_get_cpc_tcr_list(struct sncap_api *api,
			const char *cpc_snmp_id,
			int *n_tcr,
			struct sncap_tcr **records);
/*
 *      function: sncap_api_release
 *
 *      purpose: releases memory used by the sncap_api object.
 */
void sncap_api_release(struct sncap_api *api);

/*
 *	function: sncap_connect
 *
 *	purpose: connect to the HWMCAAPI API server.
 */
int sncap_connect(struct sncap_connection *connection,
		_Bool verbose,
		struct sncap_api **api);

/*
 *	function: sncap_disconnect
 *
 *	purpose: terminate HWMCAAPI API session.
 */
int sncap_disconnect(struct sncap_api *api);

/*
 *      function: sncap_check_cpc_version
 *
 *      purpose: checks if the CPC SE software version is 10.2.0 or later.
 *
 */
int sncap_check_cpc_version(struct sncap_api *api, char *cpc_name);

/*
 *	function: sncap_get_se_version
 *
 *	purpose: returns the server software version string for the specified
 *		 sncap_api object.
 */
int sncap_get_se_version(struct sncap_api *api, char **version);

/*
 *	function: sncap_change_activation_level
 *
 *	purpose: activate/deactivate or change activation level of a temporary
 *		 capacity record on specified CPC.
 */
int sncap_change_activation_level(struct sncap_api *api,
			const struct sncap_job *job);

/*
 *	function: sncap_query_record_list
 *
 *	purpose: print the installed temporary capacity record list to the
 *		 stdout stream.
 */
int sncap_query_record_list(struct sncap_api *api, const char *CPC_id);

/*
 *      fuction: sncap_get_tcr
 *
 *      purpose: return temporary capacity record object containing the data
 *              retrieved from HWMCAAPI API server.
 */
int sncap_get_tcr(struct sncap_api *api,
		const char *cpc_snmp_id,
		const char *TCR_id,
		struct sncap_tcr *tcr);

/*
 *	function: sncap_query_record
 *
 *	purpose: print the temporary capacity record state attributes to the
 *		stdout stream.
 */
int sncap_query_record(struct sncap_api *api,
			const char *CPC_id,
			const char *TCR_id);

/*
 *	function: sncap_query_cpu
 *
 *	purpose: print the current CPC CPU configuration information to the
 *		stdout stream.
 */
int sncap_query_cpus(struct sncap_api *api, const char *cpc_id);
#endif
