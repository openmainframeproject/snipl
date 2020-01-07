/*
 * snipl_stonith_plugin.h - common definitions for stonith plugin
 *
 * Copyright IBM Corp. 2002, 2011
 * Author(s): Peter Tiedemann    (ptiedem@de.ibm.com)
 * Maintainer: Ursula Braun      (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
   PLEASE NOTE:
   snIPL is provided under the terms of the enclosed common public license
   ("agreement"). Any use, reproduction or distribution of the program
   constitutes recipient's acceptance of this agreement.
 */
#ifndef _SNIPL_STONITH_PLUGIN_H
#define _SNIPL_STONITH_PLUGIN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#include <stonith/stonith.h>
#include <stonith/stonith_plugin.h>

#define LOG(w...)	PILCallLog(PluginImports->log, w)

#define MALLOC		PluginImports->alloc
#define REALLOC		PluginImports->mrealloc
#define STRDUP		(PluginImports->mstrdup)
#define FREE		PluginImports->mfree
#define EXPECT_TOK	OurImports->ExpectToken
#define STARTPROC	OurImports->StartProcess

#ifndef MALLOCT
#	define     MALLOCT(t)      ((t *)(MALLOC(sizeof(t))))
#endif

#define N_(text)	(text)
#define _(text)		dgettext(ST_TEXTDOMAIN, text)

#define PIL_PLUGINTYPE          STONITH_TYPE
#define PIL_PLUGINTYPE_S        STONITH_TYPE_S

#define	ISCORRECTDEV(i)	((i)!= NULL				\
	&& ((struct pluginDevice *)(i))->pluginid == pluginid)

#define ERRIFWRONGDEV(s, retval) if (!ISCORRECTDEV(s)) { \
    LOG(PIL_CRIT, "%s: invalid argument", __FUNCTION__); \
    return(retval); \
  }

#define VOIDERRIFWRONGDEV(s) if (!ISCORRECTDEV(s)) { \
    LOG(PIL_CRIT, "%s: invalid argument", __FUNCTION__); \
    return; \
  }

#define	ISCONFIGED(i)	(i->isconfigured)

#define ERRIFNOTCONFIGED(s,retval) ERRIFWRONGDEV(s,retval); \
    if (!ISCONFIGED(s)) { \
    LOG(PIL_CRIT, "%s: not configured", __FUNCTION__); \
    return(retval); \
  }

#define VOIDERRIFNOTCONFIGED(s) VOIDERRIFWRONGDEV(s); \
    if (!ISCONFIGED(s)) { \
    LOG(PIL_CRIT, "%s: not configured", __FUNCTION__); \
    return; \
  }

#endif

