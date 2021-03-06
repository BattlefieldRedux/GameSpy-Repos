/*
* Copyright (C) 2002-2003 r1ch.net
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
*
* See the GNU General Public License for more details.
*  
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*	
*/

//Originally gloommaster 0.0.3 (c) 2002-2003 r1ch.net
//quake 2 compatible master server for gloom

// QwazyWabbit mods begin
// 11-FEB-2006
// Changes for Linux and Windows portability by Geoff Joy (QwazyWabbit)
// Simply build for Linux with "gcc -o q2master master.c"
//
// 26-JAN-2007
// Made it a general q2 server (q2master v1.0)
//
// 26-JUL-2007
// Added registry keys to tell service what IP to use.
// Added key and handling for non-standard port.
// Server could be modified for use as Q3 or Q4 master server.
//
// 18-AUG-2007
// Dynamic allocation of buffer in SendServerListToClient function.
// 
// 01-SEP-2007
// General release. This project builds in VC++ 6.0 and GCC 4.x
// Complete project easily ported to VC 2005 or higher.
//

// 09-MAY-2015 -- FS
// Updated for specific use with Daikatana.  Will likely work with
// other old gamespy titles that use encode type 0.
// 10-MAY-2015 -- FS
// Code more generic for various encode type 0 games.
// 11-MAY-2015 -- FS
// Validation works from a look-up table and various functions by
// aluigi (www.aluigi.org).  Proper heartbeat times by default.
// 15-MAY-2015 -- FS
// Various bug fixes, clean up, etc.  Updated doc.
//

// General command line switches:

// -debug	Asserts debug mode. The program prints status messages to console
//			while running. Shows pings, heartbeats, number of servers listed.

// -sendack - by default gamespy doesn't not send this type of packet out.
//            if you want to extend the courtesy of acknowleding the
//            heartbeat then enable this setting.

// -quickvalidate - by default the master server requires 1 extra heartbeat
//                  and a successful ping request to be added to the query
//                  list.  Set this to allow any new server to show up
//                  immediately.

// -timestamp <1 or 2> - Debug outputs are timestampped.  1 - for AM/PM.
//                       2 for military.

// -validationrequired <1, 2, or 3> - Require validation from the challenge key
//                                    cross-checked with the games secure key.
//                                    1 - client list requests only.
//                                    2 - servers.
//                                    3 - clients and servers (recommended).

// -heartbeatinterval <time in minutes> - Time in minutes for sending heartbeats.
//                                        Must be at least 1 minute.

// -minimumheartbeats x - Minimum number of sucessful heartbeats that need to be
//                        sent before a server will be added to the list.

// -ip xxx.xxx.xxx.xxx causes server to bind to a particular IP address when
//	used with multi-homed hosts. Default is 0.0.0.0 (any).

// -port xxxxx causes server to bind to a particular port. Default is 27900.
// Default port for Quake 2 master servers is 27900. If you depart from this
// you need to communicate this to your users somehow. This feature is included
// since this code could be modified to provide master services for other games.

// -tcpport xxxxx causes server to bind to a particular TCP port for the
// gamespy list query from clients. Default is 28900.
// If you depart from this you need to communicate this to your users somehow.
// This feature is included since this code could be modified to provide 
// master services for other games.

// -serverlist <filename> - Adds servers from a list.  Hostnames are supported.
// Format is <ip>,<query port>,<gamename> i.e. maraakate.org,27982,daikatana.

// *** Windows ***

// Usage:
// Place executable in %systemroot%\system32 or other known location.
// To debug it as a console program, command: "master -debug" and it outputs
// status messages to the console. Ctrl-C shuts it down.
//
// From a cmd prompt type: q2master -install to install the service with defaults.
// The service will be installed as "Q2MasterServer" in the Windows service list.
//

// -install	Installs the service on Windows.
// -remove	Stops and removes the service.
// When the service is installed it is installed with "Automatic" startup type
// to allow it to start up when Windows starts. Use SCM to change this.

// Other commands:
// net start q2masterserver to start the service.
// net stop q2masterserver to stop the service.
//
// Use the Services control panel applet to start/stop, change startup modes etc.
// To uninstall the server type "master -remove" to stop and remove the active service.

// *** Linux ***

// Usage:
// -debug Sets process to debug mode and it remains attached to console.
// If debug is not specified on command line the process forks a daemon and
// detaches from terminal.
//
// Send the process a SIGTERM to stop the daemon. "kill -n SIGTERM <pid>"
// Use "netstat -anup" to see the processes/pids listening on UDP ports.
// Use "ps -ux" to list detached processes, this will show the command line that
// invoked the q2master process.
// 
// *** Mac/iMac OS/X ***
// Usage:
// Same as Linux
// Compiles on OS/X same as Linux & BSD "gcc -o q2master master.c".
//

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock.h>
#include <winerror.h>
#include <time.h>
#include <winwrap.h>
#include <process.h>
#include "service.h"
#include "performance.h"
#include <assert.h>

// Windows Service structs
SERVICE_STATUS          MyServiceStatus; 
SERVICE_STATUS_HANDLE   MyServiceStatusHandle;

void SetQ2MasterRegKey(char* name, char *value);
void GetQ2MasterRegKey(char* name, char *value);

#else

// Linux and Mac versions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <sys/signal.h>
#include <assert.h>

enum {FALSE, TRUE};

// stuff not defined in sys/socket.h
#define SOCKET unsigned int
#define SOCKET_ERROR -1
#define TIMEVAL struct timeval

// portability, rename or delete functions
#define WSAGetLastError() errno
#define _strnicmp strncasecmp
#define My_Main main
#define closesocket close // FS
#define SetQ2MasterRegKey
#define	GetQ2MasterRegKey
#define WSACleanup()
#define Sleep(x)	sleep((x/1000))
void signal_handler(int sig);

#endif

#include "master.h"
#include "dk_essentials.h" // FS

// for debugging as a console application in Windows or in Linux
int Debug;
int Timestamp; // FS
int SendAck;
unsigned long numservers;	// global count of the currently listed servers

int runmode;	// server loop control

server_t servers;

struct sockaddr_in listenaddress;
struct sockaddr_in listenaddressTCP; // FS
SOCKET out;
SOCKET listener;
SOCKET listenerTCP; // FS
TIMEVAL delay;

#ifdef WIN32
WSADATA ws;
#endif

fd_set set;
fd_set tcpSet; // FS

char incoming[MAX_INCOMING_LEN];
char incomingTcpValidate[MAX_INCOMING_LEN]; // FS
char incomingTcpList[MAX_INCOMING_LEN]; // FS
int retval;
int tcpRetval; // FS
int totalRetry = 10; // FS: Total retry attempts waiting for the gamespy validate stuff
unsigned long heartbeatInterval = DEFAULTHEARTBEAT; // FS: Total minutes before sending the next status packet

char bind_ip[KEY_LEN] = "0.0.0.0"; // default IP to bind
char bind_port[KEY_LEN] = "27900";	// default port to bind
char bind_port_tcp[KEY_LEN] = "28900";	// FS: default TCP port to bind
char serverlist_filename[MAX_PATH] = ""; // FS: For a list of servers to add at startup
int load_Serverlist = 0;

int validate_newserver_immediately = 0; // FS
int validation_required = 0; // FS
unsigned int minimumHeartbeats = 2; // FS: Minimum amount of heartbeats required before we're added to the list, used to verify it's a real server.

// FS: For gamespy list
const char *listheader = "\\";
const char *finalstring = "final\\";
const char *finalstringerror = "\\final\\";
const char *statusstring = "\\status\\secure\\";
// FS: Daikatana needs \\secure\\ then a key to encode with.
const char *challengeHeader = "\\basic\\\\secure\\"; // FS: This is the start of the handshake

