/*
 * sncapcpc.h : header file for Central Processor Complex object API.
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
#ifndef SNCAPCPCH
#define SNCAPCPCH

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*
 * The CPC object data structure definition.
 */
struct sncap_cpc_pu {
	const char *type;
	int quantity;
};

struct sncap_cpc {
	char *id;
	char *snmp_id;
	struct sncap_cpc_pu pu_active_temporary[6];
	struct sncap_cpc_pu pu_total[6];
	int pu_available;
	int msu;
	int tmp_speed_step;
	char *current_capacity_model;
	char *tmp_capacity_model;
	char *permanent_capacity_model;
};

/*
 *	function: sncap_cpc_init
 *
 *	purpose: initialize the sncap_cpc structure fields with initial
 *		 values.
 */
void sncap_cpc_init(struct sncap_cpc *cpc);

/*
 *	function: sncap_find_pu
 *
 *	purpose: search for the PU with given type in the array of defined
 *		PUs.
 */
struct sncap_cpc_pu *sncap_cpc_find_pu(struct sncap_cpc_pu *pus,
				const char *type);

/*
 *	function: sncap_cpc_print_cpu_configuration
 *
 *	purpose: print the CPC PU configuration data to the stdout stream.
 */
void sncap_cpc_print_cpu_configuration(const struct sncap_cpc *cpc);

/*
 *	function: sncap_cpc_release
 *
 *	purpose: release the memory used by the sncap_cpc object items.
 */
void sncap_cpc_release(struct sncap_cpc *cpc);
#endif
