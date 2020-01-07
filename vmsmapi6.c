/*
 * vmsmapi6.c : vm API for snIPL (simple network IPL)
 *              for socket-based SMAPI environment
 *
 * Copyright IBM Corp. 2011, 2016
 * Author(s): Ursula Braun    (ursula.braun@de.ibm.com)
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "snipl.h"
#include "vmsmapi6.h"

const char *vmsmapi6_get_error_description(int rc, int rs)
{
	unsigned int i;
	char *err;

	for (i = 0; i < DIMOF(error_codes); i++) {
		if (error_codes[i].RC != rc)
			continue;
		if (error_codes[i].RS == rs || error_codes[i].RS == RS_ANY)
			return error_codes[i].desc;
	}
	err = strdup("");
	if (asprintf(&err, "VM rc/rs = %d/%d", rc, rs) == -1)
		err = NULL;

	return err;
}


extern void prepare(struct snipl_server *server)
{
	struct snipl_image *image;

	DEBUG_PRINT("vmsmapi6 : start of function\n");

	server->ops = &vm6_server_ops;
	snipl_for_each_image(server, image)
		image->ops = &vm6_image_ops;

	return;
}

static int vm6_check(struct snipl_server *server)
{
	int rc;

	DEBUG_PRINT("vmsmapi6 : start of function\n");
	server->priv = calloc(1, sizeof(*server->priv));
	if (server->priv == NULL) {
		server->problem =
			strdup("cannot allocate buffer for "
			       "snipl_server_private\n");
		server->problem_class = FATAL;
		return STORAGE_PROBLEM;
	}

	server->priv->inlist = (char *)calloc(INPUT_LEN, 1);
	if (!server->priv->inlist) {
		server->problem = strdup("cannot allocate inlist buffer\n");
		server->problem_class = FATAL;
		return STORAGE_PROBLEM;
	}
	server->priv->outlist =
		(char *)calloc(sizeof(struct vm6_image_response), 1);
	if (!server->priv->outlist) {
		server->problem = strdup("cannot allocate outlist buffer\n");
		server->problem_class = FATAL;
		return STORAGE_PROBLEM;
	}

	rc = parms_check_vm(server);
	if (rc)
		return rc;

	if (!server->timeout)
		server->timeout = 60000;

	if ((server->parms.force == -1) && !server->parms.shutdown_time)
		server->parms.shutdown_time = 300;

	return 0;
}

static int vm6_get_cert_attributes(struct snipl_server *server,
				   char **fingerprint)
{
	unsigned char sha[EVP_MAX_MD_SIZE];
	X509 *cert;
	X509_NAME *certname;
	int i;
	unsigned int n;
	int pos;
	char *buffer = NULL;

	cert = SSL_get_peer_certificate(server->priv->sslhandle);
	if (!cert) {
		create_msg(server,
			   "%s: failed to get server certificate\n",
			   server->address);
		server->problem_class = CERTIFICATE_ERROR;
		return 1;
	}
	DEBUG_PRINT("vmsmapi6 : ssl got certificate\n");
	certname = X509_get_subject_name(cert);
	if (!certname) {
		create_msg(server,
			   "%s: failed to get name from server certificate\n",
			   server->address);
		server->problem_class = CERTIFICATE_ERROR;
		return 2;
	}

	/* calculate & print fingerprint */
	if (!X509_digest(cert, EVP_get_digestbyname("sha256"), sha, &n)) {
		create_msg(server,
			   "%s: failed to get certificate ssl fingerprint\n",
			   server->address);
		server->problem_class = CERTIFICATE_ERROR;
		return 3;
	}
	*fingerprint = calloc(3 * n, sizeof(char));
	buffer = *fingerprint;
	pos = 0;
	for (i = 0; i < n - 1; i++) {
		sprintf(&buffer[pos], "%02x%c", sha[i], ':');
		pos += 3;
	}
	sprintf(&buffer[pos], "%02x", sha[i]);

	return 0;
}

/*
 * Propmt the user confirmation for SSL connection.
 */
