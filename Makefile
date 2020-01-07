#
#  snIPL - Linux Image Control for LPAR/VM
#
#  Copyright IBM Corp. 2002, 2016
#  Author(s): Peter Tiedemann (ptiedem@de.ibm.com)
#  Maintainer: Ursula Braun   (ursula.braun@de.ibm.com)
#
#  Published under the terms and conditions of the CPL (common public license)
#
#  PLEASE NOTE:
#  snIPL is provided under the terms of the enclosed common public license
#  ("agreement"). Any use, reproduction or distribution of the program
#  constitutes recipient's acceptance of this agreement.
#
LIB_STRING = lib
ifeq ($(shell uname -m),s390x)
LIB_STRING = lib64
endif
ifeq ($(shell uname -m),x86_64)
LIB_STRING = lib64
endif

CC              = $(CROSS_COMPILE)gcc
STRIP           = $(CROSS_COMPILE)strip

PREFIX		= /usr
BINDIR          = ${PREFIX}/bin
LIBDIR          = ${PREFIX}/${LIB_STRING}
PREREQLIBDIR    = /usr/${LIB_STRING}
STONITHLIBDIR	= ${PREFIX}/${LIB_STRING}/stonith
INCDIR          = ${PREFIX}/include
STONITHINCDIR   = ${INCDIR}/stonith
HEARTBEATINCDIR = ${INCDIR}/heartbeat
SHAREDIR        = /usr/share
MANDIR          = ${PREFIX}/share/man
LIB_LPAR        =
LIB_VM          =
LIB_ZVM         = -lvmsmapi6
LPAR_INCLUDED   =
VM_INCLUDED     =
OBJ_LPAR        =
OBJ_VM          =

OWNER           = $(shell id -un)
GROUP           = $(shell id -gn)

GLIB2_HEADERS   = `pkg-config --cflags glib-2.0`
CFLAGS  += -DUNIX=1 -DST_TEXTDOMAIN='"stonith"' -g -O2 -Wall -I. -I$(INCDIR) -I$(STONITHINCDIR) -I$(HEARTBEATINCDIR) -D_FORTIFY_SOURCE=2

all: all_snconfig all_sniplapi all_vmsmapi all_snipl all_sncap all_stonith
install: install_subdirs install_snconfig install_sniplapi install_vmsmapi install_snipl install_sncap install_stonith
uninstall: uninstall_snconfig uninstall_sniplapi uninstall_vmsmapi uninstall_snipl uninstall_sncap uninstall_stonith uninstall_subdirs
#INSTALL_FLAGS = -g $(GROUP) -o $(OWNER) -m755 -s
INSTALL_FLAGS = -g $(GROUP) -o $(OWNER) -m755

install_subdirs:
	install -d -m755 $(STONITHLIBDIR)/plugins/stonith2
	install -d -m755 $(LIBDIR) $(BINDIR) $(MANDIR)/man8

uninstall_subdirs:
	rmdir --ignore-fail-on-non-empty $(STONITHLIBDIR)/plugins/stonith2
	rmdir --ignore-fail-on-non-empty $(LIBDIR) $(BINDIR) $(MANDIR)/man8

clean:
	rm -f snipl
	rm -f sncap
	rm -f lib*.so
	rm -f dmsvsma*.c dmsvsma*.h dmsvsma.x
	rm -f core *.o *.lo *.la .libs/lic_vps.* .libs/prepare.*

# Targets

all_snconfig : libsnconfig.so

snconfig.o: snconfig.c snipl.h
	$(CC) $(CFLAGS) -c -fPIC snconfig.c

libsnconfig.so: snconfig.o
	$(LINK.c) -o $@ -shared snconfig.o -ldl

install_snconfig:
	install $(INSTALL_FLAGS) libsnconfig.so $(LIBDIR)

uninstall_snconfig:
	rm -f $(LIBDIR)/libsnconfig.so


ifeq ($(shell if [ -f $(PREREQLIBDIR)/libhwmcaapi.so ]; \
		then echo ok; fi),ok)

LIB_LPAR = -lsniplapi -lhwmcaapi
LPAR_INCLUDED = -DLPAR_INCLUDED=1
OBJ_LPAR = libsniplapi.so libsnconfig.so

all_sniplapi: libsniplapi.so

libsniplapi.so: sniplapi.o
	$(LINK.c) -o $@ -shared sniplapi.o -lhwmcaapi

sniplapi.o: sniplapi.c snipl.h
	$(CC) $(CFLAGS) -c -fPIC sniplapi.c

install_sniplapi: libsniplapi.so
	install $(INSTALL_FLAGS) libsniplapi.so $(LIBDIR)

uninstall_sniplapi: libsniplapi.so
	rm -f $(LIBDIR)/libsniplapi.so

else

all_sniplapi:
	@echo "************************************************************"
	@echo "***                      WARNING                         ***"
	@echo "***                                                      ***"
	@echo "*** snIPL cannot be built completely                     ***"
	@echo "***                       without the hwmcaapi library   ***"
	@echo "************************************************************"
