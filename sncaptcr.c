/*
 * sncaptcr.c : snCap Temporary Capacity Record object API.
 *
 * Copyright IBM Corp. 2012, 2013
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */
#include "sncaptcr.h"

#define HANDLE_ERROR(l) { \
	if (ret != DSM_OK) { \
		ret = SNCAP_PROGRAM_ERROR; \
		goto l; \
	} \
}

#define SNCAP_SEC_IN_DAY 86400

/*
 *	fuction: sncap_convert_data
 *
 *	purpose: converts the timestamp received from SE to the output
 *		 timestamp representation.
 */
static int sncap_convert_date(const char *src_date, char *target_date)
{
	int ret = SNCAP_OK;
	int year = 0;
	int month = 0;
	int day = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;

	if (strlen(src_date) < 14) {
		sprintf(target_date, "%-19s", "----/--/-- --:--:--");
		return SNCAP_OK;
	}

	ret = sscanf(src_date,
		"%4d%2d%2d%2d%2d%2d",
		&year,
		&month,
		&day,
		&hours,
		&minutes,
		&seconds);
	if (ret != 6)
		return SNCAP_PROGRAM_ERROR;
	sprintf(target_date,
		"%4d/%02d/%02d %02d:%02d:%02d",
		year,
		month,
		day,
		hours,
		minutes,
		seconds);
	return SNCAP_OK;
}

/*
 *      function: sncap_tcr_find_software_model
 *
 *      purpose: verify that the specified software model belongs to the
 *               set of software models defined in the temporary capacity
 *               record and return the pointer to appropriate target.
 */
struct sncap_tcr_target *
sncap_tcr_find_software_model(const struct sncap_tcr *tcr, const char *model)
{
	int i = 0;

	assert(tcr);
	assert(model);

	for (i = 0; i < tcr->n_targets; i++)
		if (strcmp(tcr->target[i].software_model, model) == 0)
			return &tcr->target[i];

	return NULL;
}

/*
 *      function: sncap_tcr_get_data
 *
 *      purpose: retrieve the temporary capacity record attributes form the
 *               record pseudo-XML definition.
 */