static int vm6_confirm(struct snipl_server *server)
{
	char *peer_fingerprint = NULL;
	int ret = 0;
	char yes[5];

	if (!server->enc)
		return 1;

	if (vm6_get_cert_attributes(server, &peer_fingerprint))
		return -1;

	fprintf(stderr, "sslfingerprint = %s\n", peer_fingerprint);
	fprintf(stderr, "Do you accept the server certificate? (type yes)\n");
	fprintf(stderr, "(sslfingerprints can be added to snipl config file ");
	fprintf(stderr, "to prevent this question)\n");

	memset(yes, '\0', sizeof(yes));
	if (!fgets(yes, sizeof(yes) - 1, stdin)) {
		server->problem_class = CERTIFICATE_ERROR;
		ret = 0;
		goto out;
	}
	if (!strcasecmp("yes", yes)) {
		ret = 1;
		goto out;
	}
	printf("Certificate was not accepted\n");
	server->problem_class = CERTIFICATE_ERROR;
out:
	free(peer_fingerprint);
	return ret;
}

int vm6_wait_for_response(struct snipl_server *server, char *fname_print,
			  unsigned int in_or_out, int timeout)
{
	int epoll;
	struct epoll_event event;
	int rc = 0;

	DEBUG_PRINT("vmsmapi6 : start of function\n");
	event.events = in_or_out;
	event.data.ptr = NULL;
	epoll = epoll_create(1);
	rc = epoll_ctl(epoll, EPOLL_CTL_ADD, server->priv->sockid, &event);
	if (rc < 0) {
		DEBUG_PRINT("epoll_ctl return_code = %08x = %i\n", rc, rc);
		create_msg(server,
			"%s: %s failed, return_code of epoll_ctl is %i %s\n",
			server->address, fname_print, errno, strerror(errno));
		server->problem_class = FATAL;
		goto out;
	}
	rc = epoll_wait(epoll, &event, 1, timeout);
	if (rc < 0) {
		DEBUG_PRINT("epoll_wait return_code = %08x = %i\n", rc, rc);
		create_msg(server,
			"%s: %s failed, return_code of epoll_wait is %i %s\n",
			server->address, fname_print, errno, strerror(errno));
		server->problem_class = FATAL;
	} else if (rc == 0) {
		DEBUG_PRINT("epoll_wait timeout reached\n");
		create_msg(server, "%s: %s timed out\n",
			server->address, fname_print);
		server->problem_class = FATAL;
		rc = -ETIME;
	} else {
		rc = 0;
		if (!(event.events & in_or_out)) {
			if (event.events & EPOLLHUP) {
				DEBUG_PRINT("epoll_wait returned EPOLLHUP\n");
				rc = -ENOTCONN;
			}
			if (event.events & EPOLLERR) {
				DEBUG_PRINT("epoll_wait returned EPOLLERR\n");
				rc = -EIO;
			}
		}
	}
out:
	close(epoll);
	return rc;
}

/*--------------------------------------------------------------------*/
static char *vm6_handle_force(struct snipl_server *server, char *tmp)
{
	char *force_time;
	char FORCE_IMMED[6] = "IMMED\0";
	char FORCE_WITHIN[7] = "WITHIN ";
	char sd_time[13];
	uint32_t len;

	if (server->parms.force == 1)
		force_time = FORCE_IMMED;
	else {
		memset(sd_time, 0, sizeof(sd_time));
		memcpy(sd_time, FORCE_WITHIN, sizeof(FORCE_WITHIN));
		sprintf(sd_time + sizeof(FORCE_WITHIN),
			"%d", server->parms.shutdown_time);
		force_time = sd_time;
	}
	len = strlen(force_time); /* force_time_length */
	*((uint32_t *)tmp) = htonl(len);
	DEBUG_PRINT("force_time_length = %08x = %u\n",
		*((uint32_t *)tmp), *((uint32_t *)tmp));
	tmp = (char *)((unsigned long)tmp + 4);
	memcpy(tmp, force_time, strlen(force_time)); /* force_time */
	DEBUG_PRINT("force_time = %s\n", tmp);
	tmp = (char *)((unsigned long)tmp + strlen(force_time));
	return tmp;
}

