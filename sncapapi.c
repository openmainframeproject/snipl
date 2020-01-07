/*
 * sncapapi.c : snCap API for the HWMCAAPI API access
 *
 * Copyright IBM Corp. 2012, 2016
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */
#include "sncapapi.h"
#include "sncapjob.h"

static const char *cpu_type[5] = {
	[CPU_TYPE_ICF] "ICF",
	[CPU_TYPE_IFL] "IFL",
	[CPU_TYPE_SAP] "SAP",
	[CPU_TYPE_ZAAP] "AAP",
	[CPU_TYPE_ZIIP] "IIP"
};

/*
 *	function: call_HwmcaGet
 *
 *	purpose: retrieve data from the server using HwmcaGet HWMCAAPI API.
 */
int call_HwmcaGet(struct sncap_api *api,
			char *snmp_object,
			char **buffer)
{
	int ret = 0;
	ULONG bytes_needed = 0ul;
	char *data_buffer = NULL;
	ULONG buffer_size = 0ul;

	data_buffer = calloc(sizeof(HWMCA_DATATYPE_T) + 4096,
				sizeof(*data_buffer));
	if (!data_buffer)
		return SNCAP_MEMORY_ALLOC_FAILURE;
	buffer_size = sizeof(HWMCA_DATATYPE_T) + 4096;

	do {
		api->ret = HwmcaGet(&api->session,
				snmp_object,
				data_buffer,
				buffer_size,
				&bytes_needed,
				api->timeout);

		if (api->ret != HWMCA_DE_NO_ERROR) {
			ret = sncap_print_api_message(api->verbose, api->server,
						      api->ret);
			goto cleanup;
		}
		APIDEBUG("HwmcaGet has been successfully called.\n");
		if (bytes_needed <= buffer_size)
			break;
		data_buffer = realloc(data_buffer, bytes_needed);
		if (!data_buffer) {
			ret = SNCAP_MEMORY_ALLOC_FAILURE;
			goto cleanup;
		}
		buffer_size = bytes_needed;
	} while (api->ret == HWMCA_DE_NO_ERROR);

	*buffer = calloc(((HWMCA_DATATYPE_T *)data_buffer)->ulLength,
			sizeof(**buffer));
	memcpy(*buffer,
		(const char *)((HWMCA_DATATYPE_T *)data_buffer)->pData,
		((HWMCA_DATATYPE_T *)data_buffer)->ulLength);
cleanup:
	free(data_buffer);
	return ret;
}

/*
 *	function: wait_response_event
 *
 *	purpose: waits for the HWMCA_EVENT_COMMAND_RESPONSE event and parses
 *		 its contents to get the command execution return code.
 */
static _Bool wait_response_event(struct sncap_api *api,
				const int event_id,
				int *ret)
{
	int i = 0;
	ULONG needed = 0;
	HWMCA_DATATYPE_T *ackn_data = NULL;
	_Bool continue_waiting = 0;
	char response_buffer[4096];

	memset(response_buffer, '\0', sizeof(response_buffer));
	api->ret = HwmcaWaitEvent(&api->session,
				response_buffer,
				strlen(response_buffer) - 1,
				&needed,
				api->timeout);
	if (api->ret != HWMCA_DE_NO_ERROR) {
		APIDEBUG("Error occurred during the aknowledgement "
			"receiving.\n");
		*ret = sncap_print_api_message(api->verbose, api->server,
					       api->ret);
		continue_waiting = 0;
		goto cleanup;
	}
	APIDEBUG("Command result acknowledgement has been received.\n");
	if (needed > sizeof(response_buffer)) {
		APIDEBUG("The acknowledgement size is too large.\n");
		continue_waiting = 1;
		goto cleanup;
	}
	/*
	 * Move to the command correlator value.
	 */
	ackn_data = (HWMCA_DATATYPE_T *)response_buffer;

	for (i = 0; i < RESPONSE_CORRELATOR_POSITION; i++) {
		ackn_data = ackn_data->pNext;
		if (!ackn_data) {
			APIDEBUG("Another application's event "
				"has been received (event is shorter).\n");
			break;
		}
	}
	if (!ackn_data) {
		continue_waiting = 1;
		goto cleanup;
	}
	if (ackn_data->ucType != HWMCA_TYPE_OCTETSTRING) {
		APIDEBUG("Could not find the command correlator.\n");
		*ret = SNCAP_INVALID_SE_RESPONSE;
		continue_waiting = 0;
		goto cleanup;
	}
	if (!ackn_data->pData) {
		APIDEBUG("Could not access the command correlator.\n");
		*ret = SNCAP_INVALID_SE_RESPONSE;
		continue_waiting = 0;
		goto cleanup;
	}
	if (ackn_data->ulLength != (ULONG)sizeof(event_id)) {
		APIDEBUG("Another application event has been received.\n");
		continue_waiting = 1;
		goto cleanup;
	}
	if (*(int *)ackn_data->pData != event_id) {
		APIDEBUG("Another application event has been received.\n");
		continue_waiting = 1;
		goto cleanup;
	}
	/*
	 * Move to the command event return code.
	 */
	ackn_data = (HWMCA_DATATYPE_T *)response_buffer;
	for (i = 0; i < RESPONSE_RC_POSITION; i++)
		ackn_data = ackn_data->pNext;
	/*
	 * Handle the return code.
	 */
	if (ackn_data->ucType != HWMCA_TYPE_INTEGER) {
		APIDEBUG("Could not parse the acknowledgement.\n");
		sncap_print_api_message(api->verbose, api->server, 2);
		*ret = SNCAP_INVALID_SE_RESPONSE;
		continue_waiting = 0;
		goto cleanup;
	}
	if (!ackn_data->pData) {
		APIDEBUG("Could not access the event return code.\n");
		*ret = SNCAP_INVALID_SE_RESPONSE;
		continue_waiting = 0;
		goto cleanup;
	}
	api->ret = *(ULONG *)ackn_data->pData;
	if (api->ret != 0) {
		if (sncap_is_api_return_code(api->ret))
			*ret = sncap_print_api_message(api->verbose,
						api->server,
						api->ret);
		else {
			APIDEBUG("Command was not successful, "
				"event rc is %lu.\n",
				api->ret);
			*ret = SNCAP_HWMCAAPI_ERROR;
		}
		continue_waiting = 0;
		goto cleanup;
	}
cleanup:
	return continue_waiting;
}

/*
 *	function: call_HwmcaCommand
 *
 *	purpose: calls the HwmcaCommand API for temporary capacity record
 *		 activation/deactivation.
 */
