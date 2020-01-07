/*
 * Stonith module for lic_vps User Stonith device
 *   (Linux Image Control Virtual Power Switch)
 *
 * Copyright IBM Corp. 2002, 2011
 * Author(s): Peter Tiedemann    (ptiedem@de.ibm.com)
 * Maintainer: Ursula Braun      (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */

#define	DEVICE	  "LinuxImageControl_VirtualPowerSwitch"
#define DEVICE_ID "LIC_VPS_Device"
#include <snipl_stonith_plugin.h>

#define PIL_PLUGIN              lic_vps
#define PIL_PLUGIN_S		"lic_vps"

/* the following license stuff should be */
/* integrated in <pils/plugin.h> */
/*#define LICENSE_IBMCPL	"ibmcpl" */
/*#define URL_IBMCPL "http://www-128.ibm.com/developerworks/library/os-cpl.html" */

#define LICENSE_IBMCPL	"ibmcpl"
#define URL_IBMCPL   "http://www-128.ibm.com/developerworks/library/os-cpl.html"
#define PIL_PLUGINLICENSE       LICENSE_IBMCPL
#define PIL_PLUGINLICENSEURL	URL_IBMCPL

#include <pils/plugin.h>
#include <syslog.h>
#include <string.h>
#include <snipl.h>

static StonithPlugin *	lic_vps_new(const char *);
static void		lic_vps_destroy(StonithPlugin *);
static const char *	lic_vps_getinfo(StonithPlugin * s, int InfoType);
static const char* const* lic_vps_get_confignames(StonithPlugin *);
static int		lic_vps_set_config(StonithPlugin *, StonithNVpair *);
static int		lic_vps_status(StonithPlugin * );
static int		lic_vps_reset_req(StonithPlugin * s,
					int request, const char * host);
static char **		lic_vps_hostlist(StonithPlugin  *);

static struct stonith_ops lic_vpsOps ={
	lic_vps_new,		/* Create new STONITH object		*/
	lic_vps_destroy,	/* Destroy STONITH object		*/
	lic_vps_getinfo,	/* Return STONITH info string		*/
	lic_vps_get_confignames,/* Return STONITH info string		*/
	lic_vps_set_config,	/* Get configuration from NVpairs	*/
	lic_vps_status,		/* Return STONITH device status		*/
	lic_vps_reset_req,	/* Request a reset			*/
	lic_vps_hostlist,	/* Return list of supported hosts	*/
};

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)

static const PILPluginImports*  PluginImports;
static PILPlugin*       OurPlugin;
static PILInterface*    OurInterface;
static StonithImports*  OurImports;
static void*            interfprivate;

PIL_rc PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports);

PIL_rc PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports)
{
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;

	DEBUG_PRINT("lic_vps : start of function\n");

	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);

	/*  Register our interface implementation */
	return imports->register_interface( us
			, STONITH_TYPE_S	/* PIL_PLUGINTYPE_S */
			, PIL_PLUGIN_S
			, &lic_vpsOps
			, NULL     /* close */
			, &OurInterface
			, (void *)&OurImports
			, &interfprivate);
}


/*
 *	LIC_VPS STONITH device.
 */

struct pluginDevice
{
	StonithPlugin	sp;
	const char	*pluginid;
	char		*idinfo;
	char		*name;
	char		*compat;
	char		*config;
	struct	snipl_configuration *vpslist;
	int		hostcount;
};


static const char *pluginid    = DEVICE_ID;
static const char *NOTpluginID = DEVICE_ID " has been destroyed";

static const char NOTlic_vpsID[] = "has been destroyed (" DEVICE_ID ")";

/* XML definition for compat mode */
/* For now this plugin will only support the compatibility mode */
/* This means it works with the snipl configuration file or old parameter line */
/* nearly like the previous version of this stonith plugin */
/* Except that the first parameter specifies the mode (file|param|noncompat)  */

#include "stonith_config_xml.h"

#define LIC_COMPAT		"compat_mode"
#define LIC_CONFIG		"lic_config"
#define COMPAT_SNIPL_PARAM	"snipl_param"
#define COMPAT_SNIPL_FILE	"snipl_file"

#define XML_COMPAT_SHORTDESC \
	XML_PARM_SHORTDESC_BEGIN("en") \
	LIC_COMPAT \
	XML_PARM_SHORTDESC_END