/*--------------------------------------------------------------------*/
static int vm6_build_input(struct snipl_image *image, char *fname)
{
	struct snipl_server *server = image->server;
	char *tmp;
	uint32_t len;

	memset(server->priv->inlist, 0, INPUT_LEN);
	tmp = (char *)((unsigned long)server->priv->inlist + 4);
	/* add function name */
	len = strlen(fname); /* function_name_length */;
	*((uint32_t *)tmp) = htonl(len);
	DEBUG_PRINT("function_name_length = %08x = %u\n",
		*((uint32_t *)tmp), *((uint32_t *)tmp));
	tmp = (char *)((unsigned long)tmp + 4);
	memcpy(tmp, fname, strlen(fname)); /* function_name */
	DEBUG_PRINT("function_name = %s\n", tmp);
	tmp = (char *)((unsigned long)tmp + strlen(fname));
	/* add authenticated_userid */
	len = strlen(server->user); /* authenticated_userid_length */
	*((uint32_t *)tmp) = htonl(len);
	DEBUG_PRINT("userid_length = %08x = %u\n",
		*((uint32_t *)tmp), *((uint32_t *)tmp));
	tmp = (char *)((unsigned long)tmp + 4);
	memcpy(tmp, server->user, strlen(server->user)); /* auth._userid */
	DEBUG_PRINT("userid = %s\n", tmp);
	tmp = (char *)((unsigned long)tmp + strlen(server->user));
	/* add password */
	len = strlen(server->password); /* password_length */
	*((uint32_t *)tmp) = htonl(len);
	DEBUG_PRINT("password_length = %08x = %u\n",
		*((uint32_t *)tmp), *((uint32_t *)tmp));
	tmp = (char *)((unsigned long)tmp + 4);
	memcpy(tmp, server->password, strlen(server->password)); /* password */
	DEBUG_PRINT("password = %s\n", tmp);
	tmp = (char *)((unsigned long)tmp + strlen(server->password));
	/* add target identifier */
	len = strlen(image->name); /* target_identifier_length */
	*((uint32_t *)tmp) = htonl(len);
	DEBUG_PRINT("target_identifier_length = %08x = %u\n",
		*((uint32_t *)tmp), *((uint32_t *)tmp));
	tmp = (char *)((unsigned long)tmp + 4);
	memcpy(tmp, image->name, strlen(image->name)); /* target identifier */
	DEBUG_PRINT("target_identifier = %s\n", tmp);
	tmp = (char *)((unsigned long)tmp + strlen(image->name));
	/* add force_time */
	if (!strcmp(fname, "Image_Deactivate\0"))
		tmp = vm6_handle_force(server, tmp);
	/* insert total_length */
	len = (unsigned long)tmp - (unsigned long)server->priv->inlist - 4;
	tmp = (char *)((unsigned long)server->priv->inlist);
	*((uint32_t *)tmp) = htonl(len);
	DEBUG_PRINT("total_length = %08x = %u\n", *((uint32_t *)tmp),
		*((uint32_t *)tmp));
	return len;
}

/*--------------------------------------------------------------------*/
int vm6_recv_all(struct snipl_server *server, char *fname, int len)
{
	int total = 0;
	int bytesleft = len;
	int rc = 0;
	struct vm6_image_response *resp_hdr;

	memset(server->priv->outlist, 0, sizeof(struct vm6_image_response));
	while (total < len) {
		if (server->enc)
			rc = SSL_read(server->priv->sslhandle,
				      server->priv->outlist, bytesleft);
		else
			rc = recv(server->priv->sockid, server->priv->outlist,
				  bytesleft, 0);
		if (rc < 0) {
			if ((!server->enc && errno == EAGAIN) ||
			    SSL_get_error(server->priv->sslhandle, rc) ==
			    SSL_ERROR_WANT_READ) {
				rc = vm6_wait_for_response(server, fname,
						EPOLLIN, server->timeout);
				if (!rc)
					continue;
			}
			break;
		} else if (!rc) {
			rc = -ENOTCONN;
			break;
		}
		total += rc;
		bytesleft -= rc;
	}

	// Convert all integer values
	resp_hdr = (struct vm6_image_response *)server->priv->outlist;
	resp_hdr->output_length=ntohl(resp_hdr->output_length);
	resp_hdr->request_id=ntohl(resp_hdr->request_id);
	resp_hdr->return_code=ntohl(resp_hdr->return_code);
	resp_hdr->reason_code=ntohl(resp_hdr->reason_code);
	resp_hdr->processed=ntohl(resp_hdr->processed);
	resp_hdr->not_processed=ntohl(resp_hdr->not_processed);
	resp_hdr->failing_array_length=ntohl(resp_hdr->failing_array_length);

	return rc ? rc : total;
}