int call_HwmcaCommand(struct sncap_api *api,
			char *command,
			char *cpc_snmp_id,
			char *xml)
{
	HWMCA_DATATYPE_T parameter;
	int ret = 0;
	int pid = 0;
	int correlator = 0;
	long ltime = 0l;
	int stime = 0;

	assert(api);
	assert(command);
	assert(cpc_snmp_id);
	assert(xml);

	parameter.ucType = HWMCA_TYPE_OCTETSTRING;
	parameter.ulLength = strlen(xml);
	parameter.pData = xml;
	parameter.pNext = NULL;
	/*
	 * Generate the command correlator value.
	 */
	pid = getpid();
	ltime = time(NULL);
	stime = (unsigned)ltime / 2;
	srand(stime);
	correlator = rand() + pid;

	APIDEBUG("Sending the command to server '%s'.\n", api->server);
	api->ret = HwmcaCorrelatedCommand(&api->session,
				cpc_snmp_id,
				command,
				&parameter,
				api->timeout,
				&correlator,
				sizeof(correlator));
	if (api->ret != HWMCA_CMD_NO_ERROR) {
		APIDEBUG("Error occurred during command processing.\n");
		ret = sncap_print_api_message(api->verbose, api->server,
					      api->ret);
		goto cleanup;
	}
	APIDEBUG("The command has been sent.\n");
	APIDEBUG("Waiting for the command result information...\n");

	while (wait_response_event(api, correlator, &ret))
		;

	APIDEBUG("The command response event has been processed.\n");
cleanup:
	return ret;
}

/*
 *	function: sncap_get_cpc_attribute_id
 *
 *	purpose: create SNMP attribute identifier for given CPC SNMP
 *		 identifier.
 */
void sncap_get_cpc_attribute_id(const char *attr_name,
				const char *cpc_id,
				const int attr_len,
				char *attr_id)
{
	snprintf(attr_id,
		attr_len - 1,
		"%s.%s.%s",
		HWMCA_CFG_CPC_ID,
		attr_name,
		&cpc_id[strlen(HWMCA_CFG_CPC_ID) + 1]);
}

/*
 *	function: sncap_get_cpc_id_list
 *
 *	purpose: retrieves the CPC SNMP identifier list from the HWMCAAPI
 *		 API server.
 */
int sncap_get_cpc_id_list(struct sncap_api *api, char **cpc_id_list)
{
	int ret = SNCAP_OK;
	char  attr_id[81] = {'\0'};

	assert(api);

	/*
	 * Get the contents of the Defined CPC Group.
	 */
	snprintf(attr_id,
		sizeof(attr_id) - 1,
		"%s.%s",
		HWMCA_CFG_CPC_GROUP_ID,
		HWMCA_GROUP_CONTENTS_SUFFIX);
	APIDEBUG("The defined CPC group identifier is '%s'.\n", attr_id);

	ret = call_HwmcaGet(api, attr_id, cpc_id_list);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get the list of defined CPCs for "
			"server '%s'\n",
			api->server);
		return ret;
	}
	APIDEBUG("The following CPCs have been defined on server '%s':\n",
		api->server);
	APIDEBUG("'%s'.\n", *cpc_id_list);

	return SNCAP_OK;
}

/*
 *	function: sncap_get_cpc_attr_string
 *
 *	purpose: retrieve CPC attribute string value associated with the
 *		 CPC SNMP identifier.
 */
int sncap_get_cpc_attr_string(struct sncap_api *api,
			const char *snmp_id,
			const char *suffix_id,
			char **value)
{
	int ret = SNCAP_OK;
	char attr_id[81] = {'\0'};

	sncap_get_cpc_attribute_id(suffix_id,
				snmp_id,
				sizeof(attr_id),
				attr_id);
	ret = call_HwmcaGet(api, attr_id, value);
	if (ret != SNCAP_OK)
		APIDEBUG("Could not get the CPC attribute for "
			"SNMP ID '%s' on server '%s'\n",
			attr_id,
			api->server);
	return ret;
}

/*
 *	function: sncap_get_cpc_attr_int
 *
 *	purpose: retrieve integer value associated with the CPC SNMP identifier.
 */
int sncap_get_cpc_attr_int(struct sncap_api *api,
			const char *snmp_id,
			const char *suffix_id,
			int *value)
{
	int ret = SNCAP_OK;
	char *buffer = NULL;

	ret = sncap_get_cpc_attr_string(api, snmp_id, suffix_id, &buffer);

	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get attribute '%s' for SNMP object '%s'.\n",
			suffix_id,
			snmp_id);
		goto cleanup;
	}
	memcpy(value, buffer, sizeof(*value));
cleanup:
	free(buffer);
	return ret;
}

/*
 *	function: sncap_get_cpc_snmp_id
 *
 *	purpose: resolves the hardware CPC identifier to the CPC SNMP
 *		 identifier.
 */
int sncap_get_cpc_snmp_id(struct sncap_api *api,
			const char *CPC_id,
			char **cpc_snmp_id)
{
	int ret = SNCAP_OK;
	char *CPC_list = NULL;
	char *curr_id = NULL;
	char *cpc_name = NULL;

	assert(api);

	APIDEBUG("Getting the list of defined CPCs on server '%s' ...\n",
		api->server);
	ret = sncap_get_cpc_id_list(api, &CPC_list);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get the list of defined CPCs.\n");
		goto cleanup;
	}
	APIDEBUG("The list of defined CPCs has been retrieved.\n");
	/*
	 * Find the CPC with the given hardware identifier.
	 */
	curr_id = strtok(CPC_list, " ");
	while (curr_id != NULL) {
		APIDEBUG("Retrieving the CPC hardware identifier "
			"for CPC SNMP ID '%s'.\n",
			curr_id);
		ret = sncap_get_cpc_attr_string(api,
					curr_id,
					HWMCA_NAME_SUFFIX,
					&cpc_name);
		if (ret != SNCAP_OK) {
			APIDEBUG("Could not retrieve the CPC identifier.\n");
			goto cleanup;
		}
		APIDEBUG("The CPC identifier '%s' has been retrieved.\n",
			cpc_name);

		if (strcasecmp(cpc_name, CPC_id) == 0) {
			*cpc_snmp_id = strdup(curr_id);
			if (!*cpc_snmp_id) {
				ret = SNCAP_MEMORY_ALLOC_FAILURE;
				goto cleanup;
			}
			break;
		}
		free(cpc_name);
		cpc_name = NULL;
		curr_id = strtok(NULL, " ");
	}
	if (curr_id == NULL) {
		APIDEBUG("The CPC '%s' has not been defined on server '%s'.\n",
				CPC_id,
				api->server);
		ret = SNCAP_NO_CPC;
		goto cleanup;
	}
	APIDEBUG("CPC SNMP ID for the CPC '%s' is '%s'.\n",
		CPC_id,
		*cpc_snmp_id);
cleanup:
	free(CPC_list);
	free(cpc_name);

	return ret;
}

/*
 *      function: sncap_list_cpcs
 *
 *      purpose: print the list of CPCs defined on the server to stdout.
 */
