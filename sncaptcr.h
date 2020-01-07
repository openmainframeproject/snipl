/*
 * sncaptcr.h : header file for Temporary Capacity Record object API.
 *
 * Copyright IBM Corp. 2012, 2013
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *  config is provided under the terms of the enclosed common public license
 *  ("agreement"). Any use, reproduction or distribution of the program
 *  constitutes recipient's acceptance of this agreement.
 */
#ifndef SNCAPTCRH
#define SNCAPTCRH

#include <assert.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include "sncaputil.h"
#include "sncapdsm.h"

#define TCR_IS_ACTIVE(rec) \
	(!strcasecmp(rec->status, "Real") || !strcasecmp(rec->status, "Test"))

/*
 * Temporary Capacity Record object data structure definition.
 */

struct sncap_tcr_procinfo {
	char *type;
	int proc_step;
	int speed_step;
	int proc_max;
	int remaining_proc_days;
	int remaining_msu_days;
};
struct sncap_tcr_mode {
	int max_days;
	int remaining_activations;
	int remaining_days;
};

struct sncap_tcr_target {
	int proc_step;
	int speed_step;
	char *software_model;
	int msu_cost;
	int msu_delta;
};

struct sncap_tcr {
	char *id;
	char *snmp_id;
	char *type;
	char *status;
	int n_cpus;
	struct sncap_tcr_procinfo *cpu;
	char *expiration;
	char *last_activation;
	struct sncap_tcr_mode real;
	struct sncap_tcr_mode test;
	int n_targets;
	struct sncap_tcr_target *target;
};

/*
 *	function: sncap_tcr_list_release
 *
 *	purpose: release memory used by the record array items.
 */
void sncap_tcr_list_release(const int n_tcr, struct sncap_tcr *records);

/*
 *	function: sncap_tcr_find_software_model
 *
 *	purpose: verify that the specified software model belongs to the
 *		 set of software models defined in the temporary capacity
 *		 record and return the pointer to appropriate target.
 */
struct sncap_tcr_target *
sncap_tcr_find_software_model(const struct sncap_tcr *tcr, const char *model);

/*
 *	function: sncap_tcr_get_data
 *
 *	purpose: retrieve the temporary capacity record attributes form the
 *		 record pseudo-XML definition.
 */
int sncap_tcr_get_data(const char *xml, struct sncap_tcr *tcr);

/*
 *      function: sncap_get_active_quantity
 *
 *      purpose: returns the quantity of temporary active CPUs of given type,
 *              defined in a temporary capacity record.
 */
int sncap_get_tcr_active_quantity(const int n_cpus,
			struct sncap_tcr_procinfo *procinfo,
			const char *cpu_type);

/*
 *      function: sncap_tcr_find_cpu
 *
 *      purpose: find processor informaion item in the TCR processor
 *              informaion array using processor type as the search
 *              argument.
 */
struct sncap_tcr_procinfo *sncap_tcr_find_cpu(const struct sncap_tcr *tcr,
					 const char *type);

/*
 *	function: sncap_tcr_get_current_model
 *
 *	purpose: returns the currently active model capacity identifier
 *		of the given TCR.
 */
const char *sncap_tcr_get_current_model(const struct sncap_tcr *tcr);

/*
 *	function: sncap_tcr_print_attributes
 *
 *	purpose: print the record attribute value report to the stdout stream.
 */
void sncap_tcr_print_attributes(const struct sncap_tcr *tcr,
			const char *cpc_name,
			const int current_msu,
			const char *model_capacity_id);

/*
 *	function: sncap_tcr_print_list_item
 *
 *	purpose: print to stdout the CPC TCR list item attributes.
 */
void sncap_tcr_print_list_item(const struct sncap_tcr *tcr);

/*
 *	function: sncap_tcr_release
 *
 *	purpose: release memory used by the record items.
 */
void sncap_tcr_release(struct sncap_tcr *record);

/*
 *	function: sncap_tcr_find_min_model
 *
 *	purpose: returns pointer to the model capacity identifier of the
 *		 model capacity identifier with the minimal MSU Delta.
 */
const char *sncap_tcr_find_min_model(const struct sncap_tcr *tcr);
#endif
