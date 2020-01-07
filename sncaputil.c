/*
 * sncaputil.h : header file for the utilitary sncap functions.
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
#include "sncaputil.h"

struct message_entry {
	const int retcode;
	const char *message;
};

struct message_entry message_dictionary[] = {
	{SNCAP_OK,
	 "Execution completed successfully"},
	{SNCAP_UNKNOWN_OPTION,
	 "An unknown option was specified"},
	{SNCAP_INVALID_OPTION_VALUE,
	 "A command option with an invalid value was specified"},
	{SNCAP_DUPLICATE_OPTION,
	 "A command option was specified more than once"},
	{SNCAP_CONFLICTING_OPTIONS,
	 "Conflicting command options were specified"},
	{SNCAP_NO_OPTIONS,
	 "No command option was specified"},
	{SNCAP_NO_SERVER,
	 "No request server was specified on the command line nor "
		"in a configuration file"},
	{SNCAP_NO_RECORD,
	 "Temporary capacity record was not installed on the CPC"},
	{SNCAP_INVALID_SE_VERSION,
	 "Server version is not supported"},
	{SNCAP_NO_PASSWORD,
	 "No password was specified on the command line nor through "
		"a configuration file"},
	{SNCAP_NO_USERNAME,
	 "No username was specified in the command line or configuration "
		"file.\n"
		"Username is required, if encryption is selected"},
	{SNCAP_USERNAME_ENC_OFF,
	 "username must not be specified, when encryption is disabled"},
	{SNCAP_WRONG_ENC,
	 "The wrong encryption value was specified in the configuration file.\n"
		"Use \"yes\" or \"no\""},
	{SNCAP_NO_CPC,
	 "Specified CPC does not exist"},
	{SNCAP_INSUFFICIENT_HARDWARE,
	 "Insufficient hardware resources to complete the request"},
	{SNCAP_TIMEOUT_OCCURRED,
	 "The timeout occurred, the command is cancelled"},
	{SNCAP_INVALID_PASSWORD,
	 "Invalid password was specified"},
	{SNCAP_INVALID_SOFTWARE_MODEL,
	 "Invalid model capacity identifier was specified"},
	{SNCAP_OPERATION_MISSED,
	 "No operation option was specified"},
	{SNCAP_INVALID_IP,
	"Invalid server IP address or DNS name was specified"},
	{SNCAP_TOO_MANY_CPU,
	 "Too many processors were specified for the operation. "
		"No processors were activated or deactivated"},
	{SNCAP_MISSING_MANDATORY_OPTION,
	"Missing mandatory argument"},
	{SNCAP_HWMCAAPI_ERROR,
	"An error was received from HWMCAAPI API"},
	{SNCAP_CONFIG_FILE_ERROR,
	 "An error occurred when processing the configuration file"},
	{SNCAP_UNDEFINED_CPU_TYPE,
	 "One of specified processor types is not defined in the "
		"temporary capacity record"},
	{SNCAP_RECORD_IS_NOT_ACTIVE,
	 "Cannot deactivate the inactive temporary capacity record"},
	{SNCAP_ACTIVATIONS_EXHAUSTED,
	"The number of activations was exhausted"},
	{SNCAP_LIBHWMCAAPISO_ERROR,
	 "An error occurred when loading the library libhwmcaapi.so"},
	{SNCAP_INVALID_SE_RESPONSE,
	 "Response from server could not be interpreted"},
	{SNCAP_MEMORY_ALLOC_FAILURE,
	 "A storage allocation failure occurred"},
	{SNCAP_PROGRAM_ERROR,
	 "A program error occurred"},
	{-1, NULL}
};

/*
 *	function: sncap_print_message
 *
 *	purpose: prints a message for a return code to the stderr stream.
 */
void sncap_print_message(const _Bool verbose, const int ret)
{
	int i;
	const char *msg = NULL;
	FILE *output = NULL;

	if (verbose)
		output = stdout;
	else
		output = stderr;

	for (i = 0; message_dictionary[i].retcode != -1; i++)
		if (message_dictionary[i].retcode == ret) {
			msg = message_dictionary[i].message;
			break;
		}

	if (msg)
		fprintf(output, "%s.\n", msg);
	else
		fprintf(output,
			"Unknown return code %d has been received.\n",
			ret);
}

/*
 *      Error message texts for the HWMCAAPIAPI return codes
 */