int sncap_list_cpcs(struct sncap_api *api)
{
	int ret = SNCAP_OK;
	char *id_list = NULL;
	char *tmp_list = NULL;
	char *curr_id = NULL;
	int cpc_quantity = 0;
	char *cpc_id = NULL;
	char *cpc_version = NULL;
	int i = 0;

	assert(api);

	APIDEBUG("Retrieving the list of CPC SNMP identifiers "
		"for server '%s'...\n",
		api->server);
	ret = sncap_get_cpc_id_list(api, &id_list);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve CPC SNMP identifiers.\n");
		return ret;
	}
	APIDEBUG("The list of CPC SNMP identifiers has been retrieved.\n");
	/*
	 * Calculate the number of defined CPCs.
	 */
	tmp_list = strdup(id_list);
	if (!tmp_list) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	cpc_quantity = 0;
	curr_id = strtok(tmp_list, " ");
	while (curr_id != NULL) {
		cpc_quantity += 1;
		curr_id = strtok(NULL, " ");
	}
	APIDEBUG("%d CPC(s) has been defined on server '%s'.\n",
		cpc_quantity,
		api->server);
	free(tmp_list);
	/*
	 * Print CPC identifiers and versions.
	 */
	printf("\nThe following CPCs have been defined\n");
	printf("on the server '%s':\n\n", api->server);
	printf("CPC Identifier SE Version\n");
	printf("-------------- ----------\n");
	curr_id = strtok(id_list, " ");
	for (i = 0; i < cpc_quantity; i++) {
		APIDEBUG("Retrieving CPC hardware identitifer "
			"for CPC SNMP ID '%s'.\n",
			curr_id);
		ret = sncap_get_cpc_attr_string(api,
				curr_id,
				HWMCA_NAME_SUFFIX,
				&cpc_id);
		if (ret != SNCAP_OK) {
			APIDEBUG("Could not retrieve CPC hardware "
				"identifier.\n");
			goto cleanup;
		}
		APIDEBUG("The CPC hardware identifier "
			"'%s' has been retrieved.\n",
			cpc_id);

		APIDEBUG("Retrieving CPC version for CPC SNMP ID '%s'...\n",
			curr_id);
		ret = sncap_get_cpc_attr_string(api,
				curr_id,
				HWMCA_VERSION_SUFFIX,
				&cpc_version);
		if (ret != SNCAP_OK) {
			APIDEBUG("Could not retrieve CPC version.\n");
			goto cleanup;
		}
		APIDEBUG("The CPC version '%s' has been retrieved for ",
			cpc_version);
		APIDEBUG("CPC SNMP ID '%s'.\n",	curr_id);

		printf("%-14s %-10s\n", cpc_id, cpc_version);

		free(cpc_id);
		free(cpc_version);

		curr_id = strtok(NULL, " ");
	}
	printf("\n");
cleanup:
	free(id_list);
	return ret;
}

/*
 *	function: sncap_get_cpc_tcr_list
 *
 *	purpose: returns the array of pointers to the temporary capacity
 *		 record objects installed on CPC with given SNMP ID.
 */
int sncap_get_cpc_tcr_list(struct sncap_api *api,
			const char *cpc_snmp_id,
			int *n_tcr,
			struct sncap_tcr **records)
{
	int ret = SNCAP_OK;
	char *list_buffer = NULL;
	char *temp_buffer = NULL;
	int tcr_index = 0;
	char *c_tcr_id = NULL;
	char *xml = NULL;
	int i = 0;

	assert(api);

	ret = sncap_get_cpc_attr_string(api,
				cpc_snmp_id,
				HWMCA_CAPACITY_RECORD_LIST_SUFFIX,
				&list_buffer);
	if (ret != SNCAP_OK)
		return ret;

	temp_buffer = strdup(list_buffer);
	if (!temp_buffer) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	/*
	 * Calculate the number of the record identifiers in returned string.
	 */
	tcr_index = 0;
	c_tcr_id = strtok(temp_buffer, " ");
	while (c_tcr_id != NULL) {
		tcr_index += 1;
		c_tcr_id = strtok(NULL, " ");
	}
	*n_tcr = tcr_index;
	free(temp_buffer);
	/*
	 * Allocate the array of records
	 */
	*records = calloc(*n_tcr, sizeof(**records));
	if (!*records) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}

	tcr_index = 0;
	c_tcr_id = strtok(list_buffer, " ");
	while (c_tcr_id != NULL) {
		(*records)[tcr_index].snmp_id = strdup(c_tcr_id);
		if (!(*records)[tcr_index].snmp_id) {
			ret = SNCAP_MEMORY_ALLOC_FAILURE;
			goto cleanup;
		}
		c_tcr_id = strtok(NULL, " ");
		tcr_index += 1;
	}

	for (i = 0; i < *n_tcr; i++) {
		APIDEBUG("Retrieving definition XML for TCR: '%s'\n",
			(*records)[i].snmp_id);
		ret = sncap_get_tcr_xml(api, (*records)[i].snmp_id , &xml);
		if (ret != SNCAP_OK) {
			APIDEBUG("Could not retrieve definition XML for TCR: "
				"'%s'\n",
				(*records)[i].snmp_id);
			goto cleanup;
		}
		APIDEBUG("XML for TCR '%s' has been retrieved.\n",
			(*records)[i].snmp_id);

		APIDEBUG("Parsing XML for TCR '%s'\n", (*records)[i].snmp_id);

		ret = sncap_tcr_get_data(xml, &(*records)[i]);
		if (ret != SNCAP_OK) {
			APIDEBUG("Could not parse XML for TCR '%s'.\n",
				(*records)[i].snmp_id);
			goto cleanup;
		}
		free(xml);
		xml = NULL;
		APIDEBUG("XML for TCR '%s' has been parsed.\n",
			(*records)[i].snmp_id);
	}

cleanup:
	free(list_buffer);
	free(xml);

	if (ret != SNCAP_OK)
		if (*records) {
			sncap_tcr_list_release(*n_tcr, *records);
			*records = NULL;
		}
	return ret;
}

/*
 * The PU name - SNMP suffix association table.
 */
static const struct sncap_pu_type_suffix {
	const char *type;
	const char *suffix;
} type_suffix_tab[6] = {
	{"CP", HWMCA_GEN_PROCESSOR_NUM_SUFFIX},
	{"ICF", HWMCA_ICF_PROCESSOR_NUM_SUFFIX},
	{"IFL", HWMCA_IFL_PROCESSOR_NUM_SUFFIX},
	{"SAP", HWMCA_SAP_PROCESSOR_NUM_SUFFIX},
	{"AAP", HWMCA_IFA_PROCESSOR_NUM_SUFFIX},
	{"IIP", HWMCA_IIP_PROCESSOR_NUM_SUFFIX}
};

/*
 *	function: sncap_get_cpc
 *
 *	purpose: retrieves the CPC object attribute values from HWMCAAPI API.
 */
