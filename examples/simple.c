/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id$
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include <infiniband/sa.h>
#include <infiniband/cm.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t cpu_to_be64(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t cpu_to_be64(uint64_t x) { return x; }
#endif

#define TEST_SID 0x0000000ff0000000ULL

static int connect(uint32_t cm_id)
{
	struct ib_cm_req_param param;
	struct ib_sa_path_rec sa;
	union ibv_gid *dst;
	union ibv_gid *src;
	int result;

	param.qp_type       = IBV_QPT_RC;
	param.qp_num        = 0xff00;
	param.starting_psn  = 0x7000;
        param.service_id    = TEST_SID;


        param.primary_path     = &sa;
	param.alternate_path   = NULL;
	param.private_data     = NULL;
        param.private_data_len = 0;

        param.peer_to_peer               = 0;
        param.responder_resources        = 4;
        param.initiator_depth            = 4;
        param.remote_cm_response_timeout = 20;
        param.flow_control               = 1;
        param.local_cm_response_timeout  = 20;
        param.retry_count                = 2;
        param.rnr_retry_count            = 7;
        param.max_cm_retries             = 3;
        param.srq                        = 0;

	memset(&sa, 0, sizeof(sa));

	src = (union ibv_gid *)&sa.sgid;
	dst = (union ibv_gid *)&sa.dgid;

        sa.dlid = htons(0x1f9);
        sa.slid = htons(0x3e1);

        sa.dlid = 0xf901;
        sa.slid = 0xe103;

	sa.reversible = 0x1000000;

	sa.pkey = 0xffff;
	sa.mtu  = IBV_MTU_1024;

	sa.mtu_selector  = 2;
	sa.rate_selector = 2;
	sa.rate          = 3;
	sa.packet_life_time_selector = 2;
	sa.packet_life_time          = 2;

	src->global.subnet_prefix = cpu_to_be64(0xfe80000000000000ULL);
	dst->global.subnet_prefix = cpu_to_be64(0xfe80000000000000ULL);
	src->global.interface_id  = cpu_to_be64(0x0002c90200002179ULL);
	dst->global.interface_id  = cpu_to_be64(0x0005ad000001296cULL);

	return ib_cm_send_req(cm_id, &param);
}

#if 0
int ib_ucm_event_get(int cm_id, int *event, int *state)
{
	struct ib_ucm_cmd_hdr *hdr;
	struct ib_ucm_event_get *cmd;
	struct ib_ucm_event_resp resp;
	void *msg;
	int result;
	int size;
	
	size = sizeof(*hdr) + sizeof(*cmd);
	msg = alloca(size);
	if (!msg)
		return -ENOMEM;
	
	hdr = msg;
	cmd = msg + sizeof(*hdr);

	hdr->cmd = IB_USER_CM_CMD_EVENT;
	hdr->in  = sizeof(*cmd);
	hdr->out = sizeof(resp);

	cmd->response = (unsigned long)&resp;
	cmd->data     = (unsigned long)NULL;
	cmd->info     = (unsigned long)NULL;
	cmd->data_len = 0;
	cmd->info_len = 0;

	result = write(fd, msg, size);
	if (result != size)
		return (result > 0) ? -ENODATA : result;

	*event = resp.event;
	*state = resp.state;

	return 0;
}

#endif

int main(int argc, char **argv)
{
	struct ib_cm_event *event;
	struct ib_cm_rep_param rep;
	int cm_id;
	int result;

	int param_c = 0;
	int status = 0;
	int mode;
	/*
	 * read command line.
	 */
	if (2 != argc ||
	    0 > (mode = atoi(argv[++param_c]))) {

		fprintf(stderr, "usage: %s <mode>\n", argv[0]);

		fprintf(stderr, "  mode - [client:1|server:0]\n");
		exit(1);
	}

	result = ib_cm_create_id(&cm_id);
	if (result < 0) {
		printf("Error creating CM ID <%d:%d>\n", result, errno);
		goto done;
	}

	if (mode) {
		result = connect(cm_id);
		if (result) {
			printf("Error <%d:%d> sending REQ <%d>\n", 
			       result, errno, cm_id);
			goto done;
		}
	}
	else {
		result = ib_cm_listen(cm_id, TEST_SID, 0);
		if (result) {
			printf("Error <%d:%d> listneing <%d>\n", 
			       result, errno, cm_id);
			goto done;
		}
	}

	while (!status) {

		result = ib_cm_event_get(&event);
		if (result) {
			printf("Error <%d:%d> getting event\n", 
			       result, errno);
			goto done;
		}

		printf("CM ID <%d> Event <%d> State <%d>\n", 
		       event->cm_id, event->event, event->state);

		switch (event->state) {
		case IB_CM_REQ_RCVD:

			result = ib_cm_destroy_id(cm_id);
			if (result < 0) {
				printf("Error destroying listen ID <%d:%d>\n",
				       result, errno);
				goto done;
			}
			
			cm_id = event->cm_id;

			rep.qp_num       = event->param.req_rcvd.remote_qpn;
			rep.starting_psn = event->param.req_rcvd.starting_psn;

			rep.private_data        = NULL;
			rep.private_data_len    = 0;

			rep.responder_resources = 4;
			rep.initiator_depth     = 4;
			rep.target_ack_delay    = 14;
			rep.failover_accepted   = 0;
			rep.flow_control        = 1;
			rep.rnr_retry_count     = 7;
			rep.srq                 = 0;

			result = ib_cm_send_rep(cm_id, &rep);
			if (result < 0) {
				printf("Error <%d:%d> sending REP\n",
				       result, errno);
				goto done;
			}
		
			break;
		case IB_CM_REP_RCVD:

			result = ib_cm_send_rtu(cm_id, NULL, 0);
			if (result < 0) {
				printf("Error <%d:%d> sending RTU\n",
				       result, errno);
				goto done;
			}

			break;
		case IB_CM_ESTABLISHED:

			result = ib_cm_send_dreq(cm_id, NULL, 0);
			if (result < 0) {
				printf("Error <%d:%d> sending DREQ\n",
				       result, errno);
				goto done;
			}

			break;
		case IB_CM_DREQ_RCVD:

			result = ib_cm_send_drep(cm_id, NULL, 0);
			if (result < 0) {
				printf("Error <%d:%d> sending DREP\n",
				       result, errno);
				goto done;
			}

			break;
		case IB_CM_TIMEWAIT:
			break;
		case IB_CM_IDLE:
			status = 1;
			break;
		default:
			status = EINVAL;
			printf("Unhandled state <%d:%d>\n", 
			       event->state, event->event);
			break;
		}

		result = ib_cm_event_put(event);
		if (result) {
			printf("Error <%d:%d> freeing event\n", 
			       result, errno);
			goto done;
		}
	}


	result = ib_cm_destroy_id(cm_id);
	if (result < 0) {
		printf("Error destroying CM ID <%d:%d>\n", result, errno);
		goto done;
	}

done:
	return 0;
}

