/*
 * sncapdefs.h : header file for the sncap commond definitions.
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
#ifndef SNCAPUTILH
#define SNCAPUTILH
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hwmcaapi.h"

#define SNCAP_PASSWORD_MASK "********"
/*
 * Application return codes.
 */
#define SNCAP_OK			0
#define SNCAP_UNKNOWN_OPTION		1
#define SNCAP_INVALID_OPTION_VALUE	2
#define SNCAP_DUPLICATE_OPTION		3
#define SNCAP_CONFLICTING_OPTIONS	4
#define SNCAP_NO_OPTIONS		5
#define SNCAP_NO_SERVER			6
#define SNCAP_NO_RECORD			7
#define SNCAP_INVALID_SE_VERSION	8
#define SNCAP_NO_PASSWORD		9
#define SNCAP_NO_CPC			10
#define SNCAP_INSUFFICIENT_HARDWARE	11
#define SNCAP_TIMEOUT_OCCURRED		12
#define SNCAP_INVALID_PASSWORD		13
#define SNCAP_INVALID_SOFTWARE_MODEL	14
#define SNCAP_OPERATION_MISSED		15
#define SNCAP_INVALID_IP		16
#define SNCAP_TOO_MANY_CPU		17
#define SNCAP_HWMCAAPI_ERROR		18
#define SNCAP_MISSING_MANDATORY_OPTION	19
#define SNCAP_CONFIG_FILE_ERROR		20
#define SNCAP_UNDEFINED_CPU_TYPE	21
#define SNCAP_RECORD_IS_NOT_ACTIVE	22
#define SNCAP_ACTIVATIONS_EXHAUSTED	23
#define SNCAP_NO_USERNAME		24
#define SNCAP_USERNAME_ENC_OFF		25
#define SNCAP_WRONG_ENC			26
#define SNCAP_LIBHWMCAAPISO_ERROR	30
#define SNCAP_INVALID_SE_RESPONSE	50
#define SNCAP_MEMORY_ALLOC_FAILURE	90
#define SNCAP_PROGRAM_ERROR		99
#define SNCAP_NOT_FOUND			100

/*
 *	function: sncap_print_message
 *
 *	purpose: print a message for a given return code to the stdout
 *		 stream
 */
void sncap_print_message(const _Bool verbose, const int ret);

/*
 *	function: sncap_is_api_return_code
 *
 *	purpose: determine if the supplied return code belongs to the supported
 *		 HWMCAAPI API return code set.
 */
_Bool sncap_is_api_return_code(const ULONG api_ret);

/*
 *	function: sncap_print_api_message
 *
 *	purpose: print a message for a given return code from the HWMCAAPI
 *		 API, and return appropriate sncap exit code to caller.
 */
int sncap_print_api_message(const _Bool verbose, const char *CPC_id,
			    ULONG api_ret);

/*
 *      function: sncap_compare_versions
 *
 *      purpose: compare two version strings in binary mode.
 */
int sncap_compare_versions(const char *version1, const char *version2);
#endif
