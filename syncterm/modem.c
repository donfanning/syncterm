/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: modem.c,v 1.26 2013/08/20 07:36:04 deuce Exp $ */

#include <stdlib.h>

#include "comio.h"
#include "ciolib.h"

#include "sockwrap.h"

#include "modem.h"
#include "syncterm.h"
#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"

static COM_HANDLE com=COM_HANDLE_INVALID;

void modem_input_thread(void *args)
{
	int		rd;
	int	buffered;
	size_t	buffer;
	BOOL	monitor_dsr=TRUE;

	SetThreadName("Modem Input");
	conn_api.input_thread_running=1;
	if(args != NULL) {
		if((comGetModemStatus(com)&COM_DSR) == 0)
			monitor_dsr=FALSE;
	}
	while(com != COM_HANDLE_INVALID && !conn_api.terminate) {
		rd=comReadBuf(com, conn_api.rd_buf, conn_api.rd_buf_size, NULL, 100);
		buffered=0;
		while(buffered < rd) {
			pthread_mutex_lock(&(conn_inbuf.mutex));
			buffer=conn_buf_wait_free(&conn_inbuf, rd-buffered, 100);
			buffered+=conn_buf_put(&conn_inbuf, conn_api.rd_buf+buffered, buffer);
			pthread_mutex_unlock(&(conn_inbuf.mutex));
		}
		if(args==NULL) {
			if((comGetModemStatus(com)&COM_DCD) == 0)
				break;
		}
		else if(monitor_dsr) {
			if((comGetModemStatus(com)&COM_DSR) == 0)
				break;
		}
	}
	if(args != NULL)
		comLowerDTR(com);
	conn_api.input_thread_running=0;
}

void modem_output_thread(void *args)
{
	int		wr;
	int		ret;
	int	sent;
	BOOL	monitor_dsr=TRUE;

	SetThreadName("Modem Output");
	conn_api.output_thread_running=1;
	if(args != NULL) {
		if((comGetModemStatus(com)&COM_DSR) == 0)
			monitor_dsr=FALSE;
	}
	while(com != COM_HANDLE_INVALID && !conn_api.terminate) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr=conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if(wr) {
			wr=conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent=0;
			while(sent < wr) {
				ret=comWriteBuf(com, conn_api.wr_buf+sent, wr-sent);
				sent+=ret;
				if(ret==COM_ERROR)
					break;
			}
			if(ret==COM_ERROR) {
			}
		}
		else
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		if(args==NULL) {
			if((comGetModemStatus(com)&COM_DCD) == 0)
				break;
		}
		else if(monitor_dsr) {
			if((comGetModemStatus(com)&COM_DSR) == 0)
				break;
		}
	}
	conn_api.output_thread_running=0;
}

int modem_response(char *str, size_t maxlen, int timeout)
{
	char	ch;
	size_t	len=0;
	time_t	start;

	start=time(NULL);
	while(1){
		/* Abort with keystroke */
		if(kbhit()) {
			getch();
			return(1);
		}

		if(time(NULL)-start >= timeout)
			return(-1);
		if(len >= maxlen)
			return(-1);
		if(!comReadByte(com, &ch)) {
			YIELD();
			continue;
		}
		if(ch<' ' && len==0)	/* ignore prepended control chars */
			continue;

		if(ch=='\r') {
//			while(comReadByte(com,&ch));	/* eat trailing ctrl chars (e.g. 'LF') */
			break;
		}
		str[len++]=ch;
	}
	str[len]=0;

	return(0);
}