int sncap_tcr_get_data(const char *xml, struct sncap_tcr *tcr)
{
	int ret = DSM_OK;
	struct dsm_node *dsm_root = NULL;
	char *valbuf = NULL;
	const char *path = NULL;
	int intvalue = 0;
	char timestamp[20] = {'\0'};
	int n_children = 0;
	struct dsm_node **children = NULL;
	struct tm *expiration_date = NULL;
	struct tm *today = NULL;
	time_t expiration_time = 0;
	int delta_sec = 0;
	int i = 0;

	ret = dsm_parse_xml(xml, &dsm_root);
	if (ret != DSM_OK) {
		ret = SNCAP_PROGRAM_ERROR;
		goto cleanup;
	}

	path = "report/record/recordid";
	ret = dsm_get_value_string(dsm_root, path, &valbuf);
	HANDLE_ERROR(cleanup);
	tcr->id = valbuf;

	path = "report/record/recordtype";
	ret = dsm_get_value_string(dsm_root, path, &valbuf);
	HANDLE_ERROR(cleanup);
	tcr->type = valbuf;

	path = "report/record/status";
	ret = dsm_get_value_string(dsm_root, path, &valbuf);
	HANDLE_ERROR(cleanup);
	if (strcasecmp(valbuf, "none") == 0 ||
	    strcasecmp(valbuf, "Available") == 0) {
		tcr->status = strdup("Installed");
		free(valbuf);
	} else
		tcr->status = valbuf;

	path = "report/record/remainingrealactivations";
	ret = dsm_get_value_int(dsm_root, path, &intvalue);
	HANDLE_ERROR(cleanup);
	tcr->real.remaining_activations = intvalue;

	path = "report/record/remainingtestactivations";
	ret = dsm_get_value_int(dsm_root, path, &intvalue);
	HANDLE_ERROR(cleanup);
	tcr->test.remaining_activations = intvalue;

	path = "report/record/recordexpiration";
	ret = dsm_get_value_string(dsm_root, path, &valbuf);
	HANDLE_ERROR(cleanup);
	ret = sncap_convert_date(valbuf, timestamp);
	if (ret != SNCAP_OK) {
		free(valbuf);
		goto cleanup;
	}
	tcr->expiration = strdup(timestamp);
	free(valbuf);

	if (TCR_IS_ACTIVE(tcr)) {
		path = "report/record/activationstart";
		ret = dsm_get_value_string(dsm_root, path, &valbuf);
		HANDLE_ERROR(cleanup);
		ret = sncap_convert_date(valbuf, timestamp);
		if (ret != SNCAP_OK) {
			free(valbuf);
			goto cleanup;
		}
		tcr->last_activation = strdup(timestamp);
		free(valbuf);
	} else
		tcr->last_activation = NULL;

	path = "report/record/processorinfo";
	ret = dsm_get_all(dsm_root, path, &n_children, &children);
	HANDLE_ERROR(cleanup);

	tcr->n_cpus = n_children;
	tcr->cpu = calloc(n_children, sizeof(*tcr->cpu));
	for (i = 0; i < tcr->n_cpus; i++) {
		ret = dsm_get_value_string(children[i],
					"processorinfo/type",
					&tcr->cpu[i].type);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_int(children[i],
					"processorinfo/procstep",
					&tcr->cpu[i].proc_step);
		if (ret != DSM_OK) {
			tcr->cpu[i].proc_step = 0;
			ret = DSM_OK;
		}
		ret = dsm_get_value_int(children[i],
					"processorinfo/speedstep",
					&tcr->cpu[i].speed_step);
		if (ret != DSM_OK) {
			tcr->cpu[i].speed_step = 0;
			ret = DSM_OK;
		}
		ret = dsm_get_value_int(children[i],
					"processorinfo/max",
					&tcr->cpu[i].proc_max);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_int(children[i],
					"processorinfo/remainingprocdays",
					&tcr->cpu[i].remaining_proc_days);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_int(children[i],
					"processorinfo/remainingmsudays",
					&tcr->cpu[i].remaining_msu_days);
		if (ret != DSM_OK) {
			tcr->cpu[i].remaining_msu_days = -1;
			ret = DSM_OK;
		}
	}
	free(children);
	children = NULL;

	path = "report/record/target";
	ret = dsm_get_all(dsm_root, path, &n_children, &children);
	HANDLE_ERROR(cleanup);

	tcr->n_targets = n_children;
	tcr->target = calloc(n_children, sizeof(*tcr->target));
	for (i = 0; i < tcr->n_targets; i++) {
		ret = dsm_get_value_int(children[i],
					"target/procstep",
					&tcr->target[i].proc_step);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_int(children[i],
					"target/speedstep",
					&tcr->target[i].speed_step);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_string(children[i],
					"target/softwaremodel",
					&tcr->target[i].software_model);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_int(children[i],
					"target/billablemsucost",
					&tcr->target[i].msu_cost);
		HANDLE_ERROR(cleanup);

		ret = dsm_get_value_int(children[i],
					"target/billablemsudelta",
					&tcr->target[i].msu_delta);
		HANDLE_ERROR(cleanup);
	}
	free(children);
	children = NULL;

	path = "report/record/remainingrealdays";
	ret = dsm_get_value_int(dsm_root, path, &intvalue);
	HANDLE_ERROR(cleanup);
	tcr->real.remaining_days = intvalue;

	path = "report/record/remainingtestdays";
	ret = dsm_get_value_int(dsm_root, path, &intvalue);
	HANDLE_ERROR(cleanup);
	tcr->test.remaining_days = intvalue;

	/*
	 * If there are no record activations in the Real or Test mode
	 * are available and the record is in the activated state, adjust
	 * the record expiration date by the number of remaining days from
	 * today because the record cannot be activated anymore.
	 *
	 * Adjust the expiration date if the number of remaining activation
	 * days in not unlimited.
	 */
	if (TCR_IS_ACTIVE(tcr) &&
		tcr->real.remaining_activations == 0 &&
		tcr->test.remaining_activations == 0) {

		expiration_time = time(NULL);
		today = gmtime(&expiration_time);

		expiration_time -= today->tm_hour * 3600 + today->tm_min * 60 +
				today->tm_sec + 1;

		if (!strcasecmp(tcr->status, "Real"))
			delta_sec = SNCAP_SEC_IN_DAY * tcr->real.remaining_days;
		else
			delta_sec = SNCAP_SEC_IN_DAY * tcr->test.remaining_days;

		if (delta_sec >= 0) {
			expiration_time += delta_sec;
			expiration_date = gmtime(&expiration_time);

			tcr->expiration[0] = '\0';
			sprintf(tcr->expiration,
				"%4d/%02d/%02d %02d:%02d:%02d",
				expiration_date->tm_year + 1900,
				expiration_date->tm_mon + 1,
				expiration_date->tm_mday,
				expiration_date->tm_hour,
				expiration_date->tm_min,
				expiration_date->tm_sec);
		}
	}
cleanup:
	free(children);
	if (dsm_root) {
		dsm_tree_release(dsm_root);
		free(dsm_root);
	}
	return ret;
}