/*--------------------------------------------------------------------*/
int vm6_command_handling(struct snipl_image *image, char *fname)
{
	struct snipl_server *server = image->server;
	char *tmp;
	int request_id;
	int inlen;
	int total = 0;
	int bytesleft;
	int rc = 0;
	struct vm6_image_response *resp_hdr;
	const char *err_dsc;
	char fname_print[18] = "Image\0";

	DEBUG_PRINT("vmsmapi6 : start of function\n");
	tmp = strchr(fname, '_') + 1;
	memcpy(&fname_print[5], tmp, strlen(fname) - 6);
	tmp = memchr(fname_print, '_', 16);
	if (tmp) {
		memmove(tmp, tmp + 1,
			strlen(fname_print) + fname_print - tmp - 1);
		fname_print[strlen(fname_print) - 1] = '\0';
	}

	inlen = vm6_build_input(image, fname) + 4;
	bytesleft = inlen;
	while (total < inlen) {
		if (server->enc)
			rc = SSL_write(server->priv->sslhandle,
				       server->priv->inlist + total, bytesleft);
		else
			rc = send(server->priv->sockid,
				  server->priv->inlist + total, bytesleft, 0);
		if (rc < 0) {
			if (((!server->enc) && errno == EAGAIN) ||
			    (server->enc &&
			     SSL_get_error(server->priv->sslhandle, rc) ==
			     SSL_ERROR_WANT_WRITE)) {
				rc = vm6_wait_for_response(server, fname_print,
						EPOLLOUT, server->timeout);
				if (!rc)
					continue;
			}
			break;
		}
		total += rc;
		bytesleft -= rc;
	}
	if (rc < 0) {
		DEBUG_PRINT("send return_code = %08x = %i\n", rc, rc);
		create_msg(server,
			   "%s: %s failed, return_code of send is %i %s\n",
			   image->name, fname_print, errno, strerror(errno));
		server->problem_class = FATAL;
		return rc;
	}

	rc = vm6_wait_for_response(server, fname_print, EPOLLIN,
				   server->timeout);
	if (rc)
		return rc;
	/* Receive the request id */
	rc = vm6_recv_all(server, fname_print, 4);
	if (rc < 0) {
		DEBUG_PRINT("recv return_code = %08x = %i\n", rc, rc);
		create_msg(server,
			"%s: %s failed, return_code of recv request_id "
			"is %i %s\n",
			image->name, fname_print, errno, strerror(errno));
		server->problem_class = FATAL;
		return rc;
	} else if (rc < 4) {
		create_msg(server, "%s: %s request_id not received\n",
			image->name, fname_print);
		server->problem_class = FATAL;
		return INTERNAL_ERROR;
	}
	request_id = *((int *)server->priv->outlist);
	DEBUG_PRINT("request_id = %08x = %u\n", request_id, request_id);

	rc = vm6_wait_for_response(server, fname_print, EPOLLIN,
				   server->timeout);
	if (rc)
		return rc;
	/* Receive the API output list */
	rc = vm6_recv_all(server, fname_print, 16);
	if (rc < 0) {
		DEBUG_PRINT("recv return_code = %08x = %i\n", rc, rc);
		create_msg(server,
			"%s: %s failed, return_code of recv is %i %s\n",
			image->name, fname_print, errno, strerror(errno));
		server->problem_class = FATAL;
		return rc;
	} else if (rc < 16) {
		create_msg(server, "%s: %s response not received\n",
			image->name, fname_print);
		server->problem_class = FATAL;
		return INTERNAL_ERROR;
	}

	/* Analyze and return result */
	resp_hdr = (struct vm6_image_response *)server->priv->outlist;
	DEBUG_PRINT("output_length = %08x = %i\n",
		resp_hdr->output_length, resp_hdr->output_length);
	DEBUG_PRINT("request_id = %08x = %u\n",
		resp_hdr->request_id, resp_hdr->request_id);
	DEBUG_PRINT("return_code = %08x = %i\n",
		resp_hdr->return_code, resp_hdr->return_code);
	DEBUG_PRINT("reason_code = %08x = %i\n",
		resp_hdr->reason_code, resp_hdr->reason_code);
	DEBUG_PRINT("processed = %08x = %i\n",
		resp_hdr->processed, resp_hdr->processed);
	DEBUG_PRINT("not processed = %08x = %i\n",
		resp_hdr->not_processed, resp_hdr->not_processed);
	if (request_id != resp_hdr->request_id)
		DEBUG_PRINT("internal error - request id\n");
	err_dsc = vmsmapi6_get_error_description(resp_hdr->return_code,
						 resp_hdr->reason_code);
	if (!strcmp(fname, "Image_Status_Query\0") &&
	    (resp_hdr->return_code == RC_OK) &&
	    (resp_hdr->reason_code == RS_NONE))
		err_dsc = image_active;
	create_msg(server, "* %s : Image %s %s\n",
		fname_print, image->name, err_dsc);
	if (resp_hdr->return_code) {
		server->problem_class = FATAL;
		fprintf(stderr, "* Error during SMAPI server communication: "
			"return code %i, reason code %i\n",
			resp_hdr->return_code, resp_hdr->reason_code);
		rc = CONNECTION_ERROR;
	} else {
		server->problem_class = OK;
		rc = 0;
	}

	return rc;
}