#define XML_COMPAT_LONGDESC \
	XML_PARM_LONGDESC_BEGIN("en") \
	"Compatibility to snipl configuration file or param" \
	XML_PARM_LONGDESC_END

#define XML_CONFIG_SHORTDESC \
	XML_PARM_SHORTDESC_BEGIN("en") \
	LIC_CONFIG \
	XML_PARM_SHORTDESC_END

#define XML_CONFIG_LONGDESC \
	XML_PARM_LONGDESC_BEGIN("en") \
	"Configuration Data in snipl style via file or param" \
	"This is either a filename or the data itself"\
	XML_PARM_LONGDESC_END

#define XML_COMPAT_PARM \
	XML_PARAMETER_BEGIN(LIC_COMPAT, "string", "1") \
	XML_COMPAT_SHORTDESC \
	XML_COMPAT_LONGDESC \
	XML_PARAMETER_END

#define XML_CONFIG_PARM \
	XML_PARAMETER_BEGIN(LIC_CONFIG, "string", "1") \
	XML_CONFIG_SHORTDESC \
	XML_CONFIG_LONGDESC \
	XML_PARAMETER_END

static const char *lic_vpsXML =
  XML_PARAMETERS_BEGIN
	XML_COMPAT_PARM
	XML_CONFIG_PARM
  XML_PARAMETERS_END;

static void message(char **message_buffer)
{
	if (*message_buffer == NULL) {
		*message_buffer = strdup(" not available ");
	}
	syslog(LOG_ERR, "Detailed Error Description : %s\n", *message_buffer);

	free(*message_buffer);
	*message_buffer = NULL;
}


int lic_vps_status(StonithPlugin *s)
{
	struct pluginDevice        *vpsd;
	struct snipl_configuration *conf;
	struct snipl_server        *serv;
	_Bool server_found = 0;
	int cert_error;

	DEBUG_PRINT("lic_vps : start of function\n");

	ERRIFWRONGDEV(s, S_OOPS);

	vpsd = (struct pluginDevice *)s;

	conf = (struct snipl_configuration*)vpsd->vpslist;
	if (conf == NULL){
		syslog(LOG_ERR, "%s : snipl_configuration is NULL",
		      __func__);
		return S_OOPS;
	}

	snipl_for_each_server(conf, serv)
	{	/* search for one operational server */

		DEBUG_PRINT("server: %s\n", serv->address);
		if (serv->type == NULL)
			syslog(LOG_WARNING, "%s: No type specified for %s",
			       __func__, serv->address);
		else
			DEBUG_PRINT("%s: Preparing access for %s(type %s)\n",
			       __func__, serv->address, serv->type);

		snipl_prepare(serv);	/* prepare every server */
		if (!server_found) {
			/* but only check and try until one success */
			server_found = (0 == snipl_prepare_check(serv) &&
					0 == snipl_login(serv));
			cert_error = (serv->problem_class == CERTIFICATE_ERROR);
			snipl_logout(serv);
			if (!server_found &&
			    !strcasecmp(serv->type, "VM") &&
			    !cert_error) {
				serv->type = "VM5";
				snipl_prepare(serv);
				server_found =
					(0 == snipl_prepare_check(serv) &&
					 0 == snipl_login(serv));
				snipl_logout(serv);
			}
		}
	}
	if (server_found)
		return S_OK;

	return S_OOPS;
}

/*
 *	Return the list of hosts configured for lic_vps device
 */

static char** lic_vps_hostlist(StonithPlugin *s)
{
	struct pluginDevice *vpsd = (struct pluginDevice *)s;
	char**   hostlist = NULL;
	struct snipl_configuration *conf;
	struct snipl_server *serv;
	struct snipl_image *image;
	int x = 0;
	int numnames;

	DEBUG_PRINT("lic_vps : start of function\n");

	ERRIFWRONGDEV(s, NULL);

	conf = vpsd->vpslist;
	numnames = vpsd->hostcount;

	if ((numnames < 0)||(conf == NULL)) {
		syslog(LOG_ERR, "unconfigured stonith object in "
				"lic_vps_hostlist");
		return(NULL);
	}

	/* allocate space for hostlist (one more for end-condition NULL) */
	/* hostlist = (char **)calloc(numnames+1,sizeof(char*)); */

	/*
	 *use g_new or cl_malloc instead, or the check error happens when the
	 * caller free the memory with cl_free.
	 * hostlist = (char **)calloc(numnames+1,sizeof(char*));
	 */
	hostlist = (char **)g_new0(char*, numnames + 1);

	snipl_for_each_server(conf, serv) {
		snipl_for_each_image(serv, image) {
                                /* The caller will free it with cl_free, so
                                 * Duplicate it and use g_* functions.
                                 */
			hostlist[x++] = g_strdup(image->alias);
		}
	}

	return(hostlist);
}