//
// This becomes main for Linux
// In Windows, main is in service.c and it decides if we're going to see a console or not
// this function gets called when we have decided if we are a server or a console app.
//
int My_Main (int argc, char *argv[])
{
	int len, err;
//	unsigned int fromlen;
	int fromlen;
	struct sockaddr_in from;

	printf ("Daikatana-GSPY-Master v%s.\nBased on Q2-Master 1.1 originally GloomMaster.\n(c) 2002-2003 r1ch.net, modifications by QwazyWabbit 2007.\n", VERSION);
	printf ("Built: %s at %s.\n\n", __DATE__, __TIME__);
	numservers = 0;

#ifdef WIN32
	// overhead to tell Windows we're using TCP/IP.
	err = WSAStartup ((WORD)MAKEWORD (1,1), &ws);
	if (err) {
		printf("Error loading Windows Sockets! Error: %i\n",err);
		return (err);
	}
#endif

#ifndef WIN32	// Already done in ServiceStart() if Windows
	ParseCommandLine(argc, argv);
#endif

	printf("Debugging mode: %i\n", Debug);
	printf("Send Acknowledgments: %i\n", SendAck); // FS
	printf("Validate New Server Immediately: %i\n", validate_newserver_immediately); // FS
	printf("Require Validation: %i\n", validation_required); // FS
	printf("Heartbeat interval: %u Minutes\n", heartbeatInterval/60); // FS
	printf("Minimum Heartbeats Required: %u\n", minimumHeartbeats); // FS
	printf("Timestamps: %i\n", Timestamp); // FS

	listener = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	listenerTCP = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); // FS
	out = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memset (&listenaddress, 0, sizeof(listenaddress));
	memset (&listenaddressTCP, 0, sizeof(listenaddressTCP)); // FS

	// only in Windows, null def in Linux
	GetQ2MasterRegKey(REGKEY_BIND_IP, bind_ip);
	GetQ2MasterRegKey(REGKEY_BIND_PORT, bind_port);
	GetQ2MasterRegKey(REGKEY_BIND_PORT_TCP, bind_port_tcp); // FS

	// FS: Ensure we don't set the ports to something stupid or have corrupt registry values
	Check_Port_Boundaries();

	listenaddress.sin_addr.s_addr = inet_addr(bind_ip); 
	listenaddress.sin_family = AF_INET;
	listenaddress.sin_port = htons((unsigned short)atoi(bind_port));
	
	if ((bind (listener, (struct sockaddr *)&listenaddress, sizeof(listenaddress))) == SOCKET_ERROR)
	{
		printf ("[E] Couldn't bind to port %s UDP (something is probably using it)\n", bind_port);
		return 1;
	}
	
	listenaddressTCP.sin_addr.s_addr = inet_addr(bind_ip); 
	listenaddressTCP.sin_family = AF_INET;
	listenaddressTCP.sin_port = htons((unsigned short)atoi(bind_port_tcp));

	if ((bind (listenerTCP, (struct sockaddr *)&listenaddressTCP, sizeof(listenaddressTCP))) == SOCKET_ERROR)
	{
		printf ("[E] Couldn't bind to port %s TCP (something is probably using it)\n", bind_port_tcp);
		return 1;
	}

	if (listen(listenerTCP, MAXPENDING) < 0)
	{
		printf("[E] Couldn't set port %s TCP to listen mode\n", bind_port_tcp);
		return 1;
	}

	delay.tv_sec = 1;
	delay.tv_usec = 0;
	
	FD_SET (listener, &set);
	FD_SET (listenerTCP, &tcpSet); // FS
	
	fromlen = (unsigned)sizeof(from);
	memset (&servers, 0, sizeof(servers));
	printf ("listening on %s:%s (UDP)\n", bind_ip, bind_port);
	printf ("listening on %s:%s (TCP)\n", bind_ip, bind_port_tcp);
	runmode = SRV_RUN; // set loop control
	
#ifndef WIN32
	// in Linux or BSD we fork a daemon
	if (!Debug) {			// ...but not if debug mode
		if (daemon(0,0) < 0) {	
			printf("Forking error, running as console, error number was: %i\n", errno);
			Debug = 1;
		}
	}
	
	if (!Debug) 
	{
		signal(SIGCHLD, SIG_IGN); /* ignore child */
		signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGHUP, signal_handler); /* catch hangup signal */
		signal(SIGTERM, signal_handler); /* catch terminate signal */
	}
#endif

	if(load_Serverlist)
		Add_Servers_From_List(serverlist_filename);

	while (runmode == SRV_RUN) // 1 = running, 0 = stop, -1 = stopped.
	{
		FD_SET (listener, &set);
		FD_SET (listenerTCP, &tcpSet); // FS
		delay.tv_sec = 1;
		delay.tv_usec = 0;
		
		retval = select(listener + 1, &set, NULL, NULL, &delay);
		if (retval == 1)
		{
			len = recvfrom (listener, incoming, sizeof(incoming), 0, (struct sockaddr *)&from, &fromlen);
			if (len != SOCKET_ERROR)
			{
				if (len > 4)
				{
					//parse this packet
					if (!ParseResponse (&from, incoming, len))
					{
						// FS: Keep going, just don't add it
//						err = 50;
//						runmode = SRV_STOP;	// something went wrong, AddServer failed?
					}
				}
				else
				{
					Con_DPrintf ("[W] runt packet from %s:%d\n", inet_ntoa (from.sin_addr), ntohs(from.sin_port));
				}
				
				//reset for next packet
				memset (incoming, 0, sizeof(incoming));
			} 
			else 
			{
				Con_DPrintf ("[E] UDP socket error during select from %s:%d (%d)\n", 
					inet_ntoa (from.sin_addr), 
					ntohs(from.sin_port), 
					WSAGetLastError());
			}
		}
		
		// FS: Now do the Gamespy TCP handshake shit
		if(select(listenerTCP + 1, &tcpSet, NULL, NULL, &delay) == 0)
			tcpRetval = 0;
		else
			tcpRetval = accept(listenerTCP, (struct sockaddr *)&from, &fromlen);

		if (tcpRetval > 0 )
		{
			Gamespy_Parse_TCP_Packet(tcpRetval, &from);
		}
		
		// destroy old servers, etc
		RunFrame();
	}
	
	WSACleanup();	// Windows Sockets cleanup
	runmode = SRV_STOPPED;
	return(err);
}

// FS
void Close_TCP_Socket_On_Error (int socket, struct sockaddr_in *from)
{
	Con_DPrintf ("[E] TCP socket error during accept from %s:%d (%d)\n",
		inet_ntoa (from->sin_addr),
		ntohs(from->sin_port),
		WSAGetLastError());
	closesocket(socket);
	socket = INVALID_SOCKET;
}

// FS: Set the TCP port for non-blocking so assholes can't just send send a packet then bail, holding up the server.
int TCP_Set_Non_Blocking (int socket)
{
	int error = 0;
#ifdef WIN32
	u_long nonblocking_enabled = 1;
	error = ioctlsocket( socket, FIONBIO, &nonblocking_enabled );
#else
	int flags = 0;

	flags = fcntl(socket, F_GETFL, 0);
	if (flags == -1)
		flags = 0;
	error = fcntl(socket, F_SETFL, flags | O_NONBLOCK))
#endif // _WIN32
	return error;
}