int modem_connect(struct bbslist *bbs)
{
	int		ret;
	char	respbuf[1024];

	init_uifc(TRUE, TRUE);

	if(bbs->conn_type == CONN_TYPE_SERIAL) {
		if((com=comOpen(bbs->addr)) == COM_HANDLE_INVALID) {
			uifcmsg("Cannot Open Port",	"`Cannot Open Port`\n\n"
							"Cannot open the specified serial device.\n");
			conn_api.terminate=-1;
			return(-1);
		}
		if(bbs->bpsrate) {
			if(!comSetBaudRate(com, bbs->bpsrate)) {
				uifcmsg("Cannot Set Baud Rate",	"`Cannot Set Baud Rate`\n\n"
								"Cannot open the specified serial device.\n");
				conn_api.terminate=-1;
				comClose(com);
				return(-1);
			}
		}
		if(!comRaiseDTR(com)) {
			uifcmsg("Cannot Raise DTR",	"`Cannot Raise DTR`\n\n"
							"comRaiseDTR() returned an error.\n");
			conn_api.terminate=-1;
			comClose(com);
			return(-1);
		}
	}
	else {
		if((com=comOpen(settings.mdm.device_name)) == COM_HANDLE_INVALID) {
			uifcmsg("Cannot Open Modem",	"`Cannot Open Modem`\n\n"
							"Cannot open the specified modem device.\n");
			conn_api.terminate=-1;
			return(-1);
		}
		if(settings.mdm.com_rate) {
			if(!comSetBaudRate(com, settings.mdm.com_rate)) {
				uifcmsg("Cannot Set Baud Rate",	"`Cannot Set Baud Rate`\n\n"
								"Cannot open the specified modem device.\n");
				conn_api.terminate=-1;
				comClose(com);
				return(-1);
			}
		}
		if(!comRaiseDTR(com)) {
			uifcmsg("Cannot Raise DTR",	"`Cannot Raise DTR`\n\n"
							"comRaiseDTR() returned an error.\n");
			conn_api.terminate=-1;
			comClose(com);
			return(-1);
		}

		/* drain keyboard input to avoid accidental cancel */
		while(kbhit())
			getch();

		uifc.pop("Initializing...");

		comWriteString(com, settings.mdm.init_string);
		comWriteString(com, "\r");

		/* Wait for "OK" */
		while(1) {
			if((ret=modem_response(respbuf, sizeof(respbuf), 5))!=0) {
				modem_close();
				uifc.pop(NULL);
				if(ret<0)
					uifcmsg("Modem Not Responding",	"`Modem Not Responding`\n\n"
								"The modem did not respond to the initializtion string\n"
								"Check your init string and phone number.\n");
				conn_api.terminate=-1;
				return(-1);
			}
			if(strstr(respbuf, settings.mdm.init_string))	/* Echo is on */
				continue;
			break;
		}

		if(!strstr(respbuf, "OK")) {
			modem_close();
			uifc.pop(NULL);
			uifcmsg(respbuf,	"`Initialization Error`\n\n"
							"The modem did not respond favorably to your initialization string.\n");
			conn_api.terminate=-1;
			return(-1);
		}

		uifc.pop(NULL);
		uifc.pop("Dialing...");
		comWriteString(com, settings.mdm.dial_string);
		comWriteString(com, bbs->addr);
		comWriteString(com, "\r");

		/* Wait for "CONNECT" */
		while(1) {
			if((ret=modem_response(respbuf, sizeof(respbuf), 60))!=0) {
				modem_close();
				uifc.pop(NULL);
				if(ret<0)
					uifcmsg(respbuf,	"`No Answer`\n\n"
								"The modem did not connect within 60 seconds.\n");
				conn_api.terminate=-1;
				return(-1);
			}
			if(strstr(respbuf, bbs->addr))	/* Dial command echoed */
				continue;
			break;
		}

		if(!strstr(respbuf, "CONNECT")) {
			modem_close();
			uifc.pop(NULL);
			uifcmsg(respbuf,	"`Connection Failed`\n\n"
							"SyncTERM was unable to establish a connection.\n");
			conn_api.terminate=-1;
			return(-1);
		}

		uifc.pop(NULL);
		uifc.pop(respbuf);
		SLEEP(1000);
		uifc.pop(NULL);
	}

	if(!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		conn_api.close();
		return(-1);
	}
	if(!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		return(-1);
	}
	if(!(conn_api.rd_buf=(unsigned char *)malloc(BUFFER_SIZE))) {
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return(-1);
	}
	conn_api.rd_buf_size=BUFFER_SIZE;
	if(!(conn_api.wr_buf=(unsigned char *)malloc(BUFFER_SIZE))) {
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		FREE_AND_NULL(conn_api.rd_buf);
		return(-1);
	}
	conn_api.wr_buf_size=BUFFER_SIZE;

	if(bbs->conn_type == CONN_TYPE_SERIAL) {
		_beginthread(modem_output_thread, 0, (void *)-1);
		_beginthread(modem_input_thread, 0, (void *)-1);
	}
	else {
		_beginthread(modem_output_thread, 0, NULL);
		_beginthread(modem_input_thread, 0, NULL);
	}

	uifc.pop(NULL);

	return(0);
}

int serial_close(void)
{
	conn_api.terminate=1;

	while(conn_api.input_thread_running || conn_api.output_thread_running)
		SLEEP(1);
	comClose(com);
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return(0);
}

int modem_close(void)
{
	time_t start;

	conn_api.terminate=1;

	if((comGetModemStatus(com)&COM_DCD)==0)	/* DCD already low */
		goto CLOSEIT;

	/* TODO:  We need a drain function */
	SLEEP(500);

	if(!comLowerDTR(com))
		goto CLOSEIT;

	start=time(NULL);
	while(time(NULL)-start <= 10) {
		if((comGetModemStatus(com)&COM_DCD) == 0)
			goto CLOSEIT;
		SLEEP(1000); 
	}

CLOSEIT:
	while(conn_api.input_thread_running || conn_api.output_thread_running)
		SLEEP(1);
	comClose(com);

	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return(0);
}