/*
 * if hwmcaapi returnes a sequence error
 * we want to ignore this in some cases. This function checks
 * for some sequence error codes returned as part of the error
 * message and ignores them with returning success code 0
 */
int oops_error(char *errmsg, const char *request)
{
	char *rcstr;

	if (errmsg == 0)
		return 0;

	if ((rcstr = strstr(errmsg, "135921664")) ||
	    (rcstr = strstr(errmsg, "135921680"))) {
		syslog(LOG_ERR, "sequence error %s for %s is ignored\n",
		 rcstr, request);
		return 0;
	}
	if ((rcstr = strstr(errmsg, "Being Deactivated")) ||
	    (rcstr = strstr(errmsg, "Not Active")) ||
	    (rcstr = strstr(errmsg, "Already Active"))) {
		syslog(LOG_ERR, "Ignoring %s reply - complaining to be %s",
		       request, rcstr);
		return 0;
	}

	return 1;
}

/*
 *	reset/activate/deactivate the given image on this Stonith device.
 *
 */
static int
lic_vps_reset_req2(struct snipl_image *simg, int request)
{
	struct snipl_server *server;
	int rc = S_OK;

	server = simg->server;

	if (server->type == NULL) {
		syslog(LOG_ERR, "No type specified for %s", server->address);
		return S_OOPS;
	}

	rc = snipl_prepare(server);
	if (!rc)
		rc = snipl_prepare_check(server);
	if (rc) {
		syslog(LOG_ERR, "prepare error : %d using %s\n",
		       rc, server->address);
		message(&server->problem);
		return S_OOPS;
	}

	rc = snipl_login(server);
	if (rc && server->problem_class == CERTIFICATE_ERROR) {
		syslog(LOG_ERR, "Certificate fingerprint mismatch.\n");
		goto logout;
	}
	if (rc && !strcasecmp(server->type, "VM")) {
		snipl_logout(server);
		server->type = "VM5";
		if (!snipl_prepare(server)) {
			snipl_prepare_check(server);
			rc = snipl_login(server);
		}
	}
	if (rc) {
		syslog(LOG_ERR, "snipl_login error : %d using %s\n",
		       rc, server->address);
		message(&server->problem);
		return S_OOPS;
	}

	switch (request) {
	case ST_GENERIC_RESET:
		// reset image
		server->parms.image_op = RESET;
		rc = snipl_reset(simg);
		if (rc && oops_error(server->problem, "reset")) {
			syslog(LOG_ERR, "snipl_reset_error : %d using %s\n",
			       rc, simg->alias);
			message(&server->problem);
			return S_OOPS;
		}
		break;
	case ST_POWERON:
		// activate image
		server->parms.image_op = ACTIVATE;
		rc = snipl_activate(simg);
		if (rc && oops_error(server->problem, "activate")) {
			syslog(LOG_ERR, "snipl_activate error : %d using %s\n",
			       rc, simg->alias);
			message(&server->problem);
			return S_OOPS;
		}
		break;
	case ST_POWEROFF:
		// deactivate image
		server->parms.image_op = DEACTIVATE;
		server->parms.force = 1;
			// means immed in VM and unconditionally in LPAR
		rc = snipl_deactivate(simg);
		if (rc && oops_error(server->problem, "deactivate")) {
			syslog(LOG_ERR, "snipl_deactivate error : %d "
			       "using %s\n", rc, simg->alias);
			message(&server->problem);
			return S_OOPS;
		}
		break;
	default:
			syslog(LOG_ERR, "unknown request error\n");
			return S_OOPS;
		break;
	}

	DEBUG_PRINT(_("Host %s lic_vps-reset %d request successful\n"),
		simg->alias, request);

logout:
	if (server->problem_class == CERTIFICATE_ERROR)
		message(&server->problem);
	free(server->problem);
	rc = snipl_logout(server);
	if (rc) {
		syslog(LOG_ERR, "snipl_logout error : %d using %s\n",
		      rc, server->address);
		message(&server->problem);
	}

	return S_OK;
}


/*
 *	reset/activate/deactivate the given image on this Stonith device.
 *
 */