install_sniplapi: all_sniplapi

endif

# DMSVSMA_DIR=/usr/share
ifeq ($(origin DMSVSMA_DIR), undefined)
DMSVSMA_DIR = $(SHAREDIR)/dmsvsma
endif

LIBSSL = ${PREREQLIBDIR}/libssl.so
DMSVSMAX = $(DMSVSMA_DIR)/dmsvsma.x
ifeq ($(WITHVMOLD),1)
VM_ENVIRONMENT = $(shell if [ -f $(LIBSSL) ] && [ -f $(DMSVSMAX) ]; then echo 1; else echo 0; fi)
endif
SSL_INSTALLED = $(shell if [ -f $(LIBSSL) ]; then echo 1; else echo 0; fi)

ifeq ($(VM_ENVIRONMENT),1)

TARGETS_CLNT.c = dmsvsma_clnt.c dmsvsma_xdr.c vmsmapi.c
TARGETS = dmsvsma_clnt.h dmsvsma_clnt.c dmsvsma_xdr.c
OBJECTS_CAPI = vmsmapi.o dmsvsma_clnt.o dmsvsma_xdr.o
LIB_VM = -lvmsmapi
VM_INCLUDED = -DVM_INCLUDED=1
OBJ_VM = libvmsmapi.so

# Targets

all_vmsmapi: libvmsmapi.so libvmsmapi6.so

$(TARGETS): $(DMSVSMA_DIR)/dmsvsma.x
	cp -vp $(DMSVSMA_DIR)/dmsvsma.x .
	rpcgen dmsvsma.x

$(OBJECTS_CAPI) : snipl.h vmsmapi.h $(TARGETS_CLNT.c)
	$(CC) $(CFLAGS) -c -fPIC -Wno-unused  -fno-strict-aliasing $(TARGETS_CLNT.c)
vmsmapi6.o : snipl.h vmsmapi6.h vmsmapi6.c
	$(CC) $(CFLAGS) -c -fPIC -Wno-unused  -fno-strict-aliasing vmsmapi6.c

libvmsmapi.so: dmsvsma_clnt.o dmsvsma_xdr.o vmsmapi.o
	$(LINK.c) -shared -o $@ dmsvsma_clnt.o dmsvsma_xdr.o vmsmapi.o -lnsl
libvmsmapi6.so: vmsmapi6.o
	$(LINK.c) -shared -o $@ vmsmapi6.o -lnsl -lssl -lcrypto


install_vmsmapi:
	install $(INSTALL_FLAGS) libvmsmapi.so $(LIBDIR)
	install $(INSTALL_FLAGS) libvmsmapi6.so $(LIBDIR)

uninstall_vmsmapi:
	rm -f $(LIBDIR)/libvmsmapi.so
	rm -f $(LIBDIR)/libvmsmapi6.so

else

ifeq ($(SSL_INSTALLED),0)
LIB_ZVM =
all_vmsmapi:
	@echo "************************************************************"
	@echo "***                      WARNING                         ***"
	@echo "***                                                      ***"
	@echo "*** vmsmapi cannot be built without OpenSSL              ***"
	@echo "*** z/VM socket-based servers are not supported          ***"
	@echo "*** z/VM RPC-based servers are not supported             ***"
	@echo "************************************************************"
install_vmsmapi: all_vmsmapi
else
all_vmsmapi: libvmsmapi6.so
ifeq ($(WITHVMOLD),1)
	@echo "************************************************************"
	@echo "***                      WARNING                         ***"
	@echo "***                                                      ***"
	@echo "*** vmsmapi cannot be built without dmsvsma.x            ***"
	@echo "*** z/VM socket-based servers are supported              ***"
	@echo "*** z/VM RPC-based servers are not supported             ***"
	@echo "************************************************************"
endif

vmsmapi6.o : snipl.h vmsmapi6.h vmsmapi6.c
	$(CC) $(CFLAGS) -c -fPIC -Wno-unused  -fno-strict-aliasing vmsmapi6.c

libvmsmapi6.so: vmsmapi6.o
	$(LINK.c) -shared -o $@ vmsmapi6.o -lnsl -lssl -lcrypto

install_vmsmapi:
	install $(INSTALL_FLAGS) libvmsmapi6.so $(LIBDIR)

uninstall_vmsmapi:
	rm -f $(LIBDIR)/libvmsmapi6.so

endif

endif


all_snipl:  snipl.o prepare.o $(OBJ_VM) $(OBJ_LPAR)
	$(LINK.c) -o snipl -L. -L${LIBDIR} snipl.o prepare.o -lnsl -ldl -lsnconfig $(LIB_VM) $(LIB_ZVM) $(LIB_LPAR)

snipl.o: snipl.h snipl.c
	$(CC) $(CFLAGS) -Wno-unused $(LPAR_INCLUDED) $(VM_INCLUDED) -c snipl.c

prepare.o: snipl.h prepare.c
	$(CC) $(CFLAGS) -c prepare.c

