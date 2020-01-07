/*
 * sncap.h : header file for the data structures used in the function main.
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
#ifndef SNCAPJOBH
#define SNCAPJOBH
#include <memory.h>
#include <stdio.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include "sncaputil.h"
#include "sncapconf.h"
#include "sncapapi.h"

#define IS_VERBOSE(job) \
	((job)->operation_mode & SNCAP_OP_MODE_VERBOSE)

#define JOBDEBUG(m, ...) { \
	if (job->operation_mode & SNCAP_OP_MODE_VERBOSE) \
		printf(m, ##__VA_ARGS__); \
}

/*
 * sncap Job object requested operation codes.
 */
enum sncap_operation {
	UNDEFINED,
	ACTIVATE,
	DEACTIVATE,
	HELP,
	LIST_CPCS,
	QUERY_CPUS,
	QUERY_LIST,
	QUERY_RECORD,
	VERSION
};

/*
 * Requested for activation/deactivation processor
 * type attributes
 */
#define CPU_TYPE_ICF  0
#define CPU_TYPE_IFL  1
#define CPU_TYPE_SAP  2
#define CPU_TYPE_ZAAP 3
#define CPU_TYPE_ZIIP 4

/*
 * User requested job attributes
 */
#define SNCAP_OP_MODE_TEST     0x01
#define SNCAP_OP_MODE_VERBOSE  0x02
#define SNCAP_OP_MODE_PROMPTPW 0x04
#define SNCAP_OP_MODE_DRYRUN   0x08
struct sncap_job {
	char *CPC_id;
	char *TCR_id;
	char operation_mode;
	enum sncap_operation operation;
	char *config_file;
	char *software_model;
	struct sncap_connection connection;
	int cpu_quantity[5];
};

/*
 *	function: job_create
 *
 *	purpose: parse the command line arguments and create the job
 *		 structure reading the connection parameters from
 *		 configuration file if necessary.
 */
int job_create(struct sncap_job *job, int argc, char **argv);

/*
 *      function: job_verify
 *
 *      purpose: verify the arguments for the operation specified in the
 *               command line.
 */
int job_verify(const struct sncap_job *job);

/*
 *      function: job_generate_xml
 *
 *      purpose: creates the processor information XML description for
 *               the HwmcaCommand API call for the temporary capacity
 *               record activation/deactivation.
 */
int job_generate_xml(const struct sncap_job *job,
			const char *actual_software_model,
			const int *cpu_quantity,
			char **xml);

/*
 *	function: job_print
 *
 *	purpose: prints the Job object attributes to the stdout stream.
 */
void job_print(const struct sncap_job *job);

/*
 *	function: job_release
 *
 *	purpose: releases memory used by sncap_job object data.
 */
void job_release(struct sncap_job *job);
#endif