int sncap_get_cpc(struct sncap_api *api,
		const char *cpc_id,
		struct sncap_cpc *cpc)
{
	int ret = SNCAP_OK;
	struct sncap_cpc_pu *pu = NULL;
	int i = 0;

	APIDEBUG("Retrieving the CPC attributes for CPC '%s'...\n",
		cpc_id);
	cpc->id = strdup(cpc_id);
	if (cpc->id == NULL) {
		APIDEBUG("Unable to copy CPC ID.");
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	APIDEBUG("Retrieving the SNMP ID...\n");
	ret = sncap_get_cpc_snmp_id(api, cpc->id, &cpc->snmp_id);
	if (ret != SNCAP_OK)
		goto cleanup;
	APIDEBUG("SNMP ID '%s' for CPC '%s' has been retrieved.\n",
		cpc->snmp_id,
		cpc->id);

	for (i = 0; i < 6; i++) {
		APIDEBUG("Retrieving the number of permanent '%s' PU...\n",
			type_suffix_tab[i].type);
		pu = sncap_cpc_find_pu(cpc->pu_total,
				type_suffix_tab[i].type);
		if (pu) {
			ret = sncap_get_cpc_attr_int(api,
					cpc->snmp_id,
					type_suffix_tab[i].suffix,
					&pu->quantity);
			if (ret != SNCAP_OK) {
				APIDEBUG("Could not get the permanent '%s' ",
					pu->type);
				APIDEBUG("PU quantity.\n");
				goto cleanup;
			}
			APIDEBUG("%d %s permanent PU exist.\n",
				pu->quantity,
				pu->type);
		}
	}
	APIDEBUG("Retrieving the PU available for temporary activation ");
	APIDEBUG("quantity...\n");
	ret = sncap_get_cpc_attr_int(api,
				cpc->snmp_id,
				HWMCA_SPARE_PROCESSOR_NUM_SUFFIX,
				&cpc->pu_available);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get the number fo PU available for ");
		APIDEBUG("temporary activation for CPC '%s'.\n", cpc->id);
		goto cleanup;
	}
	APIDEBUG("The number of PUs available for temporary activation ");
	APIDEBUG("is %d.\n", cpc->pu_available);

	APIDEBUG("Retrieving the current MSU value...\n");
	ret = sncap_get_cpc_attr_int(api,
				cpc->snmp_id,
				HWMCA_PERMALL_MSU_SUFFIX,
				&cpc->msu);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get the current MSU value ");
		goto cleanup;
	}
	APIDEBUG("The current MSU value is %d.\n", cpc->msu);

	APIDEBUG("Retrieving the temporaty capacity model identifier...\n");
	ret = sncap_get_cpc_attr_string(api,
				cpc->snmp_id,
				HWMCA_PERMBILL_SOFTWARE_MODEL_SUFFIX,
				&cpc->tmp_capacity_model);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve the temporary capacity model ");
		APIDEBUG("identifier.\n");
		goto cleanup;
	}
	APIDEBUG("Retrieving the current capacity model identifier...\n");
	ret = sncap_get_cpc_attr_string(api,
				cpc->snmp_id,
				HWMCA_PERMALL_SOFTWARE_MODEL_SUFFIX,
				&cpc->current_capacity_model);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve the current capacity model.\n");
		goto cleanup;
	}
	APIDEBUG("The current capacity model identifier is '%s'.\n",
		cpc->current_capacity_model);

	APIDEBUG("Retrieving the permanent capacity model identifier...\n");
	ret = sncap_get_cpc_attr_string(api,
				cpc->snmp_id,
				HWMCA_PERM_SOFTWARE_MODEL_SUFFIX,
				&cpc->permanent_capacity_model);
	APIDEBUG("The permanent capacity model identifier '%s' ",
		cpc->permanent_capacity_model);
	APIDEBUG("has been retrieved.\n");
cleanup:
	if (ret == SNCAP_OK) {
		APIDEBUG("The CPC attributes for CPC '%s' have been ", cpc->id);
		APIDEBUG("successfully retrieved.\n");
	} else
		APIDEBUG("Could not get the CPC attributes for CPC '%s'.\n",
			cpc->id);
	return ret;
}

/*
 *      fucntion: connection_print
 *
 *      purpose: print connection atribute values.
 */
void connection_print(const struct sncap_connection *connection)
{
	if (connection->address)
		printf("Server address.: '%s'\n", connection->address);
	else
		printf("Server address has not been specified.\n");

	if (connection->password)
		printf("Server password: '%s'\n", SNCAP_PASSWORD_MASK);
	else
		printf("Server password has not been specified.\n");

	if (connection->username)
		printf("Server userid: '%s'\n", connection->username);
	else
		printf("Server userid has not been specified.\n");

	if (connection->encryption)
		printf("Server encryption is on.\n");
	else
		printf("Server encryption is off.\n");

	if (connection->timeout)
		printf("Timeout........: %d\n",
			connection->timeout);
	else
		printf("API timeout has not been specified.\n");
}

/*
 *      function: connection_release
 *
 *      purpose: releases memory used by connection data.
 */
void connection_release(struct sncap_connection *connection)
{
	free(connection->address);
	connection->address = NULL;

	free(connection->password);
	connection->password = NULL;

	free(connection->username);
	connection->username = NULL;
}

/*
 *	function: sncap_api_release
 *
 *	purpose: releases memory used by the sncap_api object.
 */
void sncap_api_release(struct sncap_api *api)
{
	HWMCA_SNMP_TARGET_T *p = NULL;

	assert(api);

	p = (HWMCA_SNMP_TARGET_T *)api->session.pTarget;
	if (p) {
		free(p->pHost);
		free(p);
	}
	free(api->server);
}

/*
 *	function: sncap_connect
 *
 *	purpose: connect to the HWMCAAPI API server.
 */
int sncap_connect(struct sncap_connection *connection,
	const _Bool verbose,
	struct sncap_api **api)
{
	int ret = SNCAP_OK;
	HWMCA_SNMP_TARGET_T *snmp_target = NULL;
	int id_item_len;

	APIDEBUGV("sncap_connect: function entered to connect to server.\n");
	APIDEBUGV("Specified parameters:\nRunning in Verbose mode;\n");
	if (verbose)
		connection_print(connection);
	APIDEBUGV("*** End of sncap_connect Parameters ***\n");
	/*
	 * Allocate and fill the HWMCA_INITIALIZAT_T object.
	 */
	*api = calloc(1, sizeof(struct sncap_api));
	if (!*api) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	snmp_target = calloc(1, sizeof(*snmp_target));
	if (!snmp_target) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	(*api)->session.pTarget = snmp_target;

	snmp_target->pHost = strdup(connection->address);
	strncpy(snmp_target->szCommunity,
		connection->password,
		sizeof(snmp_target->szCommunity) - 1);
	(*api)->timeout = (ULONG) connection->timeout;
	if (connection->encryption) {
		snmp_target->ulSecurityVersion = HWMCA_SECURITY_VERSION3;

		id_item_len = HWMCA_MAX_USERNAME_LEN - 1;
		strncpy(snmp_target->szUsername, connection->username,
			id_item_len);
		strncpy(snmp_target->szPassword, connection->password,
			id_item_len);
	}