struct api_message_item {
	ULONG code;
	int sncap_rc;
	const char *message;
};

static struct api_message_item api_messages[] = {
	{0, SNCAP_OK, "NO_ERROR"},
	{1, SNCAP_HWMCAAPI_ERROR, "NO_SUCH_OBJECT"},
	{2, SNCAP_HWMCAAPI_ERROR, "INVALID_DATA_TYPE"},
	{3, SNCAP_HWMCAAPI_ERROR, "INVALID_DATA_LENGTH"},
	{4, SNCAP_HWMCAAPI_ERROR, "INVALID_DATA_PTR"},
	{5, SNCAP_HWMCAAPI_ERROR, "INVALID_DATA_VALUE"},
	{6, SNCAP_HWMCAAPI_ERROR, "INVALID_INIT_PTR"},
	{7, SNCAP_HWMCAAPI_ERROR, "INVALID_ID_PTR"},
	{8, SNCAP_HWMCAAPI_ERROR, "INVALID_BUF_PTR"},
	{9, SNCAP_HWMCAAPI_ERROR, "INVALID_BUF_SIZE"},
	{10, SNCAP_HWMCAAPI_ERROR, "INVALID_DATATYPE_PTR"},
	{11, SNCAP_INVALID_IP, "INVALID_TARGET (wrong IP address)"},
	{12, SNCAP_HWMCAAPI_ERROR, "INVALID_EVENT_MASK"},
	{13, SNCAP_HWMCAAPI_ERROR, "INVALID_PARAMETER"},
	{14, SNCAP_HWMCAAPI_ERROR, "READ_ONLY_OBJECT"},
	{15, SNCAP_HWMCAAPI_ERROR, "SNMP_INIT_ERROR"},
	{16, SNCAP_HWMCAAPI_ERROR, "INVALID_OBJECT_ID"},
	{17, SNCAP_HWMCAAPI_ERROR, "REQUEST_ALLOC_ERROR"},
	{18, SNCAP_HWMCAAPI_ERROR, "REQUEST_SEND_ERROR"},
	{19, SNCAP_TIMEOUT_OCCURRED,
	 "TIMEOUT (may be also wrong community/password)"},
	{20, SNCAP_HWMCAAPI_ERROR, "REQUEST_RECV_ERROR"},
	{21, SNCAP_HWMCAAPI_ERROR, "SNMP_ERROR"},
	{22, SNCAP_HWMCAAPI_ERROR, "INVALID_TIMEOUT"},
	{23, SNCAP_HWMCAAPI_ERROR, "INVALID_CMD"},
	{24, SNCAP_HWMCAAPI_ERROR, "OBJECT_BUSY"},
	{25, SNCAP_HWMCAAPI_ERROR, "INVALID_OBJECT"},
	{26, SNCAP_HWMCAAPI_ERROR, "COMMAND_FAILED"},
	{27, SNCAP_HWMCAAPI_ERROR, "INITTERM_OK"},
	{28, SNCAP_HWMCAAPI_ERROR, "INVALID_HOST / DISRUPTIVE_OK"},
	{29, SNCAP_HWMCAAPI_ERROR, "INVALID_COMMUNITY / PARTIAL_HW"},
	{30, SNCAP_HWMCAAPI_ERROR, "CBU_NO_SPARES"},
	{31, SNCAP_HWMCAAPI_ERROR, "CBU_TEMPORARY"},
	{32, SNCAP_HWMCAAPI_ERROR, "CBU_NOT_ENABLED"},
	{33, SNCAP_HWMCAAPI_ERROR, "CBU_NOT_AUTHORIZED"},
	{34, SNCAP_HWMCAAPI_ERROR, "CBU_FAILED"},
	{35, SNCAP_HWMCAAPI_ERROR, "CBU_ALREADY_ACTIVE"},
	{36, SNCAP_HWMCAAPI_ERROR, "CBU_INPROGRESS"},
	{37, SNCAP_HWMCAAPI_ERROR, "CBU_CPSAP_SPLIT_CHG"},
	{38, SNCAP_HWMCAAPI_ERROR, "INVALID_MACHINE_STATE"},
	{39, SNCAP_NO_RECORD, "NO_RECORDID"},
	{40, SNCAP_INVALID_SOFTWARE_MODEL, "NO_SW_MODEL"},
	{41, SNCAP_INSUFFICIENT_HARDWARE, "NOT_ENOUGH_RESOURCES"},
	{42, SNCAP_INSUFFICIENT_HARDWARE, "NOT_ENOUGH_ACTIVE_RESOURCES"},
	{43, SNCAP_INSUFFICIENT_HARDWARE, "ACT_LESS_RESOURCES"},
	{44, SNCAP_INSUFFICIENT_HARDWARE, "DEACT_MORE_RESOURCES"},
	{45, SNCAP_HWMCAAPI_ERROR, "ACT_TYPE_MISMATCH"},
	{46, SNCAP_HWMCAAPI_ERROR, "API_NOT_ALLOWED"},
	{47, SNCAP_HWMCAAPI_ERROR, "CDU_IN_PROGRESS"},
	{48, SNCAP_HWMCAAPI_ERROR, "MIRRORING_RUNNING"},
	{49, SNCAP_HWMCAAPI_ERROR, "COMMUNICATIONS_NOT_ACTIVE"},
	{50, SNCAP_HWMCAAPI_ERROR, "RECORD_EXPIRED"},
	{51, SNCAP_INSUFFICIENT_HARDWARE, "PARTIAL_CAPACITY"},
	{52, SNCAP_HWMCAAPI_ERROR, "INVALID_REQUEST"},
	{53, SNCAP_HWMCAAPI_ERROR, "ALREADY_ACTIVE"},
	{54, SNCAP_HWMCAAPI_ERROR, "RESERVE_HELD"},
	{55, SNCAP_HWMCAAPI_ERROR, "GENERAL_XML_PARSING_ERROR"},
	{96, SNCAP_INVALID_PASSWORD, "INCORRECT_PASSWORD"},
	{100, SNCAP_HWMCAAPI_ERROR, NULL}
};
/*
 *      function: sncap_is_api_return_code
 *
 *      purpose: determine if the supplied return code belongs to the supported
 *               HWMCAAPI API return code set.
 */