/*--------------------------------------------------------------------*/
int vm6_image_activate(struct snipl_image *image)
{
	char FNAME[] = "Image_Activate\0";

	return vm6_command_handling(image, FNAME);
}

/*--------------------------------------------------------------------*/
int vm6_image_reset(struct snipl_image *image)
{
	char FNAME[] = "Image_Recycle\0";

	return vm6_command_handling(image, FNAME);

}

/*--------------------------------------------------------------------*/
int vm6_image_deactivate(struct snipl_image *image)
{
	char FNAME[] = "Image_Deactivate\0";

	return vm6_command_handling(image, FNAME);

}

/*--------------------------------------------------------------------*/
int vm6_image_getstatus(struct snipl_image *image)
{
	char FNAME[] = "Image_Status_Query\0";

	return vm6_command_handling(image, FNAME);
}

/*--------------------------------------------------------------------*/
static int vm6_check_certificate(struct snipl_server *server)
{
	char *svr_fingerprint = NULL;
	int ret;

	if (!server->sslfingerprint) {
		create_msg(server,
			   "%s: Undefined local fingerprint of certificate.\n",
			   server->address);
		server->problem_class = CERTIFICATE_ERROR;
		return -1;
	}

	if (vm6_get_cert_attributes(server, &svr_fingerprint))
		return -1;

	ret = 0;
	if (strcasecmp(server->sslfingerprint, svr_fingerprint)) {
		create_msg(server,
			   "%s: security issue: fingerprints do not match!\n",
			   server->address);
		server->problem_class = CERTIFICATE_ERROR;
		ret = -1;
	}
	free(svr_fingerprint);
	return ret;
}

/*-------------------------------------------------------------------*/
/*
   Get the OpenSSL provided error information and set the server
   problem string.
*/
static void print_ssl_errors(struct snipl_server *server)
{
	BIO *mem_bio;
	char *msg = NULL;
	char buf[121];
	int msg_len;

	mem_bio = BIO_new(BIO_s_mem());
	if (!mem_bio) {
		create_msg(server, "%s: unable to allocate BIO object\n",
			   server->address);
		goto out;
	}

	ERR_print_errors(mem_bio);

	memset(buf, '\0', sizeof(buf));
	msg_len = 1;
	while (BIO_gets(mem_bio, buf, sizeof(buf) - 1) > 0) {
		if (!msg) {
			msg = strdup(buf);
			if (!msg)
				goto out;
			msg_len += strlen(msg);
		} else {
			msg_len += strlen(buf);
			msg = realloc(msg, msg_len);
			if (!msg)
				goto out;
			strcat(msg, buf);
		}
		memset(buf, '\0', sizeof(buf));
	}
	if (msg)
		create_msg(server, "%s:\n %s", server->address, msg);
out:
	if (!msg)
		create_msg(server,
			   "%s: unable to allocate SSL error message buffer\n",
			   server->address);
	if (mem_bio)
		BIO_vfree(mem_bio);
	free(msg);
	}