//
// Called by ServiceCtrlHandler after the server loop is dead
// this frees the server memory allocations.
//
void ExitNicely (void)
{
	server_t	*server = &servers;
	server_t	*old = NULL;
	
	printf ("[I] shutting down.\n");
	
	while (server->next)
	{
		if (old)
			free (old);
		server = server->next;
		old = server;
	}
	
	if (old)
		free (old);
}

void DropServer (server_t *server)
{
	//unlink
	if (server->next)
		server->next->prev = server->prev;

	if (server->prev)
		server->prev->next = server->next;

	if (numservers != 0) 
		numservers--;

	free (server);
}

//
// returns TRUE if successfully added to list
//
int AddServer (struct sockaddr_in *from, int normal, unsigned short queryPort, char *gamename, char *hostnameIp)
{
	server_t	*server = &servers;
	int			preserved_heartbeats = 0;
	struct sockaddr_in addr;
	char validateString[MAX_GSPY_VAL];
	int validateStringLen = 0;
	memset(validateString, 0, sizeof(validateString));

	if(!gamename || gamename[0] == 0)
	{
		Con_DPrintf ("[E] No gamename sent from %s:%u.  Aborting AddServer.\n", inet_ntoa(from->sin_addr), htons(from->sin_port));
		return FALSE;
	}

	if(queryPort <= 0)
	{
		Con_DPrintf ("[E] No Query Port sent from %s:%u.  Aborting AddServer.\n", inet_ntoa(from->sin_addr), htons(from->sin_port));
		return FALSE;
	}

	if(!Gamespy_Get_Game_SecKey(gamename))
	{
		Con_DPrintf ("[E] Game %s not supported from %s:%u.  Aborting AddServer.\n", gamename, inet_ntoa(from->sin_addr), htons(from->sin_port));
		return FALSE;
	}

	while (server->next)
	{
		server = server->next;

		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && htons(queryPort) == htons(server->port))
		{
			//already exists - could be a pending shutdown (ie killserver, change of map, etc)
			if (server->shutdown_issued)
			{
				Con_DPrintf ("[I] scheduled shutdown server %s sent another ping!\n",
					inet_ntoa(from->sin_addr));
				DropServer (server);
				server = &servers;
				while (server->next)
					server = server->next;
				break;
			}
			else
			{
				Con_DPrintf ("[W] dupe ping from %s:%u!! ignored.\n",
					inet_ntoa (server->ip.sin_addr),
					htons(server->port));
				return TRUE;
			}
		}
	}

	server->next = (server_t *)malloc(sizeof(server_t));
	assert(server->next != NULL); // malloc failed if server->next == NULL
	// Note: printf implicitly calls malloc so this is likely to fail if we're really out of memory
	if (server->next == NULL) {
		printf("Fatal Error: memory allocation failed in AddServer\n");
		return FALSE;
	}
	
	
	server->next->prev = server;
	server = server->next;
	server->heartbeats = preserved_heartbeats;
	memcpy (&server->ip, from, sizeof(server->ip));
	server->last_heartbeat = time(NULL);
	server->next = NULL;

	if(!hostnameIp || hostnameIp[0] == 0) // FS: If we add servers from a list using dynamic IPs, etc.  let's remember it.  Else, just copy the ip
		hostnameIp = inet_ntoa(from->sin_addr);

	gamename = DK_strlwr(gamename); // FS: Some games (mainly sin) stupidly send it partially uppercase
	DG_strlcpy(server->gamename, gamename, sizeof(server->gamename));

	server->port = queryPort; //from->sin_port; // FS: Gamespy does it differently.
	server->shutdown_issued = 0;
	server->queued_pings = 0;
	server->last_ping = 0;
	server->validated = 0;
	Gamespy_Create_Challenge_Key(server->challengeKey, 6); // FS: Challenge key for this server
	server->challengeKey[6] = '\0';
	DG_strlcpy(server->hostnameIp, hostnameIp, sizeof(server->hostnameIp));

	numservers++;
	
	Con_DPrintf ("[I] %s server %s:%u added to queue! (%d) number: %u\n",
		server->gamename,
//		inet_ntoa (from->sin_addr),
		server->hostnameIp, // FS: Show it by hostname
		htons(server->port),
		normal,
		numservers);

	if(validate_newserver_immediately)
	{
		Con_DPrintf("[I] immediately validating new server %s:%u to master server.\n", server->hostnameIp, htons(server->port));
		server->validated = 1;
		server->heartbeats = minimumHeartbeats;
	}

	memcpy (&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
	addr.sin_family = AF_INET;
	addr.sin_port = server->port;
	memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));

	if (normal && SendAck) // FS: This isn't standard for DK, it will show messages about the ack.  This is more a courtesy to tell the ded server that we received the heartbeat
	{
		sendto (listener, OOB_SEQ"ack", 7, 0, (struct sockaddr *)&addr, sizeof(addr));
	}

	Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
	validateStringLen = DG_strlen(validateString);
	validateString[validateStringLen] = '\0';

	sendto (listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr)); // FS: Gamespy sends this after a heartbeat.

	return TRUE;
}

//
// We received a shutdown frame from a server, set the shutdown flag
// for it and send it a ping to ack the shutdown frame.
//
void QueueShutdown (struct sockaddr_in *from, server_t *myserver)
{
	server_t	*server = &servers;
	
	if (!myserver)
	{
		while (server->next)
		{
			server = server->next;
			if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
			{
				myserver = server;
				break;
			}
		}
	}
	
	if (myserver)
	{
		struct sockaddr_in addr;
		char validateString[MAX_GSPY_VAL]; // FS
		int validateStringLen = 0; // FS

		memset(validateString, 0, sizeof(validateString)); // FS
		memcpy (&addr.sin_addr, &myserver->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));

		//hack, server will be dropped in next minute IF it doesn't respond to our ping
		myserver->shutdown_issued = 1;
		Gamespy_Create_Challenge_Key(myserver->challengeKey, 6); // FS: Challenge key for this server
		myserver->challengeKey[6] = '\0'; // FS: Gamespy null terminates the end

		Con_DPrintf ("[I] shutdown queued %s:%u \n", inet_ntoa (myserver->ip.sin_addr), htons(server->port));

		Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
		validateStringLen = DG_strlen(validateString);
		validateString[validateStringLen] = '\0'; // FS: Gamespy null terminates the end

		sendto (listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr));
		return;
	}
	
	else
	{
		Con_DPrintf ("[W] shutdown issued from unregistered server %s!\n", inet_ntoa (from->sin_addr));
	}
}

//
// Walk the server list and ping them as needed, age the ones
// we have not heard from in a while and when they get too
// old, remove them from the list.
//
void RunFrame (void)
{
	server_t		*server = &servers;
	unsigned int	curtime = time(NULL);
	
	while (server->next)
	{
		server = server->next;
		if (curtime - server->last_heartbeat > 60)
		{
			server_t *old = server;
			
			server = old->prev;
			
			if (old->shutdown_issued || old->queued_pings > 6)
			{
				Con_DPrintf ("[I] %s:%u shut down.\n", inet_ntoa (old->ip.sin_addr), htons(old->port));
				DropServer (old);
				continue;
			}
			
			server = old;

			if (curtime - server->last_ping >= heartbeatInterval)
			{
				struct sockaddr_in addr;
				char validateString[MAX_GSPY_VAL]; // FS
				int validateStringLen = 0; // FS

				memset(validateString, 0, sizeof(validateString)); // FS
				addr.sin_addr = Hostname_to_IP(&server->ip.sin_addr, server->hostnameIp); // FS: Resolve hostname if it's from a serverlist file
//				memcpy (&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
				addr.sin_family = AF_INET;
				addr.sin_port = server->port;
				memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));
				server->queued_pings++;
				server->last_ping = curtime;
				Gamespy_Create_Challenge_Key(server->challengeKey, 6); // FS: Challenge key for this server
				server->challengeKey[6] = '\0'; // FS: Gamespy null terminates the end
//				Con_DPrintf ("[I] ping %s:%u\n", inet_ntoa (server->ip.sin_addr), htons(server->port));
				Con_DPrintf ("[I] ping %s(%s):%u\n", server->hostnameIp, inet_ntoa(addr.sin_addr), htons(server->port)); // FS: New ping message
				Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
				validateStringLen = DG_strlen(validateString);
				validateString[validateStringLen] = '\0'; // FS: Gamespy null terminates the end
				sendto (listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr)); // FS: gamespy sends a status
			}
		}
	}
}