/*
 *      function: sncap_get_active_quantity
 *
 *      purpose: returns the quantity of temporary active CPUs of given type,
 *              defined in a temporary capacity record.
 */
int sncap_get_tcr_active_quantity(const int n_cpus,
				struct sncap_tcr_procinfo *procinfo,
				const char *cpu_type)
{
	int i = 0;

	assert(procinfo);
	assert(cpu_type);

	for (i = 0; i < n_cpus; i++)
		if (strcasecmp(procinfo[i].type, cpu_type) == 0)
			return procinfo[i].proc_step;
	return 0;
}

/*
 *	function: sncap_tcr_find_cpu
 *
 *	purpose: find processor informaion item in the TCR processor
 *		informaion array using processor type as the search
 *		argument.
 */
struct sncap_tcr_procinfo *sncap_tcr_find_cpu(const struct sncap_tcr *tcr,
				const char *type)
{
	int i = 0;

	assert(tcr);
	assert(type);

	for (i = 0; i < tcr->n_cpus; i++)
		if (strcasecmp(tcr->cpu[i].type, type) == 0)
			return &tcr->cpu[i];
	return NULL;
}

/*
 *      function: sncap_tcr_get_current_model
 *
 *      purpose: returns the currently active model capacity identifier
 *              of the given TCR.
 */
const char *sncap_tcr_get_current_model(const struct sncap_tcr *tcr)
{
	int i = 0;

	assert(tcr);

	for (i = 0; i < tcr->n_targets; i++)
		if (tcr->target[i].proc_step == 0 &&
		    tcr->target[i].speed_step == 0)
			return tcr->target[i].software_model;

	return NULL;
}

/*
 *      function: sncap_tcr_print_attributes
 *
 *      purpose: print the record attribute value report to the stdout stream.
 */
void sncap_tcr_print_attributes(const struct sncap_tcr *tcr,
			const char *cpc_name,
			const int current_msu,
			const char *model_capacity_id)
{
	char out_buf[81] = {'\0'};
	int i = 0;
	int j = 0;
	struct sncap_tcr_procinfo *cpu = NULL;
	const char *record_type = NULL;
	char outbuf[2][13] = { {'\0', '\0'} };
	char *type_pair[3][2] = { {"CP", "SAP"},
			{"ICF", "IFL"},
			{"AAP", "IIP"} };
	char *outline[3] = {"CPs   %-12s            SAPs  %-12s\n",
			    "ICFs  %-12s            IFLs  %-12s\n",
			    "zAAPs %-12s            zIIPs %-12s\n"};

	assert(tcr);

	printf("\nCPC %s Record Details - %-s\n", cpc_name, tcr->id);

	printf("Record ID: %s ", tcr->id);
	if (TCR_IS_ACTIVE(tcr))
		sprintf(out_buf, "Active-%s (Attention!)", tcr->status);
	else
		strcpy(out_buf, "Installed");
	printf("Status: %s\n", out_buf);

	record_type = tcr->type;
	if (strcasecmp(tcr->type, "PLANNED_EVENT") == 0)
		record_type = "CPE";
	printf("Record Type: %s\n", record_type);

	printf("Status Details: ");
	if (tcr->real.remaining_activations == 0)
		printf("No activations remaining.\n");
	else
		printf("N/A\n");
	printf("Last Activation Time: ");
	if (TCR_IS_ACTIVE(tcr))
		printf("%s GMT\n", tcr->last_activation);
	else
		printf("\n");

	printf("%s - Model Capacity Id.: %s  ",
			cpc_name,
			model_capacity_id);
	printf("MSU Value: %d\n", current_msu);

	printf("\nAvailable Model Capacity Identifiers:\n");
	printf("Capacity Id. CLIs CPs  Target MSU Value MSU Cost\n");
	printf("------------ ---- ---- ---------------- --------\n");

	for (i = 0; i < tcr->n_targets; i++)
		printf("%-12s %4d %4d %16d %8d\n",
			tcr->target[i].software_model,
			tcr->target[i].speed_step,
			tcr->target[i].proc_step,
			tcr->target[i].msu_delta + current_msu,
			tcr->target[i].msu_cost);

	printf("\n");
	printf("Processor Counts (Maximum/Active/Remaining Days)\n");
	printf("------------------------------------------------\n");

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 2; j++) {
			cpu = sncap_tcr_find_cpu(tcr, type_pair[i][j]);
			if (!cpu)
				sprintf(outbuf[j], "---/---/----");
			else
				if (cpu->proc_max < 0)
					sprintf(outbuf[j],
						"  */%3d/%4d",
						cpu->proc_step,
						cpu->remaining_proc_days);
				else
					sprintf(outbuf[j],
						"%3d/%3d/%4d",
						cpu->proc_max,
						cpu->proc_step,
						cpu->remaining_proc_days);
		}
		printf(outline[i], outbuf[0], outbuf[1]);
	}
	cpu = sncap_tcr_find_cpu(tcr, "CP");
	if (cpu)
		printf("\nRemaining MSU days: %-4d\n",
			cpu->remaining_msu_days);

	printf("\nRecord Expiration Date: %s", tcr->expiration);
	if (strncmp(tcr->expiration, "Undefined", strlen("Undefined")) == 0)
		printf("\n");
	else
		printf(" GMT\n");
	printf("Real Activations..............: %4d\n",
		tcr->real.remaining_activations);
	printf("Real Activation Days Remaining: %4d\n",
		tcr->real.remaining_days);
	printf("Test Activations..............: %4d\n",
		tcr->test.remaining_activations);
	printf("Test Activation Days Remaining: %4d\n",
		tcr->test.remaining_days);
}