/*--------------------------------------------------------------------*/
/*
   login to vm server :
   connect to server
*/
int vm6_server_login(struct snipl_server *server)
{
	int protolevel = SOL_SOCKET;
	int option = 1;
	char FNAME[] = "Connect\0";
	struct addrinfo *ai_result = NULL;
	struct addrinfo hints;
	bzero(&hints, sizeof(hints));
	char serverPortStr[8];
	int flags = 0;
	int rc;
	int ssl_rc;

	DEBUG_PRINT("vmsmapi6 : start of function\n");

	hints.ai_family = PF_UNSPEC;
	hints.ai_protocol = IPPROTO_IP;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;

	sprintf(serverPortStr, "%d", server->port);
	rc = getaddrinfo(server->address, serverPortStr, &hints, &ai_result);

	struct sockaddr_in6 addrSpace;
	struct sockaddr *connectAddr = (struct sockaddr *) &addrSpace;

	if (rc == EAI_NONAME) {
		DEBUG_PRINT("vmsmapi6 : hostname cannot be resolved\n");
		create_msg(server,
			"%s: host name cannot be resolved, return_code is %i\n",
			server->address, rc);
		server->problem_class = FATAL;
		return INTERNAL_ERROR;
	} else if (rc != 0) {
		// try old IPv4 way ...
		DEBUG_PRINT("vmsmapi6 : trying old IPv4 way\n");

		struct sockaddr_in *sa = (struct sockaddr_in *) connectAddr;
		bzero(sa, sizeof(struct sockaddr_in));

		sa->sin_addr.s_addr = inet_addr(server->address);

		if (sa->sin_addr.s_addr == INADDR_NONE || sa->sin_addr.s_addr == 0) {
			// not an IP, must be a hostname
			struct hostent *he = gethostbyname(server->address);
			if (he == NULL) {
				create_msg(server,
					"%s: gethostbyname() failed, return_code is %i %s\n",
					server->address, errno, strerror(errno));
					server->problem_class = FATAL;
					return INTERNAL_ERROR;
			}
			DEBUG_PRINT("vmsmapi6 : using first IPv4 address from returned list\n");
			// get first IPv4 address from returned list
			memcpy((char*)&sa->sin_addr, he->h_addr_list[0], he->h_length);

		} else {
			// it was an textual IP, sin_addr is set
			DEBUG_PRINT("vmsmapi6 : it was an textual IP, sin_addr is set\n");
		}

		sa->sin_family = AF_INET;
		sa->sin_port = server->port;

	} else {
		// success
		DEBUG_PRINT("vmsmapi6 : addrinfo is set\n");
		// note: we use the 1st addrinfo, but we could try more
		memcpy(connectAddr,ai_result->ai_addr,sizeof(struct sockaddr_in6));

		freeaddrinfo(ai_result);
	}

	// connectAddr is set

	server->priv->sockid = socket(connectAddr->sa_family, SOCK_STREAM, IPPROTO_IP);

	if (server->priv->sockid < 0) {

		if (errno == EAFNOSUPPORT && connectAddr->sa_family == AF_INET) {
			// IPv6 stack only, map v4 address to v6 address
			DEBUG_PRINT("vmsmapi6 : IPv6 stack only, mapping v4 address to v6 address\n");
			struct sockaddr_in6 *sa6 =
				(struct sockaddr_in6 *) connectAddr;

			in_addr_t ipv4 =
				((struct sockaddr_in *)connectAddr)->sin_addr.s_addr;

			bzero(sa6, sizeof(struct sockaddr_in6));

			sa6->sin6_family = AF_INET6;
			sa6->sin6_port = server->port;
			sa6->sin6_addr.s6_addr32[0] = 0;
			sa6->sin6_addr.s6_addr32[1] = 0;
			sa6->sin6_addr.s6_addr32[2] = htonl (0xffff);
			sa6->sin6_addr.s6_addr32[3] = ipv4;

			// retry socket call ...
			server->priv->sockid = socket(connectAddr->sa_family, SOCK_STREAM, IPPROTO_IP);

			if (server->priv->sockid < 0) {
				create_msg(server,
					"%s: socket failed, return_code is %i %s\n",
					server->address, errno, strerror(errno));
				server->problem_class = FATAL;
				return INTERNAL_ERROR;
			}
		} else {
			create_msg(server,
				"%s: socket failed, return_code is %i %s\n",
				server->address, errno, strerror(errno));
			server->problem_class = FATAL;
			return INTERNAL_ERROR;
		}
	}

	server->priv->sock_state = ALLOCATED;

	flags = fcntl(server->priv->sockid, F_GETFL, 0);
	fcntl(server->priv->sockid, F_SETFL, flags | O_NONBLOCK);
	rc = setsockopt(server->priv->sockid, protolevel, SO_REUSEADDR, &option,
			sizeof(option));
	if (rc < 0) {
		DEBUG_PRINT("setsockopt return_code = %08x = %i\n", rc, rc);
		create_msg(server,
			"%s: setsockopt failed, return_code is %i %s\n",
			server->address, errno, strerror(errno));
		server->problem_class = FATAL;
		return rc;
	}

	rc = connect( server->priv->sockid, connectAddr,
			((connectAddr->sa_family == AF_INET)
					? sizeof(struct sockaddr_in)
					: sizeof(struct sockaddr_in6)));

	if (errno == EINPROGRESS)
		rc = vm6_wait_for_response(server, FNAME, EPOLLOUT, 1000);
	if (rc < 0) {
		DEBUG_PRINT("connect return_code = %08x = %i\n", rc, rc);
		create_msg(server, "%s: connect failed, return_code is %i %s\n",
			   server->address, errno, strerror(errno));
		server->problem_class = FATAL;
		return rc;
	}
	server->priv->sock_state = CONNECTED;
	if (!server->enc)
		return rc;

	SSL_library_init();
	/* SSL_library_init() always returns "1", */
	/* so it is safe to discard the return value. */
	/* SSL_load_error_strings is void -> no error checking */
	SSL_load_error_strings();
	server->priv->sslcontext = SSL_CTX_new(SSLv23_client_method());
	if (!server->priv->sslcontext) {
		print_ssl_errors(server);
		server->problem_class = FATAL;
		return INTERNAL_ERROR;
	}
	server->priv->sslhandle = SSL_new(server->priv->sslcontext);
	if (!server->priv->sslhandle) {
		print_ssl_errors(server);
		server->problem_class = FATAL;
		return INTERNAL_ERROR;
	}
	if (!SSL_set_fd(server->priv->sslhandle, server->priv->sockid)) {
		print_ssl_errors(server);
		server->problem_class = FATAL;
		return INTERNAL_ERROR;
	}

	while ((ssl_rc = SSL_connect(server->priv->sslhandle)) != 1) {
		rc = SSL_get_error(server->priv->sslhandle, ssl_rc);
		switch (rc) {
		case SSL_ERROR_WANT_WRITE:
			rc = vm6_wait_for_response(server,
						   FNAME, EPOLLOUT, 1000);
			if (rc)
				return rc;
			continue;
		case SSL_ERROR_WANT_READ:
			rc = vm6_wait_for_response(server,
						   FNAME, EPOLLIN, 1000);
			if (rc)
				return rc;
			continue;
		default:
			print_ssl_errors(server);
			server->problem_class = FATAL;
			return INTERNAL_ERROR;
		}
	}
	DEBUG_PRINT("vmsmapi6 : ssl login done %d %d\n", errno, rc);
	return vm6_check_certificate(server);
}