//
// This function assembles the serverstring preamble and 6 bytes for each
// listed server into a buffer for transmission to the client in response
// to a query frame.
//
#ifdef QUAKE2_SERVERS
void SendServerListToClient (struct sockaddr_in *from)
{
	int				buflen;
	char			*buff;
	server_t		*server = &servers;
	unsigned long	servercount;
	unsigned long	bufsize;
	
	// assume buffer size needed is for all current servers (numservers)
	// and eligible servers in list will always be less or equal to numservers
	
	bufsize = 12 + 6 * (numservers + 1); // 12 bytes for serverstring, 6 bytes for game server ip and port
	buflen = 0;
	buff = malloc (bufsize);
	assert(buff != NULL);	// catch it in debug
	if (buff == NULL) {
		printf("Fatal Error: memory allocation failed in SendServerListToClient\n");
		return;
	}
	
	memset (buff, 0, bufsize);
	memcpy (buff, serverstring, 12);	// 12 = length of serverstring
	buflen += 12;
	servercount = 0;
	
	while (server->next)
	{
		server = server->next;
		if (server->heartbeats >= minimumHearbeats && !server->shutdown_issued && server->validated)
		{
			memcpy (buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;
			
			memcpy (buff + buflen, &server->port, 2);
			buflen += 2;
			servercount++;
		}
	}

	Con_DPrintf ("[I] list: %s\n", buff);
	Con_DPrintf ("[I] query response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa (from->sin_addr), ntohs (from->sin_port));

	if ((sendto (listener, buff, buflen, 0, (struct sockaddr *)from, sizeof(*from))) == SOCKET_ERROR)
	{
		Con_DPrintf ("[E] list socket error on send! code %d.\n", WSAGetLastError());
	}
	
	Con_DPrintf ("[I] sent server list to client %s, servers: %u of %u\n", 
				inet_ntoa (from->sin_addr), 
				servercount, /* sent */
				numservers); /* on record */

	free(buff);
}
#endif // QUAKE2_SERVERS

// FS: Gamespy BASIC data is in the form of '\ip\1.2.3.4:1234\ip\1.2.3.4:1234\final\'
// FS: Gamespy non-basic data is in the form of '<sin_addr><sin_port>\final\'
void SendGamespyListToClient (int socket, char *gamename, struct sockaddr_in *from, int basic)
{
	int				buflen;
	char			*buff;
	char			*port;
	char			*ip = NULL;
	server_t		*server = &servers;
	unsigned long	servercount;
	unsigned long	bufsize;
	
	// assume buffer size needed is for all current servers (numservers)
	// and eligible servers in list will always be less or equal to numservers
	if(!gamename || gamename[0] == 0)
	{
		Con_DPrintf("[E] No gamename specified for Gamespy List Request!  Aborting.\n");
		return;
	}

	gamename = DK_strlwr(gamename); // FS: Some games (mainly sin) stupidly send it partially uppercase

	bufsize = 1 + 26 * (numservers + 1) + 6; // 1 byte for /, 26 bytes for ip:port/, 6 for final/
	buflen = 0;
	buff = (char *)malloc (bufsize);
	port = (char *)malloc (bufsize);
	memset (port, 0 , bufsize);
	assert(buff != NULL);	// catch it in debug
	if (buff == NULL)
	{
		printf("Fatal Error: memory allocation failed in SendGamespyListToClient\n");
		return;
	}
	
	memset (buff, 0, bufsize);
	if (basic)
	{
		memcpy (buff, listheader, 1);
		buflen += 1;
	}
	servercount = 0;

	while (server->next)
	{
		server = server->next;

		if (server->heartbeats >= minimumHeartbeats && !server->shutdown_issued && server->validated && server->gamename && gamename && !strcmp(server->gamename, gamename))
		{
			ip = inet_ntoa(server->ip.sin_addr);

			if (basic)
			{
				memcpy (buff + buflen, "ip\\", 3); // 3
				buflen += 3;
				memcpy (buff + buflen, ip, DG_strlen(ip)); // 16
				buflen += DG_strlen(ip);
				memcpy (buff + buflen, ":", 1); // 1
				buflen += 1;
			
				sprintf(port ,"%d\\", ntohs(server->port));
				memcpy (buff + buflen, port, DG_strlen(port)); // 6
				buflen += DG_strlen(port);
				servercount++;
				continue;
			}
			memcpy (buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;
			memcpy (buff + buflen, &server->port, 2);
			buflen += 2;
			servercount++;
		}
	}

	if(basic)
	{
		memcpy(buff + buflen, finalstring, 6);
		buflen += 6;
	}
	else
	{
		memcpy(buff + buflen, "\\", 1);
		buflen += 1;
		memcpy(buff + buflen, finalstring, 6);
		buflen += 6;
	}

	Con_DPrintf ("[I] TCP gamespy list: %s\n", buff);
	Con_DPrintf ("[I] TCP gamespy list response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa (from->sin_addr), ntohs (from->sin_port));
	if(send(socket, buff, buflen, 0) == SOCKET_ERROR)
	{
		Con_DPrintf ("[E] TCP list socket error on send! code %d.\n", WSAGetLastError());
	}
	
	Con_DPrintf ("[I] sent TCP gamespy list to client %s, servers: %u of %u\n", 
				inet_ntoa (from->sin_addr), 
				servercount, /* sent */
				numservers); /* on record */

	free(buff);
	free(port);
}

void Ack (struct sockaddr_in *from, char* dataPacket)
{
	server_t	*server = &servers;
	
	//iterate through known servers
	while (server->next)
	{
		server = server->next;
		//a match!
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
		{
			Con_DPrintf ("[I] ack from %s:%u (%d)(%i).\n",
				inet_ntoa (server->ip.sin_addr),
				htons(server->port),
				server->queued_pings,
				server->validated);

			server->last_heartbeat = time(NULL);
			server->validated = Gamespy_Challenge_Cross_Check(server->challengeKey, dataPacket, 1);
			server->queued_pings = 0;
			if(server->shutdown_issued)
			{
				Con_DPrintf("[I] aborting scheduled shutdown from %s:%u.\n", inet_ntoa (server->ip.sin_addr), htons(server->port));
			}
			server->shutdown_issued = 0; // FS: If we're still responding then we didn't shutdown, just changed the map
			server->heartbeats++;
			return;
		}
	}
}

int HeartBeat (struct sockaddr_in *from, char *data)
{
	server_t	*server = &servers;
	int status;
	unsigned short queryPort = 0; // FS
	char seperators[] = "\\"; // FS
	char *cmdToken = NULL; // FS
	char *cmdPtr = NULL;
	int statechanged = FALSE; // FS

	status = TRUE;

	if(!data || data[0] == '\0') // FS: Should never happen, but I'd rather just be paranoid.
		return FALSE;

	if(strstr(data,"\\statechanged\\")) // FS: Schedule a shutdown if statechanged is sent with heartbeat
		statechanged = TRUE;

	cmdToken = DK_strtok_r(data, seperators, &cmdPtr); // FS: heartbeat
	cmdToken = DK_strtok_r(NULL, seperators, &cmdPtr); // FS: actual port #
	if(!cmdPtr)
	{
		Con_DPrintf("[E] Invalid heartbeat packet from %s:%u!\n", inet_ntoa (from->sin_addr), htons(from->sin_port));
		return FALSE;
	}
	queryPort = (unsigned short)atoi(cmdToken); // FS: Query port
	cmdToken = DK_strtok_r(NULL, seperators, &cmdPtr); // FS: gamename
	if(!cmdPtr || strcmp(cmdToken,"gamename"))
	{
		Con_DPrintf("[E] Invalid heartbeat packet from %s:%u!\n", inet_ntoa (from->sin_addr), htons(from->sin_port));
		return FALSE;
	}
	cmdToken = DK_strtok_r(NULL, seperators, &cmdPtr); // FS: actual gamename

	//walk through known servers
	while (server->next)
	{
		server = server->next;
		//a match!

		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && queryPort == htons(server->port))
		{
			struct sockaddr_in addr;
			char validateString[MAX_GSPY_VAL]; // FS
			int validateStringLen = 0; // FS
			
			memset(validateString, 0, sizeof(validateString));
			memcpy (&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
			addr.sin_family = AF_INET;
			server->port = htons(queryPort);
			addr.sin_port = server->port;
			memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));

			Gamespy_Create_Challenge_Key(server->challengeKey, 6); // FS: Challenge key for this server
			server->challengeKey[6] = '\0'; // FS: Gamespy null terminates the end
			server->last_heartbeat = time(NULL);
			Con_DPrintf ("[I] heartbeat from %s:%u.\n",	inet_ntoa (server->ip.sin_addr), htons(server->port));

			Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
			validateStringLen = DG_strlen(validateString);
			validateString[validateStringLen] = '\0'; // FS: Gamespy null terminates the end
			sendto (listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr)); // FS: Gamespy uses the \status\ data for collection in a database so people can see the current stats without having to really ping the server.
			if(SendAck) // FS: This isn't standard for DK, it will show messages about the ack.  This is more a courtesy to tell the ded server that we received the heartbeat
				sendto (listener, OOB_SEQ"ack", 7, 0, (struct sockaddr *)&addr, sizeof(addr));
			if(statechanged)
				QueueShutdown(&addr, NULL);
			return status;
		}
	}

	//we didn't find server in our list
	status = AddServer (from, 0, htons(queryPort), cmdToken, NULL);
	return status; // false if AddServer failed
}

int ParseResponse (struct sockaddr_in *from, char *data, int dglen)
{
	char *cmd = data;
	char *line = data;
	int	status = TRUE;

#ifdef QUAKE2_SERVERS
	if (_strnicmp (data, "query", 5) == 0 || _strnicmp (data, OOB_SEQ"getservers", 14) == 0 ) // FS: For requesting to old-style Quake 2 master utilites.
	{
		Con_DPrintf ("[I] %s:%d : query (%d bytes)\n",
			inet_ntoa(from->sin_addr),
			htons(from->sin_port),
			dglen);
		
		SendServerListToClient (from);
	}
	else
#endif // QUAKE2_SERVERS
	{
		while (*line && *line != '\n')
			line++;
		
		*(line++) = '\0';

		if(strstr(data, OOB_SEQ)) // FS: Gamespy doesn't send the 0xFF out-of-band.
		{
			cmd +=4;
		}
		else
		{
			cmd +=1;
		}

		Con_DPrintf ("[I] %s: %s:%d (%d bytes)\n",
			cmd,
			inet_ntoa(from->sin_addr),htons(from->sin_port),
			dglen);
		
		if (_strnicmp (cmd, "ping", 4) == 0)
		{
			// FS: Do nothing, gamespy doesn't care about this
//			status = AddServer (from, 1, htons(from->sin_port), "ping");
		}
		else if (_strnicmp (cmd, "heartbeat", 9) == 0) // FS: Gamespy only responds to "heartbeat", print is Q2
		{
			status = HeartBeat (from, cmd);
		}
		else if ( (strncmp (cmd, "ack", 3) == 0))
		{
			Ack (from, data);
		}
		else if (_strnicmp (cmd, "shutdown", 8) == 0)
		{
			QueueShutdown (from, NULL);
		}
		else
		{
//			Con_DPrintf ("[W] Unknown command from %s!\n", inet_ntoa (from->sin_addr));
			// FS: Assume anything else passed in here is some ack from a heartbeat or \\status\\secure\\<key>
			Ack(from, data);
		}
	}
	return status;
}

void ParseCommandLine(int argc, char *argv[])
{
	int i = 0;
	
	if (argc >= 2)
	{
		Debug = 3; //initializing
	}

	for (i = 1; i < argc; i++) 
	{
		if (Debug == 3) 
		{
			if(_strnicmp(argv[i] + 1,"debug", 5) == 0)
				Debug = TRUE;	//service debugged as console
			else
				Debug = FALSE;
		}

		if(_strnicmp((char*)argv[i] + 1,"sendack", 7) == 0) // FS
			SendAck = TRUE;
		else
			SendAck = FALSE;

		if(_strnicmp((char*)argv[i] + 1,"quickvalidate", 13) == 0) // FS
		{
			validate_newserver_immediately = TRUE;
		}

		if(_strnicmp((char*)argv[i] + 1,"validationrequired", 18) == 0) // FS
		{
			validation_required = atoi((char*)argv[i]+20);
		}
		
		if(_strnicmp((char*)argv[i] + 1,"timestamp", 9) == 0) // FS
		{
			Timestamp = atoi((char*)argv[i]+11);
		}

		if(_strnicmp((char*)argv[i] + 1, "heartbeatinterval", 17) == 0)
		{
			heartbeatInterval = atol((char*)argv[i]+19);
			if(heartbeatInterval < 1)
			{
				printf("[W] Heartbeat interval less than one minute!  Setting to one minute.\n");
				heartbeatInterval = 60;
			}
			else
				heartbeatInterval = heartbeatInterval * 60;
		}

		if(_strnicmp((char*)argv[i] + 1, "minimumheartbeats", 17) == 0)
		{
			minimumHeartbeats = atoi((char*)argv[i]+19);
			if(minimumHeartbeats < 1)
			{
				printf("[W] Minimum heartbeat less than one!  Setting to one heartbeat required.\n");
				minimumHeartbeats = 1;
			}
		}

		if(_strnicmp((char*)argv[i] + 1,"ip", 2) == 0)
		{
			//bind_ip, a specific host ip if desired
			DG_strlcpy(bind_ip, (char*)argv[i] + 4, sizeof(bind_ip));
			SetQ2MasterRegKey(REGKEY_BIND_IP, bind_ip);
		}
		
		if(_strnicmp((char*)argv[i] + 1,"port", 4) == 0)
		{
			//bind_port, if other than default port
			DG_strlcpy(bind_port, (char*)argv[i] + 6, sizeof(bind_port));
			SetQ2MasterRegKey(REGKEY_BIND_PORT, bind_port);
		}

		if(_strnicmp((char*)argv[i] + 1,"tcpport", 7) == 0) // FS
		{
			//bind_port_tcp, if other than default TCP port
			DG_strlcpy(bind_port_tcp, (char*)argv[i] + 9, sizeof(bind_port_tcp));
			SetQ2MasterRegKey(REGKEY_BIND_PORT_TCP, bind_port_tcp);
		}

		if(_strnicmp((char*)argv[i] + 1,"serverlist", 10) == 0) // FS
		{
			DG_strlcpy(serverlist_filename, (char *)argv[i] + 12, sizeof(serverlist_filename));
			load_Serverlist = 1;
		}
	}
}

//
// This stuff plus a modified service.c and service.h
// from the Microsoft examples allows this application to be
// installed as a Windows service.
//

#ifdef WIN32

void ServiceCtrlHandler (DWORD Opcode) 
{
    switch(Opcode) 
    { 
	case SERVICE_CONTROL_STOP: 
		// Kill the server loop. 
		runmode = SRV_STOP; // zero the loop control
		while(runmode == SRV_STOP)	//give loop time to die
		{
			int i = 0;
			
			Sleep(500);	// SCM times out in 3 secs.
			i++;		// we check twice per sec.
			if(i >=	6)	// hopefully we beat the SCM timer
				break;	// still no return? rats, terminate anyway
		}
		
		ExitNicely();
		
		MyServiceStatus.dwWin32ExitCode = 0; 
		MyServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
		MyServiceStatus.dwCheckPoint    = 0; 
		MyServiceStatus.dwWaitHint      = 0; 
		
		if(MyServiceStatusHandle)
			SetServiceStatus (MyServiceStatusHandle, &MyServiceStatus);
		return; 
    } 
    // Send current status. 
    SetServiceStatus (MyServiceStatusHandle,  &MyServiceStatus);
} 

void ServiceStart (DWORD argc, LPTSTR *argv) 
{ 
	ParseCommandLine(argc, argv); // we call it here and in My_Main
	
	MyServiceStatus.dwServiceType        = SERVICE_WIN32; 
	MyServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
	MyServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP; 
	MyServiceStatus.dwWin32ExitCode      = 0; 
	MyServiceStatus.dwServiceSpecificExitCode = 0; 
	MyServiceStatus.dwCheckPoint         = 0; 
	MyServiceStatus.dwWaitHint           = 0; 
	
	MyServiceStatusHandle = RegisterServiceCtrlHandler(argv[0], 
		(LPHANDLER_FUNCTION)ServiceCtrlHandler); 
	
	if (!Debug && MyServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) {
		printf("%s not started.\n", SZSERVICEDISPLAYNAME);
		return;
	}
	
	// Initialization complete - report running status. 
	MyServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
	MyServiceStatus.dwCheckPoint         = 0; 
	MyServiceStatus.dwWaitHint           = 0; 
	
	if (!Debug)
		SetServiceStatus (MyServiceStatusHandle, &MyServiceStatus);
	
	My_Main(argc, &argv[0]);
} 

void ServiceStop(void)
{
	ServiceCtrlHandler (SERVICE_CONTROL_STOP);
}

/* 
* This sets the registry keys in "HKLM/Software/Q2MasterServer" so we can tell
* the service what IP address or port to bind to when starting up. If it's not preset
* the service will bind to 0.0.0.0:27900. Not critical on most Windows boxes
* but it can be a pain if you want to use multiple IP's on a NIC and don't want the
* service listening on all of them. Not as painful on Linux, we do the -ip switch
* in the command line.
*/
void SetQ2MasterRegKey(char* name, char *value)
{
	HKEY	hKey;
	DWORD	Disposition;
	LONG	status;
	
	status = RegCreateKeyEx(HKEY_LOCAL_MACHINE, 
		REGKEY_Q2MASTERSERVER,
		0, //always 0
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		&Disposition);
	
	if(status)
		Con_DPrintf("Error creating registry key for %s\n", SZSERVICEDISPLAYNAME);
	
	status = RegSetValueEx(hKey, name, 0, REG_SZ, (unsigned char*)value, DG_strlen(value));
	
	if(status)
		Con_DPrintf("Registry key not set for IP: %s\n", bind_ip);
	
	RegCloseKey(hKey);
}

//
// As as Service, get the key and use the IP address stored there.
// If the key doesn't exist, it will be created.
// The user can add the Bind_IP or Bind_Port value 
// by hand or use the -ip x.x.x.x command line switch.
//
void GetQ2MasterRegKey(char* name, char *value)
{
	HKEY	hKey;
	DWORD	Disposition;
	LONG	status;
	DWORD	size = KEY_LEN;	// expected max size of the bind_ip or bind_port array
	
	// check value, create it if it doesn't exist
	status = RegCreateKeyEx(HKEY_LOCAL_MACHINE, 
		REGKEY_Q2MASTERSERVER,
		0, //always 0
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ,
		NULL,
		&hKey,
		&Disposition);
	
	if(status)
		Con_DPrintf("Registry key not found\n");
	
	status = RegQueryValueEx(hKey, name, NULL, NULL, (unsigned char*)value, &size);

	if(status)
		Con_DPrintf("Registry value not found %s\\%s\n", REGKEY_Q2MASTERSERVER, name);
	
	RegCloseKey(hKey);
}

#else	// not doing windows

//
// handle Linux and BSD signals
//
void signal_handler(int sig)
{
	switch(sig) {
	case SIGHUP:
		break;
	case SIGTERM:
		runmode = SRV_STOP;
		while(runmode == SRV_STOP)	//give loop time to die
		{
			int i = 0;
			
			Sleep(500);	// 500 ms
			i++;		// we check twice per sec.
			if(i >=	6)
				break;	// still no return? rats, terminate anyway
		}
		
		ExitNicely();
		break;
	}
}

#endif

// FS
void Gamespy_Parse_List_Request(char *querystring, int socket, struct sockaddr_in *from)
{
	char *gamename = NULL;
	char *tokenPtr = NULL;
	char seperators[] = "\\";
	int basic = 0;

	if(!querystring || !strstr(querystring, "\\list\\") || !strstr(querystring, "\\gamename\\"))
		goto error;

	if(strstr(querystring,"\\list\\cmp\\gamename\\"))
	{
		gamename = DK_strtok_r(querystring, seperators, &tokenPtr);

		while (strcmp(gamename, "cmp"))
		{
			gamename = DK_strtok_r(NULL, seperators, &tokenPtr); // FS: cmp
		}
		gamename = DK_strtok_r(NULL, seperators, &tokenPtr); // FS: gamename
		gamename = DK_strtok_r(NULL, seperators, &tokenPtr); // FS: actual gamename

		basic = 0;
		Con_DPrintf("[I] Sending TCP list for %s\n", gamename);
	}
	else // FS: Older style that still sent out "basic" style lists
	{
		gamename = DK_strtok_r(querystring, seperators, &tokenPtr);

		while (strcmp(gamename, "list"))
		{
			gamename = DK_strtok_r(NULL, seperators, &tokenPtr); // FS: list
		}
		gamename = DK_strtok_r(NULL, seperators, &tokenPtr); // FS: gamename
		gamename = DK_strtok_r(NULL, seperators, &tokenPtr); // FS: actual gamename

		basic = 1;
		Con_DPrintf("[I] Sending basic TCP list for %s\n", gamename);
	}

	if (!gamename || gamename[0] == 0)
	{
error:
		Con_DPrintf("[I] Invalid TCP list request from %s:%d.  Sending %s string.\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port), finalstringerror);
		send(tcpRetval, finalstringerror, DG_strlen(finalstringerror), 0);
		return;
	}

	gamename = DK_strlwr(gamename); // FS: Some games (mainly sin) stupidly send it partially uppercase

	SendGamespyListToClient(socket, gamename, from, basic);
}

// FS
int Gamespy_Challenge_Cross_Check(char *challengePacket, char *validatePacket, int rawsecurekey)
{
	char *ptr = NULL;
	char validateKey[MAX_INFO_STRING];
	char gameKey[MAX_INFO_STRING];
	char *decodedKey = NULL;
	char *gameSecKey = NULL;
	char challengeKey[MAX_INFO_STRING];

	if(!validation_required) // FS: Just pass it if we want to.
	{
		Con_DPrintf("[I] Skipping validation checks.\n");
		return 1;
	}
	else if(validation_required == 1 && rawsecurekey) // FS: This is an "ack" sent from a heartbeat, dropserver, or addserver
	{
		Con_DPrintf("[I] Skipping server validation checks.\n");
		return 1;
	}
	else if(validation_required == 2 && !rawsecurekey) // FS: This is "list" requests sent from clients
	{
		Con_DPrintf("[I] Skipping client validation checks.\n");
		return 1;
	}

	if(rawsecurekey)
		ptr = challengePacket;
	else
		ptr = Info_ValueForKey(challengePacket, "secure");
	if(!ptr)
	{
		Con_DPrintf("[E] Validation failed.  \\secure\\ missing from packet!\n");
		return 0;
	}
	DG_strlcpy(challengeKey,ptr,sizeof(challengeKey));

	ptr = NULL;
	ptr = Info_ValueForKey(validatePacket, "gamename");
	if(!ptr)
	{
		Con_DPrintf("[E] Validation failed.  \\gamename\\ missing from packet!\n");
		return 0;
	}
	DG_strlcpy(gameKey,ptr,sizeof(gameKey));

	if(!strcmp(gameKey,"nolf") && rawsecurekey) // FS: special hack for NOLF
	{
		Con_DPrintf("[I] NOLF does not respond to \\secure\\ from servers.  Skipping Validation.\n");
		return 1;
	}

	ptr = NULL;
	ptr = Info_ValueForKey(validatePacket, "validate");
	if(!ptr)
	{
		Con_DPrintf("[E] Validation failed.  \\validate\\ missing from packet!\n");
		return 0;
	}
	DG_strlcpy(validateKey,ptr,sizeof(validateKey));

	gameSecKey = Gamespy_Get_Game_SecKey (gameKey);
	if(!gameSecKey)
	{
		Con_DPrintf("[E] Validation failed.  Game not supported!\n");
		return 0;
	}

	decodedKey = (char *)gsseckey(NULL, (unsigned char*)challengeKey, (unsigned char*)gameSecKey, 0);
	if(decodedKey && decodedKey[0] != '\0' && !strcmp(decodedKey, validateKey))
	{
		Con_DPrintf("[I] Validation passed!\n");
		return 1;
	}

	Con_DPrintf("[E] Validation failed.  Incorrect key sent!\n");
	return 0;
}

// FS
void Gamespy_Parse_TCP_Packet (int socket, struct sockaddr_in *from)
{
	int len = 0;
	int lastWSAError = 0;
	int retry = 0;
	int sleepMs = 50;
	int challengeKeyLen = 0;
	int challengeBufferLen = 0;
	char *challengeKey = (char *)malloc(7);
	char *challengeBuffer = (char *)malloc(84);
	char *enctypeKey = NULL;

	memset (incomingTcpValidate, 0, sizeof(incomingTcpValidate));
	memset (incomingTcpList, 0, sizeof(incomingTcpList));
	memset(challengeKey, 0, 7);
	memset(challengeBuffer, 0, 84);
	Gamespy_Create_Challenge_Key(challengeKey, 6); // FS: Generate a random 6 character key to cross-check later

	challengeKeyLen = DG_strlen(challengeKey);
	challengeKey[challengeKeyLen] = '\0'; // FS: Gamespy null terminates the end

	if(TCP_Set_Non_Blocking(socket) == SOCKET_ERROR)
	{
		Con_DPrintf("[E] TCP socket failed to set non-blocking.\n");
		goto closeTcpSocket;
	}

	Con_DPrintf("[I] TCP connection from %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
	Com_sprintf(challengeBuffer, 83, "%s%s", challengeHeader, challengeKey);
	challengeBufferLen = DG_strlen(challengeBuffer);
	challengeBuffer[challengeBufferLen] = '\0';
	len = send(socket, challengeBuffer, challengeBufferLen, 0);
	retry = 0;
	sleepMs = 50;
retryIncomingTcpValidate:
	len = recv(socket, incomingTcpValidate, sizeof(incomingTcpValidate), 0);
	if (len == SOCKET_ERROR)
	{
		lastWSAError = WSAGetLastError();

		if(lastWSAError == WSAEWOULDBLOCK && (retry < totalRetry)) // FS: Yeah, yeah; this sucks.  But, it works OK for our purpose.  If you don't like this, redo it and send me the code.
		{
			retry++;
			Con_DPrintf("[W] Retrying Gamespy TCP Validate Request, Attempt %i of %i.\n", retry, totalRetry);
			Sleep(sleepMs);
			sleepMs = sleepMs + 10;
			goto retryIncomingTcpValidate;
		}
		else // FS: give up
		{
			Close_TCP_Socket_On_Error(socket, from);
			goto closeTcpSocket;
		}
	}

	if (incomingTcpValidate && incomingTcpValidate[0] != 0)
		Con_DPrintf("[I] Incoming Validate: %s\n", incomingTcpValidate);
	else
	{
		Con_DPrintf("[E] Incoming Validate Packet is NULL!\n");
		goto closeTcpSocket;
	}

	// FS: Currently, only the oldest mode, enctype 0 is supported.
	enctypeKey = Info_ValueForKey(incomingTcpValidate, "enctype");
	if(enctypeKey && enctypeKey[0] != 0)
	{
		int encodetype = atoi(enctypeKey);
		if(encodetype > 0)
		{
			Con_DPrintf("[E] Encode Type: %i not supported on this server\n", encodetype);
			goto closeTcpSocket;
		}
	}

	// FS: Not supported currently, or junk data, bye.
	if(!Gamespy_Challenge_Cross_Check(challengeBuffer, incomingTcpValidate, 0))
	{
		goto closeTcpSocket;
	}

	// FS: This is the later version of gamespy which sent it all as one packet.
	if(strstr(incomingTcpValidate,"\\list\\"))
	{
		Gamespy_Parse_List_Request(incomingTcpValidate, socket, from);
		goto closeTcpSocket;
	}
	// FS: If we got here, it's the "basic" version which sends things a little differently (Daikatana, afaik)
	// FS: So grab the packet which contains the list request as well as the gamename
	retry = 0;
	sleepMs = 50;
retryIncomingTcpList:
	len = recv(socket, incomingTcpList, sizeof(incomingTcpList), 0);
	if (len == SOCKET_ERROR)
	{
		if(lastWSAError == WSAEWOULDBLOCK && (retry < totalRetry)) // FS: Yeah, yeah; this sucks.  But, it works OK for our purpose.  If you don't like this, redo it and send me the code.
		{
			retry++;
			Con_DPrintf("[W] Retrying Gamespy TCP List Request, Attempt %i of %i.\n", retry, totalRetry);
			Sleep(sleepMs);
			sleepMs = sleepMs + 10;
			goto retryIncomingTcpList;
		}
		else // FS: give up
		{
			Close_TCP_Socket_On_Error(socket, from);
			goto closeTcpSocket;
		}
	}

	if (len != SOCKET_ERROR)
	{
		if(incomingTcpList && incomingTcpList[0] != 0)
		{
			Con_DPrintf("[I] Incoming List Request: %s\n", incomingTcpList);
		}
		else
		{
			Con_DPrintf("[E] Incoming List Request Packet is NULL!\n");
			goto closeTcpSocket;
		}

		if (len > 4)
		{
			//parse this packet
			if (strstr(incomingTcpList, "\\list\\") && strstr(incomingTcpList, "\\gamename\\")) // FS: We have to have \\list\\ and \\gamename\\ for this to even work
			{
				Gamespy_Parse_List_Request(incomingTcpList, socket, from);
			}
			else
			{
				Con_DPrintf("[I] Invalid TCP list request from %s:%d.  Sending %s string.\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port), finalstringerror);
				send(socket, finalstringerror, DG_strlen(finalstringerror), 0);
			}
		}
		else
		{
			Con_DPrintf ("[W] runt TCP packet from %s:%d\n", inet_ntoa (from->sin_addr), ntohs(from->sin_port));
		}

		//reset for next packet
		goto closeTcpSocket;
	}
	else
	{
		Con_DPrintf ("[E] TCP socket error during select from %s:%d (%d)\n",
			inet_ntoa (from->sin_addr),
			ntohs(from->sin_port),
			WSAGetLastError());
	}
closeTcpSocket:
	//reset for next packet
	free(challengeKey);
	free(challengeBuffer);
	memset (incomingTcpValidate, 0, sizeof(incomingTcpValidate));
	memset (incomingTcpList, 0, sizeof(incomingTcpList));
	closesocket(socket);
	tcpRetval = INVALID_SOCKET;
}

void Add_Servers_From_List(char *filename)
{
	char *fileBuffer = NULL;
	long fileSize;
	FILE *listFile = fopen(filename, "r+");
	size_t toEOF = 0;

	if(!listFile)
	{
		printf("[E] Cannot open file '%s'.\n", filename);
		return;
	}

	fseek(listFile, 0, SEEK_END);
	fileSize = ftell(listFile);
	// FS: If the file size is less than 3 (an emtpy serverlist file) then don't waste time.
	if (fileSize < 3)
	{
		printf("[E] File '%s' is emtpy!\n", filename);
		return;
	}
	else
	{
		fseek(listFile, fileSize-1, SEEK_SET);
	}

	rewind(listFile);
	fileBuffer = (char *)malloc(sizeof(char)*(fileSize+1)); // FS: In case we have to add a newline terminator
	assert(fileBuffer);
	if(!fileBuffer)
	{
		printf("[E] Out of memory!\n");
		return;
	}
	toEOF = fread(fileBuffer, sizeof(char), fileSize, listFile); // FS: Copy it to a buffer
	fclose(listFile);
	if(toEOF <= 0)
	{
		printf("[E] Cannot read file '%s' into memory!\n", filename);
		return;
	}
	// FS: Add newline terminator for some paranoia
	fileBuffer[toEOF] = '\n';
	fileBuffer[toEOF+1] = '\0';

	Parse_ServerList(toEOF, fileBuffer); // FS: Break it up divided by newline terminator
	free(fileBuffer);
}

// FS
void AddServers_From_List_Execute(char *fileBuffer)
{
	char *ip = NULL;
	char *listToken = NULL;
	char *listPtr = NULL;
	char separators[] = ",\n";
	unsigned short queryPort = 0;
	struct hostent *remoteHost;
	struct in_addr addr;
	struct sockaddr_in from;
	size_t ipStrLen = 0;

	listToken = DK_strtok_r(fileBuffer, separators, &listPtr); // IP
	if(!listToken)
	{
		return;
	}

	while(1)
	{
		ipStrLen = DG_strlen(listToken)+2;
		ip = (char *)malloc(sizeof(char)*(ipStrLen));
		if(!ip)
		{
			printf("Memory error in AddServers_From_List_Execute!\n");
			break;
		}
		DG_strlcpy(ip, listToken, ipStrLen);
		remoteHost = gethostbyname(ip);
		// FS: Junk data, or doesn't exist.
		if (!remoteHost)
		{
			Con_DPrintf("[E] Could not resolve '%s' in server list; skipping.\n", ip);
			break;
		}
		addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];

		listToken = DK_strtok_r(NULL, separators, &listPtr); // Port
		if(!listToken)
		{
			Con_DPrintf("[E] Port not specified for '%s' in server list; skipping.\n", ip);
			break;
		}
		queryPort = (unsigned short)atoi(listToken);
		if(atoi(listToken) <= 0 || atoi(listToken) > 65536)
		{
			Con_DPrintf("[E] Invalid Port specified for '%s' in server list; skipping.\n", ip);
			break;
		}

		listToken = DK_strtok_r(NULL, separators, &listPtr); // Gamename
		if(!listToken)
		{
			Con_DPrintf("[E] Gamename not specified for '%s:%u' in server list; skipping.\n", ip, queryPort);
			break;
		}

		memset(&from, 0, sizeof(from));
		from.sin_addr.s_addr = addr.s_addr;
		from.sin_family = AF_INET;
		from.sin_port = htons(queryPort);
		AddServer(&from, 0, htons(queryPort), listToken, ip);
		break;
	}
	free(ip);
}

struct in_addr Hostname_to_IP (struct in_addr *server, char *hostnameIp)
{
	struct hostent *remoteHost;
	struct in_addr addr;
	struct sockaddr_in from;

	memset(&addr, 0, sizeof(addr));
	remoteHost = gethostbyname(hostnameIp);
	// FS: Can't resolve.  Just default to the old IP previously retained so it can be removed later.
	if (!remoteHost)
	{
		Con_DPrintf("Can't resolve %s.  Defaulting to previous known good %s.\n", hostnameIp, inet_ntoa(*server));
		return *server;
	}
	addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];
	from.sin_addr.s_addr = addr.s_addr;

	return addr;
}

// FS
void Check_Port_Boundaries (void)
{
	int udp;
	int tcp;

	if(bind_port[0] != 0)
		udp = atoi(bind_port);

	if(bind_port_tcp[0] != 0)
		tcp = atoi(bind_port_tcp);

	if(bind_port[0] == 0 || udp < 1)
	{
		printf("[W] UDP Port is 0!  Setting to default value of 27900\n");
		SetQ2MasterRegKey(REGKEY_BIND_PORT, "27900");
		DG_strlcpy(bind_port,"27900", 6);
		udp = 27900;
	}
	else if(udp > 65536)
	{
		printf("[W] UDP Port is greater than 65536!  Setting to default value of 27900\n");
		SetQ2MasterRegKey(REGKEY_BIND_PORT, "27900");
		DG_strlcpy(bind_port,"27900", 6);
		udp = 27900;
	}

	if(bind_port_tcp[0] == 0 || tcp < 1)
	{
		printf("[W] TCP Port is 0!  Setting to default value of 28900\n");
		SetQ2MasterRegKey(REGKEY_BIND_PORT_TCP, "28900");
		DG_strlcpy(bind_port_tcp,"28900", 6);
		tcp = 28900;
	}
	else if(tcp > 65536)
	{
		printf("[W] TCP Port is greater than 65536!  Setting to default value of 28900\n");
		SetQ2MasterRegKey(REGKEY_BIND_PORT_TCP, "28900");
		DG_strlcpy(bind_port_tcp,"28900", 6);
		tcp = 28900;
	}

	if(tcp == udp)
	{
		printf("[W] UDP and TCP Ports are the same values!  Setting to defaults.\n");
		SetQ2MasterRegKey(REGKEY_BIND_PORT, "27900");
		SetQ2MasterRegKey(REGKEY_BIND_PORT_TCP, "28900");
		DG_strlcpy(bind_port,"27900", 6);
		DG_strlcpy(bind_port_tcp,"28900", 6);
	}
}