	if (connection->timeout < 0) {
		(*api)->timeout = 60000;
		APIDEBUGV("Default timeout of %lums is used.\n",
			(*api)->timeout);
	}
	(*api)->verbose = verbose;
	(*api)->server = strdup(connection->address);
	/*
	 * Try to connect to the Support Element.
	 */
	(*api)->session.ulEventMask = HWMCA_EVENT_COMMAND_RESPONSE +
		HWMCA_DIRECT_INITIALIZE +
		HWMCA_SNMP_VERSION_2;

	APIDEBUGV("sncap_connect: creating the connection to %s ...\n",
		(*api)->server);

	(*api)->ret = HwmcaInitialize(&(*api)->session, (*api)->timeout);
	if ((*api)->ret != HWMCA_DE_NO_ERROR) {
		ret = sncap_print_api_message((*api)->verbose,
					      snmp_target->pHost, (*api)->ret);
		goto cleanup;
	}
	/*
	 * Complete the processing and free unnecessary resources.
	 */
cleanup:
	if (ret != SNCAP_OK) {
		APIDEBUGV("sncap_connect: "
			"connection to server has not been created.\n");
		if (*api != NULL) {
			sncap_api_release(*api);
			free(*api);
			}
		*api = NULL;
	} else {
		APIDEBUGV("sncap_connect: a connection "
			"to '%s' has been successfully created.\n",
			(*api)->server);
	}
	return ret;
}

/*
 *	function: sncap_disconnect
 *
 *	purpose: terminate HWMCAAPI API session.
 */
int sncap_disconnect(struct sncap_api *api)
{
	int ret = 0;
	assert(api);
	/*
	 * Terminate the HWMCAAPI session.
	 */
	api->ret = HwmcaTerminate(&api->session, api->timeout);
	if (api->ret != HWMCA_DE_NO_ERROR) {
		ret = sncap_print_api_message(api->verbose, api->server,
					      api->ret);
		APIDEBUG("sncap_disconnect: "
			"Unable to terminate the HWMCAAPI API session.\n");
		return ret;
	}
	APIDEBUG("sncap_disconnect: "
		"HWMCAAPI API session has been successfully terminated.\n");
	/*
	 * Release the resources used by the HWMCAAPI API session.
	 */
	sncap_api_release(api);
	free(api);

	return SNCAP_OK;
}

/*
 *	function: sncap_check_cpc_version
 *
 *	purpose: checks if the CPC SE software version is 10.2.0 or later.
 *
 */
int sncap_check_cpc_version(struct sncap_api *api, char *cpc_name)
{
	int ret = SNCAP_OK;
	char *snmp_id = NULL;
	char *version = NULL;

	assert(api);
	assert(cpc_name);

	APIDEBUG("Retrieving the SNMP ID for the CPC '%s'.\n",
		cpc_name);

	ret = sncap_get_cpc_snmp_id(api, cpc_name, &snmp_id);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve the SNMP ID.\n");
		goto cleanup;
	}
	APIDEBUG("SNMP ID for the CPC '%s' is '%s'.\n", cpc_name, snmp_id);

	APIDEBUG("Retrieving the SE software version for the CPC '%s'.\n",
		cpc_name);

	ret = sncap_get_cpc_attr_string(api,
				snmp_id,
				HWMCA_VERSION_SUFFIX,
				&version);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve SE version for the CPC '%s'.\n",
			snmp_id);
		goto cleanup;
	}
	APIDEBUG("The SE version for the CPC '%s' is '%s'.\n",
		cpc_name,
		version);

	if (sncap_compare_versions(version, "2.10.0") < 0) {
		APIDEBUG("The SE version '%s' for the CPC '%s' is invalid.\n",
			version,
			cpc_name);
		ret = SNCAP_INVALID_SE_VERSION;
		goto cleanup;
	}
	APIDEBUG("The SE version '%s' for the CPC '%s' is valid.\n",
		version,
		cpc_name);
cleanup:
	free(snmp_id);
	free(version);

	return ret;
}

/*
 *      function: sncap_get_se_version
 *
 *      purpose: returns the server software version string for the specified
 *               sncap_api object.
 */
int sncap_get_se_version(struct sncap_api *api, char **version)
{
	char snmp_attr_id[41] = {'\0'};
	int ret = SNCAP_OK;

	assert(api);

	snprintf(snmp_attr_id,
		sizeof(snmp_attr_id) - 1,
		"%s.%s",
		HWMCA_CONSOLE_ID,
		HWMCA_VERSION_SUFFIX);

	ret = call_HwmcaGet(api, snmp_attr_id, version);
	if (ret != HWMCA_DE_NO_ERROR)
		return ret;

	APIDEBUG("The server '%s' has software version '%s'.\n",
		api->server,
		*version);

	return SNCAP_OK;
}

/*
 *	function: sncap_change_activation_level
 *
 *	purpose: activate/deactivate or change activation level of a temporary
 *		 capacity record on specified CPC.
 */