static int vm6_server_logout(struct snipl_server *server)
{
	struct snipl_image *image;
	int rc = 0;

	DEBUG_PRINT("vmsmapi6 : start of function\n");
	if (server->priv && server->priv->sock_state == CONNECTED) {
		if (server->enc) {
			if (SSL_shutdown(server->priv->sslhandle))
				print_ssl_errors(server);
			SSL_free(server->priv->sslhandle);
		}
		rc = shutdown(server->priv->sockid, SHUT_RDWR);
		if (rc) {
			DEBUG_PRINT("shutdown return_code = %08x = %i\n",
				rc, rc);
			create_msg(server,
				"%s: shutdown failed, return_code is %i %s\n",
				server->address, errno, strerror(errno));
			server->problem_class = FATAL;
		}
		rc = close(server->priv->sockid);
		if (rc) {
			DEBUG_PRINT("close return_code = %08x = %i\n", rc, rc);
			create_msg(server,
				"%s: close failed, return_code is %i %s\n",
				server->address, errno, strerror(errno));
			server->problem_class = FATAL;
		}
		server->priv->sock_state == ALLOCATED;
	}

	snipl_for_each_image(server, image) {
		free(image->priv);
		image->priv = NULL;
	}
	if (server->priv) {
		free(server->priv->inlist);
		free(server->priv->outlist);
		free(server->priv);
		server->priv = NULL;
	}

	return rc;
}