install_snipl:
	install $(INSTALL_FLAGS) snipl $(BINDIR)
	install -g $(GROUP) -o $(OWNER) -m644 snipl.8 $(MANDIR)/man8

uninstall_snipl:
	rm -f $(BINDIR)/snipl
	rm -f $(MANDIR)/man8/snipl.8


ifeq ($(shell if [ -f $(STONITHINCDIR)/stonith_plugin.h ] || [ -f /usr/include/stonith/stonith_plugin.h ]; \
	then echo ok; fi),ok)

all_stonith: lic_vps.la

install_stonith: lic_vps.la
	$(SHELL) $(BINDIR)/libtool --mode=install $(BINDIR)/install -c lic_vps.la \
       	$(STONITHLIBDIR)/plugins/stonith2/lic_vps.la

uninstall_stonith: lic_vps.la
	rm -f $(STONITHLIBDIR)/plugins/stonith2/lic_vps.la
	rm -f $(STONITHLIBDIR)/plugins/stonith2/lic_vps.a
	rm -f $(STONITHLIBDIR)/plugins/stonith2/lic_vps.so

lic_vps.la: lic_vps.lo prepare.lo
	$(SHELL) $(BINDIR)/libtool --mode=link gcc -o $@ \
    	-rpath ${STONITHLIBDIR}/plugins/stonith2 \
	-export-dynamic -module -avoid-version lic_vps.lo prepare.lo -L. -lc -lrt -ldl -lnet \
        $(LIB_VM) $(LIB_ZVM) $(LIB_LPAR) -lsnconfig $(GLIB2_HEADERS)

lic_vps.lo: lic_vps.c snipl.h
	$(SHELL) $(BINDIR)/libtool --mode=compile gcc \
	$(CFLAGS) $(LPAR_INCLUDED) $(VM_INCLUDED) $(GLIB2_HEADERS) \
	-c lic_vps.c -o $@

prepare.lo: prepare.c snipl.h
	$(SHELL) $(BINDIR)/libtool --mode=compile gcc $(CFLAGS) \
	$(LPAR_INCLUDED) $(VM_INCLUDED) $(GLIB2_HEADERS) \
	-c prepare.c -o $@

else


all_stonith:
	@echo "************************************************************"
	@echo "***                      WARNING                         ***"
	@echo "***                                                      ***"
	@echo "*** some stonith include files are needed to build the   ***"
	@echo "*** snipl stonith plugin lic_vps but could not be found. ***"
	@echo "*** Stonith is part of the heartbeat package which       ***"
	@echo "*** should be installed first.                           ***"
	@echo "*** Read the README.snipl... files                       ***"
	@echo "************************************************************"
install_stonith: all_stonith

endif

ifeq ($(shell if [ -f $(PREREQLIBDIR)/libhwmcaapi.so ]; \
		then echo ok; fi),ok)

all_sncap: sncap.o sncapjob.o sncaputil.o sncapconf.o sncapdsm.o sncapapi.o \
	sncaptcr.o sncapcpc.o
	$(LINK.c) -o sncap sncap.o sncapjob.o \
		sncaputil.o sncapconf.o sncapdsm.o sncapapi.o sncaptcr.o \
		sncapcpc.o -lhwmcaapi

sncap.o: sncap.c sncap.h sncaputil.h sncapjob.h
	$(CC) $(CFLAGS) -c sncap.c

sncapjob.o: sncapjob.c sncapjob.h sncaputil.h sncapconf.h
	$(CC) $(CFLAGS) -c -Wno-unused sncapjob.c

sncaputil.o: sncaputil.c sncaputil.h
	$(CC) $(CFLAGS) -c sncaputil.c

sncapconf.o: sncapconf.c sncapconf.h sncaputil.h
	$(CC) $(CFLAGS) -c sncapconf.c

sncapdsm.o: sncapdsm.c sncapdsm.h
	$(CC) $(CFLAGS) -c sncapdsm.c

sncapapi.o: sncapapi.c sncapapi.h sncaputil.h sncaptcr.h sncapjob.h \
	sncapcpc.h
	$(CC) $(CFLAGS) -c sncapapi.c

sncaptcr.o: sncaptcr.c sncaptcr.h sncaputil.h sncapdsm.h sncapjob.h
	$(CC) $(CFLAGS) -c sncaptcr.c

sncapcpc.o: sncapcpc.c sncapcpc.h
	$(CC) $(CFLAGS) -c sncapcpc.c

install_sncap:
	install $(INSTALL_FLAGS) sncap $(BINDIR)
	install -g $(GROUP) -o $(OWNER) -m644 sncap.8 $(MANDIR)/man8

uninstall_sncap:
	rm -f $(BINDIR)/sncap
	rm -f $(MANDIR)/man8/sncap.8

else

all_sncap:
	@echo "************************************************************"
	@echo "***                      WARNING                         ***"
	@echo "***                                                      ***"
	@echo "*** sncap cannot be built without hwmcaapi library       ***"
	@echo "***                                                      ***"
	@echo "************************************************************"
install_sncap: all_sncap

endif