static int
lic_vps_reset_req(StonithPlugin *s, int request, const char* image_name)
{
	struct pluginDevice *vpsd = (struct pluginDevice *)s;
	struct snipl_configuration *conf;
	struct snipl_server *server = NULL;
	struct snipl_image  *simg;
	int rc = S_OK;

	DEBUG_PRINT("lic_vps_reset_req: request = %d image = %s\n",
		    request, image_name);

	ERRIFWRONGDEV(s,S_OOPS);

	conf = (struct snipl_configuration*)vpsd->vpslist;
	if (conf == NULL){
		syslog(LOG_ERR, "%s : snipl_configuration is NULL",
		      __func__);
		return S_OOPS;
	}

	do {
		// get snipl_server by image name:
		simg = find_next_image(conf, image_name, NULL, server);
		if (simg == NULL){
			syslog(LOG_ERR, "no server for image %s found\n",
			       image_name);
			return S_OOPS;
		}
		server = simg->server;
		rc = lic_vps_reset_req2(simg, request);
	} while (rc != S_OK);
	return rc;
}


/*
 *  Parse the config information
 */
static int
lic_vps_set_config(StonithPlugin* s, StonithNVpair *list)
{
	struct	pluginDevice* vpsd = (struct pluginDevice *)s;
	struct	snipl_configuration * conf;
	struct	snipl_server *serv;
	struct	snipl_image *image;
	int	numnames = 0;
	int	rc;
	char *used_cfgname;

//	static const char * names[] = {LIC_COMPAT, LIC_CONFIG, NULL};
	StonithNamesToGet	namestocopy [] =
	{	{LIC_COMPAT,	NULL}
	,	{LIC_CONFIG,	NULL}
	,	{NULL,		NULL}
	};

	DEBUG_PRINT("lic_vps : start of function\n");

	ERRIFWRONGDEV(s, S_OOPS);

	if (s->isconfigured) {
		return S_OOPS;
	}

	if ((rc = OurImports->CopyAllValues(namestocopy, list)) != S_OK) {
		return rc;
	}

	vpsd->compat = namestocopy[0].s_value;
	vpsd->config = namestocopy[1].s_value;

	if (0 == strcmp(vpsd->compat, COMPAT_SNIPL_PARAM)) {
		conf = snipl_configuration_from_line(vpsd->config);
	}
	else if (0 == strcmp(vpsd->compat, COMPAT_SNIPL_FILE)) {

		used_cfgname = get_config_file_name(vpsd->config);

		if (!used_cfgname){
			syslog(LOG_ERR, "Error : no configuration file could "
			       "be opened\n");
			return S_BADCONFIG;
		}

		// config file found
		conf = snipl_configuration_from_file(used_cfgname);
	}
	else {	// stonith native name value param passing
		//  not supported at the moment
		return S_BADCONFIG;
	}

	if (conf == NULL) {
		syslog(LOG_ERR, "Error reading configuration line\n");
		return S_BADCONFIG;
	} else if (conf->problem_class == FATAL) {
		syslog(LOG_ERR, "Fatal Error reading configuration line\n");
		syslog(LOG_ERR, "%s\n",conf->problem);
		return S_BADCONFIG;
	} else if (conf->problem_class == WARNING) {
		syslog(LOG_ERR, "Configuration line warning:\n");
		syslog(LOG_ERR, "%s\n",conf->problem);
	}

	vpsd->vpslist = conf;
	// count the images and store result in vpsd
	snipl_for_each_server(conf, serv) {
		snipl_for_each_image(serv, image) {
			++numnames;
		}
	}

	vpsd->hostcount = numnames;

	return S_OK;
}


/*
 *	return information about lic_vps device and its configuration,
 */