int sncap_change_activation_level(struct sncap_api *api,
				const struct sncap_job *job)
{
	int ret = 0;
	int i = 0;
	int req_cpu = 0;
	char *xml = NULL;
	char *mode_word = NULL;
	const char *req_software_model = NULL;
	struct sncap_tcr_target *target = NULL;
	struct sncap_tcr tcr;
	char *cpc_snmp_id = NULL;
	struct sncap_tcr_procinfo *curr_cpu = NULL;
	int actual_cpu_quantity[5] = { 0, 0, 0, 0, 0 };

	memset(&tcr, '\0', sizeof(tcr));

	assert(api);
	assert(job);

	if (job->operation != ACTIVATE && job->operation != DEACTIVATE) {
		APIDEBUG("Invalid operation in the job structure.\n")
		return SNCAP_PROGRAM_ERROR;
	}
	mode_word = (job->operation == ACTIVATE)
			? "Activating" : "Deactivating";
	APIDEBUG("%s the temporary capacity record '%s' on CPC '%s'\n",
		mode_word,
		job->TCR_id,
		job->CPC_id);
	APIDEBUG("Retrieving the record infromation...\n");

	APIDEBUG("Retrieving the SNMP ID...\n");
	ret = sncap_get_cpc_snmp_id(api, job->CPC_id, &cpc_snmp_id);
	if (ret != SNCAP_OK)
		goto cleanup;
	APIDEBUG("SNMP ID '%s' for CPC '%s' has been retrieved.\n",
		cpc_snmp_id,
		job->CPC_id);

	ret = sncap_get_tcr(api, cpc_snmp_id, job->TCR_id, &tcr);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get record information from the server.\n");
		goto cleanup;
	}
	APIDEBUG("The record information has been retrieved.\n");
	/*
	 * Check if the requested operation can be performed.
	 *
	 * The record can be deactivated (CPU capacity can be removed) only
	 * if the record has the Real or Test activation status.
	 */
	if (job->operation == DEACTIVATE && !TCR_IS_ACTIVE((&tcr))) {
		ret = SNCAP_RECORD_IS_NOT_ACTIVE;
		goto cleanup;
	}
	/*
	 * Check if the number of activation attempts was not exhausted.
	 */
	if (job->operation == ACTIVATE) {
		if (job->operation_mode & SNCAP_OP_MODE_TEST) {
			if (!tcr.test.remaining_activations) {
				ret = SNCAP_ACTIVATIONS_EXHAUSTED;
				goto cleanup;
			}
		} else
			if (!tcr.real.remaining_activations) {
				ret = SNCAP_ACTIVATIONS_EXHAUSTED;
				goto cleanup;
			}
	}
	/*
	 * Check the command line activation parameters.
	 */
	req_software_model = job->software_model;
	if (req_software_model) {
		target = sncap_tcr_find_software_model(&tcr,
					req_software_model);
		if (!target) {
			ret = SNCAP_INVALID_SOFTWARE_MODEL;
			goto cleanup;
		}
		APIDEBUG("The software model '%s' is valid.\n",
				req_software_model);
	}
	for (i = 0; i < 5; i++) {
		if (job->cpu_quantity[i] == 0)
			continue;
		curr_cpu = sncap_tcr_find_cpu(&tcr, cpu_type[i]);
		if (!curr_cpu) {
			APIDEBUG("CPU type '%s' has not been defined in ",
				 cpu_type[i]);
			APIDEBUG("the TCR '%s'.\n", job->TCR_id);
			ret = SNCAP_UNDEFINED_CPU_TYPE;
			goto cleanup;
		}
		if (job->operation == ACTIVATE) {
			req_cpu = curr_cpu->proc_step + job->cpu_quantity[i];
			if (req_cpu > curr_cpu->proc_max) {
				APIDEBUG("The number of CPU to be "
					"activated exceeds the "
					"maximum allowed number of CPUs %d",
					curr_cpu->proc_max);
				APIDEBUG(" for CPU type '%s'.\n", cpu_type[i]);
				ret = SNCAP_TOO_MANY_CPU;
				goto cleanup;
			}
		} else {
			req_cpu = curr_cpu->proc_step - job->cpu_quantity[i];
			if (req_cpu < 0) {
				APIDEBUG("The number of CPU to be "
					"deactivated  exceeds "
					"the number of active CPUs %d",
					curr_cpu->proc_step);
				APIDEBUG(" for CPU type '%s'.\n", cpu_type[i]);
				ret = SNCAP_TOO_MANY_CPU;
				goto cleanup;
			}
		}
	}
	APIDEBUG("The specified CPU quantities are valid.\n");
	/*
	 * Calculate the quantities of CPU to be activated.
	 */
	req_cpu = 0;
	for (i = 0; i < 5; i++)
		if (job->cpu_quantity[i] > 0)
			req_cpu += 1;

	if (req_cpu)
		for (i = 0; i < 5; i++) {
			if (job->cpu_quantity[i] > 0) {
				curr_cpu = sncap_tcr_find_cpu(&tcr,
							cpu_type[i]);
				if (!curr_cpu) {
					APIDEBUG("CPU type check error!\n");
					ret = SNCAP_PROGRAM_ERROR;
					goto cleanup;
				}
				actual_cpu_quantity[i] = job->cpu_quantity[i];
			}
		}
	/*
	 * Don't allow activation of software model (model capacity identifier)
	 * which has the MSU Delta value lower or equal to the current one to
	 * avoid the temporary capacity record activation by a user mistake.
	 */
	APIDEBUG("Validating the requested target MSU Delta value...\n");
	if (req_software_model &&
	   !req_cpu &&
	   job->operation == ACTIVATE &&
	   target->msu_delta <= 0) {
		APIDEBUG("MSU Delta of the target being activated <= 0. "
			"Model capacity identifier '%s' is invalid.\n",
				req_software_model);
			ret = SNCAP_INVALID_SOFTWARE_MODEL;
			goto cleanup;
	}
	APIDEBUG("The requested target MSU Delta value is valid.\n");
	/*
	 * If neither CPC software model nor CPU types have been specified
	 * try deacivate the whole record using the software model with the
	 * minimum value of MSU Delta attribute available in the TCR being
	 * deactivated.
	 */
	if ((!req_software_model && !req_cpu) && job->operation != DEACTIVATE) {
		ret = SNCAP_MISSING_MANDATORY_OPTION;
		goto cleanup;
	}

	if ((!req_software_model && !req_cpu) && job->operation == DEACTIVATE) {
		for (i = 0; i < 5; i++) {
			curr_cpu = sncap_tcr_find_cpu(&tcr, cpu_type[i]);
			if (curr_cpu)
				actual_cpu_quantity[i] = curr_cpu->proc_step;
			else
				actual_cpu_quantity[i] = 0;
		}
		req_software_model = sncap_tcr_find_min_model(&tcr);
		if (!req_software_model) {
			APIDEBUG("Could not get the minimal capacity model.\n");
			ret = SNCAP_PROGRAM_ERROR;
			goto cleanup;
		}
	}
	mode_word = (job->operation == ACTIVATE) ? "activated" : "deactivated";
	if (api->verbose) {
		if (req_software_model) {
			APIDEBUG("The following capacity model id. will be set "
				"'%s'.\n",
				req_software_model);
		}
		APIDEBUG("The following auxiliary CPU quantities will be %s:\n",
			mode_word);
		for (i = 0; i < 5; i++)
			APIDEBUG("%s - %d\n",
				cpu_type[i],
				actual_cpu_quantity[i]);
	}
	mode_word = (job->operation == ACTIVATE)
			? "activation" : "deactivation";
	APIDEBUG("Generating XML for the TCR %s.\n", mode_word);

	ret = job_generate_xml(job,
			req_software_model,
			actual_cpu_quantity,
			&xml);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not generate the XML.\n");
		goto cleanup;
	}
	APIDEBUG("XML generation has been completed.\n");

	mode_word = (job->operation == ACTIVATE)
		? "Activating" : "Deactivating";
	APIDEBUG("%s the record...\n", mode_word);
	if (job->operation == ACTIVATE)
		mode_word = HWMCA_ADD_CAPACITY_COMMAND;
	else
		mode_word = HWMCA_REMOVE_CAPACITY_COMMAND;
	if (!(job->operation_mode & SNCAP_OP_MODE_DRYRUN))
		ret = call_HwmcaCommand(api, mode_word, cpc_snmp_id, xml);
	else {
		ret = SNCAP_OK;
		APIDEBUG("The NO RECORD CHANGES mode is active. "
			"The SNMP command has not been executed.\n");
	}
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not process the record.\n");
		goto cleanup;
	}
	mode_word = (job->operation == ACTIVATE)
			? "activated" : "deactivated";
	APIDEBUG("TCR '%s' has been %s on CPC '%s'\n",
			job->TCR_id,
			mode_word,
			job->CPC_id);

	APIDEBUG("Querying the record infromation...\n");
	ret = sncap_query_record(api, job->CPC_id, job->TCR_id);
	APIDEBUG("The record information has been retrieved.\n");
