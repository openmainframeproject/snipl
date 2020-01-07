/*
 * sncapcpc.c : Central Processor Complex object API.
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
#include "sncapcpc.h"

/*
 *	function: sncap_set_pu
 *
 *	purpose: sets the values in the struct sncap_cpc_pu type object.
 */
static void sncap_set_pu(struct sncap_cpc_pu *pu,
			const char *type,
			const int quantity)
{
	pu->type = type;
	pu->quantity = quantity;
}

/*
 *      function: sncap_cpc_init
 *
 *      purpose: initializes the sncap_cpc structure fields with initial
 *               values.
 */
void sncap_cpc_init(struct sncap_cpc *cpc)
{
	cpc->id = NULL;

	sncap_set_pu(&cpc->pu_active_temporary[0], "CP", 0);
	sncap_set_pu(&cpc->pu_active_temporary[1], "ICF", 0);
	sncap_set_pu(&cpc->pu_active_temporary[2], "IFL", 0);
	sncap_set_pu(&cpc->pu_active_temporary[3], "SAP", 0);
	sncap_set_pu(&cpc->pu_active_temporary[4], "AAP", 0);
	sncap_set_pu(&cpc->pu_active_temporary[5], "IIP", 0);

	sncap_set_pu(&cpc->pu_total[0], "CP", 0);
	sncap_set_pu(&cpc->pu_total[1], "ICF", 0);
	sncap_set_pu(&cpc->pu_total[2], "IFL", 0);
	sncap_set_pu(&cpc->pu_total[3], "SAP", 0);
	sncap_set_pu(&cpc->pu_total[4], "AAP", 0);
	sncap_set_pu(&cpc->pu_total[5], "IIP", 0);

	cpc->pu_available = 0;
	cpc->msu = 0;
	cpc->tmp_speed_step = 0;

	cpc->current_capacity_model = NULL;
	cpc->tmp_capacity_model = NULL;
	cpc->permanent_capacity_model = NULL;
}
/*
 *      function: sncap_find_pu
 *
 *      purpose: searches for the PU with given type in the array of defined
 *              PUs.
 */
struct sncap_cpc_pu *sncap_cpc_find_pu(struct sncap_cpc_pu *pus,
				const char *type)
{
	int i = 0;

	for (i = 0; i < 6; i++)
		if (strcmp(pus[i].type, type) == 0)
			return &pus[i];
	return NULL;
}

/*
 *      function: sncap_cpc_print_cpu_configuration
 *
 *      purpose: print the CPC PU configuration data to the stdout stream.
 */
void sncap_cpc_print_cpu_configuration(const struct sncap_cpc *cpc)
{
	printf("\nCurrent CPC %s PU Configuration\n", cpc->id);
	printf("-----------------------------------------------------\n\n");
	printf("PU Information   CLIs CPs  SAPs ICFs IFLs zAAPs zIIPs\n");
	printf("---------------- ---- ---- ---- ---- ---- ----- -----\n");

	printf("Active Temporary %4d %4d %4d %4d %4d %5d %5d\n",
		cpc->tmp_speed_step,
		cpc->pu_active_temporary[0].quantity,
		cpc->pu_active_temporary[3].quantity,
		cpc->pu_active_temporary[1].quantity,
		cpc->pu_active_temporary[2].quantity,
		cpc->pu_active_temporary[4].quantity,
		cpc->pu_active_temporary[5].quantity);

	printf("Permanent        %4s %4d %4d %4d %4d %5d %5d\n",
		"-",
		cpc->pu_total[0].quantity -
			cpc->pu_active_temporary[0].quantity,
		cpc->pu_total[3].quantity -
			cpc->pu_active_temporary[3].quantity,
		cpc->pu_total[1].quantity -
			cpc->pu_active_temporary[1].quantity,
		cpc->pu_total[2].quantity -
			cpc->pu_active_temporary[2].quantity,
		cpc->pu_total[4].quantity -
			cpc->pu_active_temporary[4].quantity,
		cpc->pu_total[5].quantity -
			cpc->pu_active_temporary[5].quantity);

	printf("Total Used       %4d %4d %4d %4d %4d %5d %5d\n\n",
		cpc->tmp_speed_step,
		cpc->pu_total[0].quantity,
		cpc->pu_total[3].quantity,
		cpc->pu_total[1].quantity,
		cpc->pu_total[2].quantity,
		cpc->pu_total[4].quantity,
		cpc->pu_total[5].quantity);

	printf("Model Capacity Identifier..........: %-5s Available PUs: %d\n",
		cpc->current_capacity_model,
		cpc->pu_available);
	printf("Model Temporary Capacity Identifier: %-5s MSUs.........: %d\n",
		cpc->tmp_capacity_model,
		cpc->msu);
	printf("Model Permanent Capacity Identifier: %-5s\n",
		cpc->permanent_capacity_model);
}

/*
 *	function: sncap_cpc_release
 *
 *	purpose: release the memory used by the sncap_cpc object items.
 */
void sncap_cpc_release(struct sncap_cpc *cpc)
{
	assert(cpc);

	free(cpc->id);
	free(cpc->snmp_id);

	free(cpc->current_capacity_model);
	free(cpc->tmp_capacity_model);
	free(cpc->permanent_capacity_model);
}