static const char *
lic_vps_getinfo(StonithPlugin * s, int reqtype)
{
	struct pluginDevice *vpsd = (struct pluginDevice *)s;
	static const char *ret;

	DEBUG_PRINT("lic_vps : start of function\n");

	ERRIFWRONGDEV(s,NULL);

	switch (reqtype) {
	case ST_DEVICEID:
		ret = vpsd->idinfo;
	break;
	case ST_DEVICENAME:
		ret = vpsd->name;
	break;
	case ST_DEVICEDESCR:
		ret = "Linux Image Control Virtual Power Switch          \n"
	"----------------------------------------------------------------\n"
	"controlling one or more Linux systems running under z/VM or     \n"
	"LPAR.                                                           \n"
	"                                                                \n"
	"Sample call (using a snipl configuration file) :                \n"
	"  stonith -t lic_vps -p \"snipl_file snipl.conf\" -T reset x    \n"
	"Sample call (using snipl parameter syntax) :                    \n"
	"  stonith -t lic_vps -p \"snipl_param server=boet2930,type=vm\\ \n"
	"   ,user=t2930043,password=passw0rd,port=123,image=t2930043\"\\ \n"
	"   -T reset t2930043                                            \n"
	"                                                                \n"
	"The name-value pair passed with -p to stonith (and lic_vps)     \n"
	"  is either snipl_file <name of the snipl configuration file>   \n"
	"  or snipl_param <configuration in one comma-separated string>  \n"
	"The -F config file option of stonith is not supported - use     \n"
	"  the snipl_file parameter option instead.                      \n"
	"Sample configuration file:                                      \n"
	"                                                                \n"
	"------------------------------                                  \n"
	" Server = 9.164.70.100                                          \n"
	" type = lpar                                                    \n"
	" user = daisy                                                   \n"
	" password = duck                                                \n"
	"   image = lparname1/ipalias1                                   \n"
	"   image = lparname2/ipalias2                                   \n"
	"   image = lparname3/ipalias3                                   \n"
	"                                                                \n"
	" Server = duckburg                                              \n"
	" type = VM                                                      \n"
	" user = donald                                                  \n"
	" password = duck                                                \n"
	" port = 12345                                                   \n"
	"   image = vmlnx1                                               \n"
	"   image = vmlnx2                                               \n"
	"   image = vmlnx3                                               \n"
	"   image = vmlnx4                                               \n"
	"------------------------------                                  \n"
	"                                                                \n"
	" For LPARs :                                                    \n"
	" The password is the community-password as needed by hwmcaapi.  \n"
	" The image name can be specified with an alias (as above)       \n"
	" to support different naming of LPARs and the hostname of its   \n"
	" image. An alias may also be specified for VM images - although \n"
	" this is normally not needed.                                   \n"
	" General image specification is : image = <name>[/<alias>]      \n"
	"\n";
	break;
	case ST_CONF_XML:		/* XML metadata */
		ret = lic_vpsXML;
		break;
	default:
		ret = NULL;
	break;
	}
	return ret;
}

static const char* const*
lic_vps_get_confignames(StonithPlugin* p)
{
	static const char * names[] = {LIC_COMPAT, LIC_CONFIG, NULL};

	DEBUG_PRINT("lic_vps : start of function\n");
	if (Debug) {
		LOG(PIL_DEBUG, "%s: called.", __func__);
	}
	return names;
}


/*
 *	lic_vps Stonith destructor
struct pluginDevice
{
	StonithPlugin	sp;
	const char*	pluginid;
	char*		idinfo;
	char*		name;
	char*		compat;
	char*		config;
	struct snipl_configuration *vpslist;
	int		hostcount;
 */
static void lic_vps_destroy(StonithPlugin *s)
{
	struct pluginDevice *vpsd = (struct pluginDevice *)s;

	DEBUG_PRINT("lic_vps : start of function\n");

	VOIDERRIFWRONGDEV(s);

	vpsd->pluginid = NOTpluginID;
	if (vpsd->vpslist){
		snipl_configuration_free(vpsd->vpslist);
		vpsd->vpslist = NULL;
	}
	if (vpsd->config) {
		FREE(vpsd->config);
		vpsd->config = NULL;
	}
	if (vpsd->compat) {
		FREE(vpsd->compat);
		vpsd->compat = NULL;
	}
	vpsd->hostcount = -1;
	FREE(vpsd);
}


/* Create a new LIC_VPS Stonith device.*/
/*
 * returns a pointer to the structure pluginDevice which is
 *  then stored in the pinfo field of Stonith struct
 *
 */
static StonithPlugin *	lic_vps_new(const char *subplugin)
{
	struct pluginDevice* vpsd = MALLOCT(struct pluginDevice);

	DEBUG_PRINT("lic_vps : start of function\n");

	if (vpsd == NULL){
		syslog(LOG_ERR, "out of memory");
		return(NULL);
	}
	memset(vpsd, 0, sizeof(*vpsd));
	vpsd->pluginid  = pluginid;
	vpsd->idinfo	= DEVICE;
	vpsd->hostcount = -1;
	vpsd->sp.s_ops = &lic_vpsOps;
	return &(vpsd->sp);
}