cleanup:
	free(xml);
	free(cpc_snmp_id);
	sncap_tcr_release(&tcr);

	return ret;
}

/*
 *      function: sncap_get_tcr_xml
 *
 *      purpose: retrieve pseudo-XML definition for the temporary capacity
 *               record
 */
int sncap_get_tcr_xml(struct sncap_api *api, const char *tcr_id, char **xml)
{
	char attr_id[81] = {'\0'};
	char *xml_buffer = NULL;
	int ret = SNCAP_OK;

	assert(api);

	snprintf(attr_id,
		sizeof(attr_id) - 1,
		"%s.%s",
		HWMCA_CAPACITY_RECORD_OBJECT_ID,
		&tcr_id[strlen(HWMCA_CAPACITY_RECORD_OBJECT_ID) + 1]);

	ret = call_HwmcaGet(api, attr_id, &xml_buffer);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get XML for record '%s'.\n", attr_id);
		goto cleanup;
	}
	*xml = strdup(xml_buffer);
	if (!*xml) {
		ret = SNCAP_MEMORY_ALLOC_FAILURE;
		goto cleanup;
	}
	APIDEBUG("The following XML has been received for TCR '%s'\n", tcr_id);
	APIDEBUG("%s\n", *xml);

cleanup:
	free(xml_buffer);
	return ret;
}

/*
 *	function: sncap_query_record_list
 *
 *	purpose: print the installed temporary capacity record list to the
 *		 stdout stream.
 */
int sncap_query_record_list(struct sncap_api *api, const char *CPC_id)
{
	int ret = SNCAP_OK;
	char *cpc_snmp_id = NULL;
	struct sncap_tcr *records = NULL;
	int n_tcr = 0;
	int i = 0;

	assert(api);

	APIDEBUG("Printing Temporary Capacity Record list "
		"for CPC '%s'...\n",
		CPC_id);

	ret = sncap_get_cpc_snmp_id(api, CPC_id, &cpc_snmp_id);
	if (ret != SNCAP_OK)
		goto cleanup;

	ret = sncap_get_cpc_tcr_list(api, cpc_snmp_id, &n_tcr, &records);
	if (ret != SNCAP_OK)
		goto cleanup;

	printf("\nCPC %s installed temporary capacity records\n",
		CPC_id);
	printf("\n");
	printf("RecordID Record Type Status    Real Act. Test Act. ");
	printf("Record Expiration\n");
	printf("-------- ----------- --------- --------- --------- ");
	printf("-------------------\n");

	for (i = 0; i < n_tcr; i++)
		sncap_tcr_print_list_item(&records[i]);
	printf("\n");

	APIDEBUG("Temporary Capacity Record list for CPC %s(%s) ",
		CPC_id,
		cpc_snmp_id);
	APIDEBUG(" has been printed.\n");
cleanup:
	free(cpc_snmp_id);
	if (records)
		sncap_tcr_list_release(n_tcr, records);

	return ret;
}

/*
 *	function: sncap_get_tcr_name
 *
 *	purpose: retrieve the temporary capacity record purchasing indentifier
 *		 (name) from the server.
 */
int sncap_get_tcr_name(struct sncap_api *api,
			const char *tcr_id,
			char **tcr_name)
{
	int ret = 0;
	char attr_id[81] = {'\0'};

	snprintf(attr_id,
		sizeof(attr_id) - 1,
		"%s.%s.%s",
		HWMCA_CAPACITY_RECORD_OBJECT_ID,
		HWMCA_RECORD_ID_SUFFIX,
		&tcr_id[strlen(HWMCA_CAPACITY_RECORD_OBJECT_ID) + 1]);

	ret = call_HwmcaGet(api, attr_id, tcr_name);

	return ret;
}

/*
 *	fuction: sncap_get_tcr
 *
 *	purpose: return temporary capacity record object containing the data
 *		retrieved from HWMCAAPI API server.
 */
int sncap_get_tcr(struct sncap_api *api,
		const char *cpc_snmp_id,
		const char *TCR_id,
		struct sncap_tcr *tcr)
{
	int ret = SNCAP_OK;
	char *tcr_ids = NULL;
	char *curr_id = NULL;
	char *tcr_name = NULL;
	char *tcr_xml = NULL;

	assert(api);
	assert(cpc_snmp_id);
	assert(TCR_id);
	assert(tcr);

	APIDEBUG("Retrieving the list of TCR SNMP IDs.\n");
	ret = sncap_get_cpc_attr_string(api,
				cpc_snmp_id,
				HWMCA_CAPACITY_RECORD_LIST_SUFFIX,
				&tcr_ids);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve TCR SNMP IDs.\n");
		goto cleanup;
	}
	APIDEBUG("The TCR ID list is: '%s'.\n", tcr_ids);
	APIDEBUG("TCR ID list has been retrieved.\n");

	curr_id = strtok(tcr_ids, " ");
	while (curr_id != NULL) {
		APIDEBUG("Retrieving TCR name...\n");
		ret = sncap_get_tcr_name(api, curr_id, &tcr_name);
		if (ret != SNCAP_OK) {
			APIDEBUG("Could not retrieve TCR name.\n");
			goto cleanup;
		}
		APIDEBUG("TCR name has been retrieved.\n");
		if (strcasecmp(TCR_id, tcr_name) == 0)
			break;
		free(tcr_name);
		curr_id = strtok(NULL, " ");
	}
	if (curr_id == NULL) {
		APIDEBUG("TCR '%s' has not been installed on CPC.\n", TCR_id);
		ret = SNCAP_NO_RECORD;
		goto cleanup;
	}
	tcr->snmp_id = strdup(curr_id);
	free(tcr_name);

	APIDEBUG("Retrieving XML for the TCR '%s' ('%s').\n", TCR_id, curr_id);
	ret = sncap_get_tcr_xml(api, tcr->snmp_id, &tcr_xml);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve XML for record '%s' ('%s').\n",
			TCR_id,
			curr_id);
		goto cleanup;
	}
	APIDEBUG("XML has been retrieved.\n");

	APIDEBUG("Parsing XML for the TCR '%s' ('%s').\n", TCR_id, curr_id);
	ret = sncap_tcr_get_data(tcr_xml, tcr);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not parse the XML.\n");
		goto cleanup;
	}
	APIDEBUG("XML has been parsed.\n");
cleanup:
	free(tcr_ids);
	free(tcr_xml);
	return ret;
}