/*
 *      function: sncap_tcr_print_list_item
 *
 *      purpose: print to stdout the CPC TCR list item attributes.
 */
void sncap_tcr_print_list_item(const struct sncap_tcr *tcr)
{
	const char *type = NULL;

	type = tcr->type;
	if (strcasecmp(tcr->type, "PLANNED_EVENT") == 0)
		type = "CPE";

	printf("%-8s %-11s %-9s %9d %9d %16s\n",
		tcr->id,
		type,
		tcr->status,
		tcr->real.remaining_activations,
		tcr->test.remaining_activations,
		tcr->expiration);
}

/*
 *      function: sncap_tcr_list_release
 *
 *      purpose: release memory used by the record array items.
 */
void sncap_tcr_list_release(const int n_tcr, struct sncap_tcr *records)
{
	int i = 0;

	assert(records);

	for (i = 0; i < n_tcr; i++)
		sncap_tcr_release(&records[i]);

	free(records);
}

/*
 *      function: sncap_tcr_release
 *
 *      purpose: release memory used by the record items.
 */
void sncap_tcr_release(struct sncap_tcr *record)
{
	int i = 0;

	free(record->id);
	free(record->snmp_id);
	free(record->type);
	free(record->status);

	if (record->cpu)
		for (i = 0; i < record->n_cpus; i++)
			free(record->cpu[i].type);
	free(record->cpu);

	free(record->expiration);
	free(record->last_activation);

	if (record->target)
		for (i = 0; i < record->n_targets; i++)
			free(record->target[i].software_model);
	free(record->target);
}

/*
 *      function: sncap_tcr_find_min_model
 *
 *      purpose: returns pointer to the model capacity identifier of the
 *               model capacity identifier with the minimal MSU Delta.
 */
const char *sncap_tcr_find_min_model(const struct sncap_tcr *tcr)
{
	int i = 0;
	int min_msu_delta = 0;
	const char *min_model_id = NULL;

	assert(tcr);

	if (tcr->n_targets == 0)
		return NULL;

	min_msu_delta = tcr->target[0].msu_delta;
	min_model_id = tcr->target[0].software_model;
	for (i = 1; i < tcr->n_targets; i++)
		if (tcr->target[i].msu_delta < min_msu_delta) {
			min_msu_delta = tcr->target[i].msu_delta;
			min_model_id = tcr->target[i].software_model;
		}
	return min_model_id;
}

