/*
   snCAP - snIPL interface to control dynamic CPU capacity

   Copyright IBM Corp. 2012, 2016
   Published under the terms and conditions of the CPL (common public license)

   PLEASE NOTE:
   snCAP is provided under the terms of the enclosed common public license
   ("agreement"). Any use, reproduction or distribution of the program
   constitutes recipient's acceptance of this agreement.
*/
#include "sncap.h"

/*
 *	function: sncap_print_version
 *
 *	purpose: prints sncap version to the stdout stream.
 */
static void sncap_print_version()
{
	printf("%s\n", SNCAP_VERSION);
	printf("%s\n", SNCAP_COPYRIGHT);
}

/*
 *	function: sncap_help
 *
 *	purpose: prints the brief sncap help information to the
 *		 stdout stream.
 */
static void sncap_help()
{
	printf("Call simple network CPU capacity management\n");
	printf("Usage: sncap [-v|-h] [<cpcid>] [options] [<recid>]\n");
	printf(" cpcid\t\t\t\tname of the CPC to manage\n");
	printf(" recid\t\t\t\tname of the temporary capacity record "
		"to manage\n");
	printf("\n");
	printf(" -a --activate <recid>\t\tactivates temporary"
		" capacity record\n");
	printf(" -t --test\t\t\tspecifies record activation in test mode\n");
	printf("\n");
	printf(" -d --deactivate <recid>\tdeactivates temporary capacity "
		"record\n");
	printf("\n");
	printf(" -c --pu_configuration\t\tqueries the CPC PU ");
	printf("configuration\n");
	printf(" -l --list_records\t\tlists records installed on SE\n");
	printf(" -q --query <recid>\t\tqueries record state data\n");
	printf(" -x --list_cpcs\t\t\tlists CPCs controlled by SE of HMC\n");
	printf("\n");
	printf(" -n --no_record_changes\t\tspecifies no record changes mode\n");
	printf(" -V --verbose\t\t\tspecifies verbose mode of execution\n");
	printf("\n");
	printf(" -f --configfilename <filename> configuration file name\n");
	printf(" -S --se <addr>\t\t\tSE or HMC IP address or DNS name\n");
	printf(" -p --password <password>\tspecifies the SE or HMC password\n");
	printf(" -P --promptpassword\t\tspecifies password to be prompted\n");
	printf(" -u --userid\t\t\tspecifies the HMC username\n");
	printf(" -e --noencryption\t\tdisables encryption for this connection");
	printf("\n\n");
	printf(" -h --help\t\t\tdisplays brief help information\n");
	printf(" -v --version\t\t\tdisplays version information\n");
	printf("\n");
	printf(" -m --model_capacity <id>\tspecifies model capacity id\n");
	printf("    --icf n\t\t\tspecifies the number of ICF processors\n");
	printf("    --ifl n\t\t\tspecifies the number of IFL processors\n");
	printf("    --sap n\t\t\tspecifies the number of SAP processors\n");
	printf("    --zaap n\t\t\tspecifies the number of ZAAP processors\n");
	printf("    --ziip n\t\t\tspecifies the number of ZIIP processors\n");
	printf("\n");
	printf("    --timeout <timeout>\t\tspecifies timeout in ms\n");
	printf("\n");
	printf("Please report bugs to: linux390@de.ibm.com\n");
}

/*
 *	function: main
 *
 *	purpose: point of control
 */
int main(int argc, char **argv)
{
	int rc = SNCAP_OK;
	struct sncap_api *api = NULL;
	char *api_version = NULL;
	_Bool verbose = 0;

	struct sncap_job job = {
		NULL,
		NULL,
		0x00,
		UNDEFINED,
		NULL,
		NULL,
		{NULL, NULL, -1},
		{0, 0, 0, 0, 0}
		};

	rc = job_create(&job, argc, argv);
	if (rc != SNCAP_OK) {
		if (rc == SNCAP_NO_OPTIONS)
			sncap_help();
		goto cleanup;
	}

	if (job.operation_mode & SNCAP_OP_MODE_VERBOSE)
		job_print(&job);

	rc = job_verify(&job);
	if (rc != SNCAP_OK)
		goto cleanup;
	/*
	 * Perform the operations that does not need the connection to
	 * the Support Element API server.
	 */
	switch (job.operation) {
	case HELP:
		sncap_help();
		goto cleanup;
		break;
	case VERSION:
		sncap_print_version();
		goto cleanup;
		break;
	default:
		break;
	}
	/*
	 * Perform the operations on SE API server;
	 */
	rc = sncap_connect(&job.connection,
			job.operation_mode & SNCAP_OP_MODE_VERBOSE,
			&api);
	if (rc != SNCAP_OK)
		goto cleanup;

	/*
	 * Check the SE software version because the operaions on the
	 * temporary capacity records are available in versions 2.10.0
	 * and later.
	 */
	APIDEBUG("Checking server software version...\n");
	rc = sncap_get_se_version(api, &api_version);
	if (rc != SNCAP_OK) {
		APIDEBUG("Could not get the server software version.\n");
		goto cleanup;
	}
	if (sncap_compare_versions(api_version, "2.10.0") < 0) {
		rc = SNCAP_INVALID_SE_VERSION;
		APIDEBUG("Invalid server software version '%s'", api_version);
		goto cleanup;
	}
	APIDEBUG("Server software version check has been passed.\n");
	/*
	 * Since an HMC can control the CPC with different SE software
	 * versions we need to check the version of the SE software for the
	 * CPC being processed.
	 */
	if (job.operation != LIST_CPCS) {
		APIDEBUG("Checking the CPC SE software version...\n");
		rc = sncap_check_cpc_version(api, job.CPC_id);
		if (rc != SNCAP_OK) {
			APIDEBUG("Could not get CPC SE software version.\n");
			goto cleanup;
		}
		APIDEBUG("The CPC SE software version check has "
			"been passed.\n");
	}
	/*
	 * Perform the operation requested by the user.
	 */
	switch (job.operation) {
	case ACTIVATE:
		rc = sncap_change_activation_level(api, &job);
		break;
	case DEACTIVATE:
		rc = sncap_change_activation_level(api, &job);
		break;
	case LIST_CPCS:
		rc = sncap_list_cpcs(api);
		break;
	case QUERY_CPUS:
		rc = sncap_query_cpus(api, job.CPC_id);
		break;
	case QUERY_LIST:
		rc = sncap_query_record_list(api, job.CPC_id);
		break;
	case QUERY_RECORD:
		rc = sncap_query_record(api, job.CPC_id, job.TCR_id);
		break;
	default:
		rc = SNCAP_PROGRAM_ERROR;
		goto cleanup;
		break;
	}
cleanup:
	verbose = IS_VERBOSE(&job);
	if (api != NULL) {
		if (api->ret == 19)
			rc = SNCAP_TIMEOUT_OCCURRED;
		if (sncap_disconnect(api) != SNCAP_OK) {
			sncap_api_release(api);
			free(api);
		}
	}

	free(api_version);
	job_release(&job);

	sncap_print_message(verbose, rc);
	return rc;
}