_Bool sncap_is_api_return_code(const ULONG api_ret)
{
	int last_item = 0;

	last_item = sizeof(api_messages) / sizeof(struct api_message_item) - 2;

	return (api_ret >= 0 && api_ret <= api_messages[last_item].code);
}

/*
 *      function: sncap_print_api_message
 *
 *      purpose: prints a message for a given return code from the HWMCAAPI
 *               API, and return appropriate sncap exit code to caller.
 */
int sncap_print_api_message(const _Bool verbose, const char *se_address,
			    ULONG api_ret)
{
	int i;
	const char *msg = NULL;
	FILE *output = NULL;
	int ret;

	if (verbose)
		output = stdout;
	else
		output = stderr;

	for (i = 0; api_messages[i].message != NULL; i++)
		if (api_messages[i].code == api_ret) {
			msg = api_messages[i].message;
			ret = api_messages[i].sncap_rc;
			break;
		}
	if (msg)
		fprintf(output, "%s: %s - %lu.\n",
				se_address,
				msg,
				api_ret);
	else {
		fprintf(output,
			"Unknown return code %lu has been received ",
			api_ret);
		fprintf(output, "for SE %s.\n", se_address);
		ret = SNCAP_HWMCAAPI_ERROR;
	}
	return ret;
}

/*
 *	function: sncap_compare_version
 *
 *	purpose: compare two version strings in binary mode.
 */
int sncap_compare_versions(const char *version1, const char *version2)
{
	char *v[2] = {NULL, NULL};
	int ver_num[2] = {0, 0};
	int version = 0;
	int release = 0;
	int modifier = 0;
	int i = 0;
	int j = 0;

	v[0] = strdup(version1);
	if (!v[0])
		return 1;
	v[1] = strdup(version2);
	if (!v[1])
		return -1;

	for (i = 0; i < 2; i++)
		for (j = 0; v[i][j] != 0; j++)
			if (v[i][j] == '.')
				v[i][j] = ' ';

	for (i = 0; i < 2; i++) {
		sscanf(v[i], "%3d %3d %3d", &release, &version, &modifier);
		ver_num[i] = 1000000 * release + 1000 * version + modifier;
	}

	free(v[0]);
	free(v[1]);

	if (ver_num[0] < ver_num[1])
		return -1;
	if (ver_num[0] > ver_num[1])
		return 1;
	return 0;
}