/*
 *	function: sncap_query_record
 *
 *	purpose: print the temporary capacity record state attributes to the
 *		stdout stream.
 */
int sncap_query_record(struct sncap_api *api,
		const char *CPC_id,
		const char *TCR_id)
{
	int ret = SNCAP_OK;
	struct sncap_tcr tcr;
	char *cpc_snmp_id = NULL;
	char *model_capacity_id = NULL;
	int msu = 0;

	memset(&tcr, '\0', sizeof(tcr));

	assert(api);

	APIDEBUG("Retrieving the CPC SNMP ID for the CPC '%s'...\n", CPC_id);
	ret = sncap_get_cpc_snmp_id(api, CPC_id, &cpc_snmp_id);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get SNMP ID for the CPC '%s'.\n", CPC_id);
		goto cleanup;
	}
	APIDEBUG("The SNMP ID '%s' for CPC '%s' has been retrieved.\n",
		cpc_snmp_id,
		CPC_id);

	APIDEBUG("Retrieving the TCR '%s' of the CPC '%s'...\n",
		TCR_id,
		CPC_id);
	ret = sncap_get_tcr(api, cpc_snmp_id, TCR_id, &tcr);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve data for TCR '%s' on CPC '%s'.\n",
			TCR_id,
			CPC_id);
		goto cleanup;
	}
	APIDEBUG("TCR '%s' of CPC '%s' data has been retrieved.\n",
		TCR_id,
		CPC_id);

	APIDEBUG("Retrieving the current MSU value for CPC '%s'...\n",
		CPC_id);
	ret = sncap_get_cpc_attr_int(api,
				cpc_snmp_id,
				HWMCA_PERMALL_MSU_SUFFIX,
				&msu);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get the current MSU value.\n");
		goto cleanup;
	}
	APIDEBUG("The current MSU value is %d.\n", msu);

	APIDEBUG("Retrieving the current model capacity identifier ");
	APIDEBUG("for CPC '%s'.\n", CPC_id);

	ret = sncap_get_cpc_attr_string(api,
				cpc_snmp_id,
				HWMCA_PERMALL_SOFTWARE_MODEL_SUFFIX,
				&model_capacity_id);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not get current model capacity identifier.\n");
		goto cleanup;
	}
	APIDEBUG("Model capacity for CPC '%s' is '%s'.\n",
		CPC_id,
		model_capacity_id);

	sncap_tcr_print_attributes(&tcr, CPC_id, msu, model_capacity_id);
	printf("\n");
cleanup:
	free(cpc_snmp_id);
	free(model_capacity_id);
	sncap_tcr_release(&tcr);
	return ret;
}

/*
 *      function: sncap_query_cpus
 *
 *      purpose: print the current CPC CPU configuration information to the
 *              stdout stream.
 */
int sncap_query_cpus(struct sncap_api *api, const char *cpc_id)
{
	struct sncap_cpc cpc;
	struct sncap_tcr *records = NULL;
	int max_speed_step = 0;
	int n_active_tcrs = 0;
	struct sncap_tcr **active_tcrs = NULL;
	const struct sncap_tcr_procinfo *cp_pi = NULL;
	int ret = SNCAP_OK;
	int n_tcr = 0;
	int i = 0;
	int j = 0;

	assert(api);
	assert(cpc_id);

	sncap_cpc_init(&cpc);
	/*
	 * Get CPC configuration attributes.
	 */
	APIDEBUG("Retrieving the CPC object '%s'...\n", cpc_id);
	ret = sncap_get_cpc(api, cpc_id, &cpc);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve CPC object '%s'.\n", cpc_id);
		goto cleanup;
	}
	APIDEBUG("CPC object '%s' has been retrieved.\n", cpc_id);
	/*
	 * Calculate quantities of active CPUs defined by the temporary
	 * capacity records.
	 */
	APIDEBUG("Retrieving the TCR list for CPC '%s'\n", cpc.id);
	ret = sncap_get_cpc_tcr_list(api, cpc.snmp_id, &n_tcr, &records);
	if (ret != SNCAP_OK) {
		APIDEBUG("Could not retrieve TCR list for CPC object '%s'.\n",
			cpc.id);
		goto cleanup;
	}
	APIDEBUG("The TCR list for the CPC '%s' has been retrieved.\n",
			cpc.id)

	APIDEBUG("Calculating quantities of temporary active resources.\n");
	n_active_tcrs = 0;
	for (i = 0; i < n_tcr; i++)
		if (TCR_IS_ACTIVE((&records[i])))
			n_active_tcrs += 1;
	APIDEBUG("%d temporary capacity records are in active state.\n",
			n_active_tcrs);
	if (n_active_tcrs > 0) {
		active_tcrs = calloc(n_active_tcrs, sizeof(*active_tcrs));
		j = 0;
		for (i = 0; i < n_tcr; i++)
			if (TCR_IS_ACTIVE((&records[i])))
				active_tcrs[j++] = &records[i];

		max_speed_step = 0;
		cp_pi = sncap_tcr_find_cpu(active_tcrs[0], "CP");
		if (cp_pi)
			max_speed_step = cp_pi->speed_step;
		for (i = 0; i < n_active_tcrs; i++) {
			for (j = 0; j < 6; j++)
				cpc.pu_active_temporary[j].quantity +=
					sncap_get_tcr_active_quantity(
					  active_tcrs[i]->n_cpus,
					  active_tcrs[i]->cpu,
					  cpc.pu_active_temporary[j].type);

			cp_pi = sncap_tcr_find_cpu(active_tcrs[i], "CP");
			if (cp_pi)
				if (cp_pi->speed_step > max_speed_step)
					max_speed_step = cp_pi->speed_step;
		}
		cpc.tmp_speed_step = max_speed_step;

		APIDEBUG("CPs: %d, ICF: %d, IFL: %d, "
			"SAP: %d, AAP: %d, IIP: %d\n",
			cpc.pu_active_temporary[0].quantity,
			cpc.pu_active_temporary[1].quantity,
			cpc.pu_active_temporary[2].quantity,
			cpc.pu_active_temporary[3].quantity,
			cpc.pu_active_temporary[4].quantity,
			cpc.pu_active_temporary[5].quantity);
		APIDEBUG("Capacity Level Indicator (CLI): %d\n",
			cpc.tmp_speed_step);
		APIDEBUG("Temporary active resource quantities have "
			"been calculated.\n");

	} else
		APIDEBUG("No temporary resources were activated.\n");

	APIDEBUG("Printing the CPC PU configuration for CPC '%s'...\n", cpc.id);
	sncap_cpc_print_cpu_configuration(&cpc);
	printf("\n");
	APIDEBUG("The CPC PU configuration for CPC '%s' has been printed.\n",
		cpc.id);
cleanup:
	free(active_tcrs);
	if (records)
		sncap_tcr_list_release(n_tcr, records);

	sncap_cpc_release(&cpc);
	return ret;
}

