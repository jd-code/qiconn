#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h> /* clok times */

#include <iomanip>

#define USEDUMMYCONNECTION
#include "qiconn/qiconn.h"

namespace qiconn
{
    using namespace std;

    /*
     *  ---------------------------- simple ostream operators for hostent and sockaddr -----------------------
     */

    ostream& operator<< (ostream& out, const struct hostent &he) {
	cout << he.h_name << " :" << endl;
	int i=0;
	if (he.h_addrtype == AF_INET) {
	    while (he.h_addr_list[i] != NULL) {
		out << (int)((unsigned char)he.h_addr_list[i][0]) << "."
		    << (int)((unsigned char)he.h_addr_list[i][1]) << "."
		    << (int)((unsigned char)he.h_addr_list[i][2]) << "."
		    << (int)((unsigned char)he.h_addr_list[i][3]) << endl;
		i++;
	    }
	} else {
	    cout << " h_addrtype not known" << endl ;
	}
	return out;
    }

//    ostream& operator<< (ostream& out, struct sockaddr_in const &a) {
//	const unsigned char* p = (const unsigned char*) &a.sin_addr;
//	out << (int)p[0] << '.' << (int)p[1] << '.' << (int)p[2] << '.' << (int)p[3];
//	return out;
//    }

    ostream& operator<< (ostream& out, struct sockaddr const &a) {
	switch (a.sa_family) {
	    case AF_INET:
		{   const sockaddr_in & ip4 = *(const sockaddr_in*) &a;
		    const unsigned char* p = (const unsigned char*) &ip4.sin_addr;
		    out << (int)p[0] << '.' << (int)p[1] << '.' << (int)p[2] << '.' << (int)p[3];
		    return out;
		}
		break;
	    case AF_INET6:
		{
		    char buf [INET6_ADDRSTRLEN];
		    const sockaddr_in6 & ip6 = *(const sockaddr_in6*) &a;
		    const char * r = inet_ntop (AF_INET6, (const void *) &ip6.sin6_addr, buf, INET6_ADDRSTRLEN);
		    if (r == NULL) {
			int e = errno;
			cerr << "ostream& operator<< : inet_ntop with sa_family=" << a.sa_family << " triggerred : " << strerror(e) << endl;
		    }
		    out << buf;
		    return out;
		}
		break;
	    default:
		cerr << "ostream& operator<< : don't know what to do with sa_family=" << a.sa_family << endl;
	}
	return out;
    }

    ostream& operator<< (ostream& out, struct sockaddr_storage const &a) {
	return out << (*(const sockaddr*)&a);
    }
    /*
     *  ---------------------------- server_pool : opens a socket for listing a port at a whole --------------
     */

    #define MAX_QUEUDED_CONNECTIONS 5

    int server_pool (int port, const char *addr /* = NULL */, int type /*= AF_INET*/) {
	int s = server_pool_nodefer (port, addr, type);
#if HAVE_SOL_TCP == 1
	{   int yes = 1;
	    if (setsockopt (s, SOL_TCP, TCP_DEFER_ACCEPT, &yes, sizeof (yes)) != 0) {
		int e = errno;
		cerr << "could not setsockopt TCP_DEFER_ACCEPT (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }
	}
#endif
	return s;
    }


//--------------------------------------------------------------------

#include <net/if.h> // if_nametoindex()
#include <ifaddrs.h> // getifaddrs()
#include <netdb.h> // NI_ constants


// returns 0 on error
unsigned getScopeForIp(const char *ip){
    struct ifaddrs *addrs;
    char ipAddress[NI_MAXHOST];
    unsigned scope=0;
    // walk over the list of all interface addresses
    getifaddrs(&addrs);
    for(ifaddrs *addr=addrs;addr;addr=addr->ifa_next){
        if (addr->ifa_addr && addr->ifa_addr->sa_family==AF_INET6){ // only interested in ipv6 ones
            getnameinfo(addr->ifa_addr,sizeof(struct sockaddr_in6),ipAddress,sizeof(ipAddress),NULL,0,NI_NUMERICHOST);
            // result actually contains the interface name, so strip it
            for(int i=0;ipAddress[i];i++){
                if(ipAddress[i]=='%'){
                    ipAddress[i]='\0';
                    break;
                }
            }
            // if the ip matches, convert the interface name to a scope index
            if(strcmp(ipAddress,ip)==0){
                scope=if_nametoindex(addr->ifa_name);
                break;
            }
        }
    }
    freeifaddrs(addrs);
    return scope;
}

//--------------------------------------------------------------------

    int server_pool_nodefer (int port, const char *addr /* = NULL */, int type /*= AF_INET*/) {
	struct sockaddr_storage serv_addr;

	memset (&serv_addr, 0, sizeof(serv_addr));

	switch (type) {
	    case AF_INET:
		{   sockaddr_in &serv_addr_4 = *(sockaddr_in *) &serv_addr;
		    serv_addr_4.sin_family = type;
		    serv_addr_4.sin_port = htons (port);
		    if (addr == NULL) {
			serv_addr_4.sin_addr.s_addr = INADDR_ANY;
		    } else {
			if (inet_aton(addr, &serv_addr_4.sin_addr) == (int)INADDR_NONE) {
			    int e = errno;
			    cerr << "gethostbyaddr (" << addr << " failed : " << strerror (e) << endl;
			    return -1;
			}
		    }
		}
		break;

	    case AF_INET6:
		{   sockaddr_in6 &serv_addr_6 = *(sockaddr_in6 *) &serv_addr;
		    serv_addr_6.sin6_family = type;
		    serv_addr_6.sin6_port = htons (port);
		    serv_addr_6.sin6_port = htons (80);	// JDJDJDJDJD this is weird ????
		    if (addr == NULL) {
			serv_addr_6.sin6_addr = in6addr_any;
		    } else {
			if (inet_pton(AF_INET6, addr, &serv_addr_6.sin6_addr) != 1) {
			    int e = errno;
			    cerr << "gethostbyaddr (" << addr << " failed : " << strerror (e) << endl;
			    return -1;
			}
		    }
serv_addr_6.sin6_scope_id = getScopeForIp (addr);
cerr << "scope for " << addr << " is " << serv_addr_6.sin6_scope_id << endl;
		}
		break;

	    default:
		cerr << "server_pool_nodefer unknown of unhandeled type = " << type << endl;
		return -1;
	}

	int s;
#ifdef SOCK_CLOEXEC
	s = socket(type, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
	s = socket(type, SOCK_STREAM, 0);
#endif
	if (s == -1) {
	    int e = errno;
	    cerr << "could not create socket (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	    return -1;
	}
#ifndef SOCK_CLOEXEC
	{
	long s_flags = 0;
	if (fcntl (s, F_GETFD, s_flags) == -1) {
	    int e = errno;
	    cerr << "could not get socket flags (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	    close (s);
	    return -1;
	}

	s_flags |= FD_CLOEXEC;
	if (fcntl (s, F_SETFD, s_flags)  == -1) {
	    int e = errno;
	    cerr << "could not set socket flags with FD_CLOEXEC (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	    close (s);
	    return -1;
	}
	}
#endif

	{   int yes = 1;
	    if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes)) != 0) {
		int e = errno;
		cerr << "could not setsockopt SO_REUSEADDR (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }
	}

	if (type == AF_INET6) {	// this one is about listening on ipv6 only and not (supposely) * on ipv4 when requesting only ipv6
	int on = 1;
	    if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) != 0) {
			int e = errno;
			cerr << "could not setsockopt IPV6_V6ONLY (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
			return -1;
	    }
	}


	{
	    int buflen;
	    socklen_t param_len = sizeof(buflen);

	// ------------------ SO_SNDBUF
	    if (getsockopt (s, SOL_SOCKET, SO_SNDBUF, &buflen, &param_len) != 0) {
		int e = errno;
		cerr << "could not getsockopt SO_SNDBUF (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }
	//cout << "############### buflen = " << buflen << endl;
	    buflen = 128*1024;
	    if (setsockopt (s, SOL_SOCKET, SO_SNDBUF, &buflen, sizeof(buflen)) != 0) {
		int e = errno;
		cerr << "could not setsockopt SO_SNDBUF (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }

#if HAVE_TCP_WINDOW_CLAMP == 1
	    int clamp = 64*1024;
	    if (setsockopt (s, IPPROTO_TCP, TCP_WINDOW_CLAMP, &clamp, sizeof(clamp)) != 0) {
		int e = errno;
		cerr << "could not setsockopt TCP_WINDOW_CLAMP (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }
#endif


	// ------------------ TCP_MAXSEG
	////    if (getsockopt (s, IPPROTO_TCP, TCP_MAXSEG, &buflen, &param_len) != 0) {
	////	int e = errno;
	////	cerr << "could not getsockopt TCP_MAXSEG (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	////	return -1;
	////    }
	////cout << "############### tcp_maxseg = " << buflen << endl;
	////    buflen = 1500;
	////    if (setsockopt (s, IPPROTO_TCP, TCP_MAXSEG, &buflen, sizeof(buflen)) != 0) {
	////	int e = errno;
	////	cerr << "could not setsockopt TCP_MAXSEG (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	////	return -1;
	////    }



	    int flag = 1;
	    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) != 0) {
		int e = errno;
		cerr << "could not setsockopt TCP_NODELAY=1 (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }

	////    int flag = 0;
	////    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) != 0) {
	////	int e = errno;
	////	cerr << "could not setsockopt TCP_NODELAY=0 (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	////	return -1;
	////    }



	    if (getsockopt (s, SOL_SOCKET, SO_SNDBUF, &buflen, &param_len) != 0) {
		int e = errno;
		cerr << "could not getsockopt SO_SNDBUF (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
		return -1;
	    }
	    //cout << "############### buflen = " << buflen << endl;
	}

	if (bind (s, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) != 0) {
	    int e = errno;
	    cerr << "could not bind socket (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	    return -1;
	}
	if (listen (s, MAX_QUEUDED_CONNECTIONS) != 0) {
	    int e = errno;
	    cerr << "could not listen socket (for listenning connections " << addr << ":" << port << ") : " << strerror (e) << endl ;
	    return -1;
	}
	return s;
    }



    /*
     *  ---------------------------- init_connect : makes a telnet over socket, yes yes ----------------------
     */

    // JDJDJDJD a better thing should be to create an intermediate state after needtoconnect
    // like needtoconnectconfirm in order to wait for writable state (?) and poll for connection establishment

    // init_connect open a tcp session from fqdn and port
    // return a connected socket
    // may fail with -1 on a straightforward fail   (connection refused)
    // may fail with -2 on a slow timed-out failure (port filtered, packet dropped, unreachable machine)
    


    int init_connect (const char *fqdn, int port, struct sockaddr_in *ps /* = NULL */ ) {
	struct hostent * he;
	if (debug_connect) cerr << "init_connect -> gethostbyname (" << fqdn << ")" << endl;
	he = gethostbyname (fqdn);
	if (he != NULL) {
	    if (debug_resolver) cout << "gethostbyname(" << fqdn << ") = " << *he;
	} else
	    return -1;		    // A logger et détailler JDJDJDJD

    //    struct servent *se = getservbyport (port, "tcp");
	
	struct sockaddr_in sin;
	bzero ((char *)&sin, sizeof(sin));

	sin.sin_family = AF_INET;
	bcopy (he->h_addr_list[0], (char *)&sin.sin_addr, he->h_length);
	sin.sin_port = htons(port);

	if (ps != NULL)
	    *ps = sin;
	
	struct protoent *pe = getprotobyname ("tcp");
	if (pe == NULL) {
	    int e = errno;
	    cerr << "could not get protocol entry tcp : " << strerror (e) << endl ;
	    return -1;
	}

	if (debug_connect) cerr << "init_connect -> socket (PF_INET, SOCK_STREAM, tcp)" << endl;
#ifdef SOCK_CLOEXEC
	int s = socket(PF_INET, SOCK_STREAM | SOCK_CLOEXEC, pe->p_proto);
#else
	int s = socket(PF_INET, SOCK_STREAM,                pe->p_proto);
#endif
	if (s == -1) {
	    int e = errno;
	    cerr << "could not create socket (for connection to " << fqdn << ":" << port << ") : " << strerror (e) << endl ;
	    return -1;
	}

	long s_flags = 0;
	if (fcntl (s, F_GETFL, s_flags) == -1) {
	    int e = errno;
	    cerr << "could not get socket flags (for connection to " << fqdn << ":" << port << ") : " << strerror (e) << endl ;
	    close (s);
	    return -1;
	}

	s_flags |= O_NONBLOCK;
	if (fcntl (s, F_SETFL, s_flags)  == -1) {
	    int e = errno;
	    cerr << "could not set socket flags with O_NONBLOCK (for connection to " << fqdn << ":" << port << ") : " << strerror (e) << endl ;
	    close (s);
	    return -1;
	}
#ifndef SOCK_CLOEXEC
	{
	long s_flags = 0;
	if (fcntl (s, F_GETFD, s_flags) == -1) {
	    int e = errno;
	    cerr << "could not get socket desc flags (for connection to " << fqdn << ":" << port << ") : " << strerror (e) << endl ;
	    close (s);
	    return -1;
	}

	s_flags |= FD_CLOEXEC;
	if (fcntl (s, F_SETFD, s_flags)  == -1) {
	    int e = errno;
	    cerr << "could not set socket desc flags with FD_CLOEXEC (for connection to " << fqdn << ":" << port << ") : " << strerror (e) << endl ;
	    close (s);
	    return -1;
	}
	}
#endif
	
	if (debug_connect) cerr << "init_connect -> socket (PF_INET, SOCK_STREAM, tcp)" << endl;

//PROFILECONNECT    timeval startc, clok;
//PROFILECONNECT    gettimeofday (&startc, NULL);

	int r = connect (s, (const struct sockaddr *)&sin, sizeof(sin));

//PROFILECONNECT    gettimeofday (&clok, NULL);
	if (r < 0) {
//PROFILECONNECT    cerr << "first connect fail connect=" << r
//PROFILECONNECT         << " took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
	    int e = errno;
	    if (e == EINPROGRESS /* EINPROGRESS */) {
		time_t start = time (NULL);
		bool connected =false;
		do { 
		    struct timeval tv;
		    fd_set myset;
		    tv.tv_sec = 0; 
		    tv.tv_usec = 10000;
		    FD_ZERO(&myset); 
		    FD_SET(s, &myset); 
		    if (debug_connect) cerr << "init_connect -> select ()" << endl;
		    int res = select(s+1, NULL, &myset, NULL, &tv); 
//PROFILECONNECT    gettimeofday (&clok, NULL);
//PROFILECONNECT    cerr << "select took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
		    if ((res<0) && (errno != EINTR)) { 
			int e = errno;
			cerr << "while selecting after connect attempt (for connection to "
			     << fqdn << ":" << port << ") got : "
			     << strerror (e) << " still attempting..." << endl ;
		    } 
		    else if (res > 0) { 
			// Socket selected for write 
			int valopt;
			socklen_t lon = sizeof(valopt); 
			if (debug_connect) cerr << "init_connect -> getsockopt (SOL_SOCKET, SO_ERROR)" << endl;
			if (getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) { 
			    int e = errno;
			    cerr << "while selecting after connect attempt for (for connection to "
				 << fqdn << ":" << port << ") getsockopt got "
				 << strerror (e) << endl;
//PROFILECONNECT    gettimeofday (&clok, NULL);
//PROFILECONNECT    cerr << "getsockopt (SOL_SOCKET, SO_ERROR) took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
			    close (s);
			    return -1;
			} 

//PROFILECONNECT    gettimeofday (&clok, NULL);
//PROFILECONNECT    cerr << "getsockopt (SOL_SOCKET, SO_ERROR) took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
			// Check the value returned... 
			if (valopt) { 
			    cerr << "while selecting after connect attempt for (for connection to "
				 << fqdn << ":" << port << ") getsockopt returned "
				 << strerror(valopt) << endl;
			    close (s);
			    return -1;
			} else {
			    connected = true;
			    break; 
			}
		    }
		} while ((time(NULL) - start) < 2);	// JDJDJDJD ceci est une valeur arbitraire de 2 secondes !
		if (connected) {
//PROFILECONNECT    gettimeofday (&clok, NULL);
//PROFILECONNECT    cerr << "connected finally took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
		    return s;
		} else {
//PROFILECONNECT    gettimeofday (&clok, NULL);
//PROFILECONNECT    cerr << "not connected timeout took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
		    cerr << "selecting after connect attempt for (for connection to "
			 << fqdn << ":" << port << ") timed out" << endl;
		    close (s);
		    return -2;
		} 
	    } else if (e != 0) {
//PROFILECONNECT    gettimeofday (&clok, NULL);
//PROFILECONNECT    cerr << "not connected straigh error took[" << ((clok.tv_sec - startc.tv_sec) * 10000 + (clok.tv_usec - startc.tv_usec)/100) << "]" << endl;
		cerr << "could not connect to " << fqdn << ":" << port << " : " << strerror (e) << " errno=" << e << endl ;
		close (s);
		return -1;
	    }
	}
	return s;
    }

    /*
     *  ---------------------------- Connection : handles an incoming connection from fd --------------------
     */

    /*
     *	virtual class in order to pool and watch several fds with select later on.
     *	this skeleton provides
     *	    building from a file descriptor and an incoming internet socket address
     *	    building from a file descriptor only (partly broken)
     *	    read is called whene theres some reading found by select
     *	    write is called when the fd requested to watch for write status and is write-ready
     */

    void Connection::close (void) {
	deregister_from_pool ();
	if (fd >= 0) {
	    if (isclosedalready) return;
	    if (::close(fd) != 0) {
		int e = errno;
		cerr << "error closing fd[" << fd << "] for " << getname() << ": " << strerror(e) << endl ;
	    } else {
		fd = -1;
		isclosedalready = true;
	    }
	}
    }

    void Connection::closebutkeepregistered (void) {
	int newfd = fd;
	if (isclosedalready) return;
	if (fd >= 0) {
	    if (::close(fd) != 0) {
		int e = errno;
		cerr << "error closing fd[" << fd << "] for " << getname() << ": " << strerror(e) << endl ;
	    } else {
		newfd = -1;
		isclosedalready = true;
	    }
	}
	notifyfdchange (newfd);
    }

    void Connection::register_into_pool (ConnectionPool *cp, bool readit /* = true */) {
	Connection::readit = readit;
	if (cp != NULL) {
	    Connection::cp = cp;
	    cp->push (this);
	} else if (Connection::cp != cp)
	    cerr << "error: connection [" << getname() << "] already registered to another cp" << endl;
    }

    void Connection::deregister_from_pool () {
	if (cp != NULL) {
	    cp->pull (this);
	    cp = NULL;
	}
    }

    void Connection::notifyfdchange (int newfd) {
	if (cp == NULL) {
	    cerr << "error: notifyfdchange connection [" << getname() << "] isn't registered to any connection pool" << endl;
	    fd = newfd;
//	    if (fd >= 0)
//		isclosedalready = true;	// JDJDJDJD not really true !!!
	    return;
	}
	ConnectionPool *oldcp = cp;
	deregister_from_pool ();
	fd = newfd;
	register_into_pool (oldcp);
    }

    Connection::~Connection (void) {
	if (debug_fddestr) cerr << "destruction of fd[" << fd << "] (deregistering)" << endl ;
	deregister_from_pool ();
	close ();
    }

    void Connection::schedule_for_destruction (void) {
	if (cp != NULL)
	    cp->schedule_for_destruction (this);
	else
	    cerr << "warning : unable to register fd[" << getname() << "]for destrucion because cp=NULL" << endl;
    }


    SocketConnection::SocketConnection (int fd, struct sockaddr_storage const &client_addr)
	: BufConnection (fd, true)
    {
if (fd >= 0)
{
    int buflen;
    socklen_t param_len = sizeof(buflen);
    if (getsockopt (fd, SOL_SOCKET, SO_SNDBUF, &buflen, &param_len) != 0) {
	int e = errno;
	cerr << "could not getsockopt SO_SNDBUF (for SocketConnections " << client_addr << ") : " << strerror (e) << endl ;
    } else
	if (debug_newconnect) cout << "SocketConnection::SocketConnection " << client_addr << " " << buflen << endl;
}
	setname (client_addr);
    }

    void SocketConnection::setname (struct sockaddr_storage const &client_addr) {
	SocketConnection::client_addr = *(const sockaddr_storage*)&client_addr;
	stringstream s;
	s << client_addr;
	name = s.str();
    }

    /*
     *  ---------------------------- select pooling : need to be all in one class --------- JDJDJDJD ---------
     */

    /*
     *  ---------------------------- rought signal treatments ------------------------------------------------
     */

    int caught_signal;

    void signal_handler (int sig) {
// printf ("got signal %d\n", sig);    // JDJDJDJD this should be cleaned !!!
// fprintf (stderr, "got signal %d\n", sig);    // JDJDJDJD this should be cleaned !!!
	caught_signal ++;
	if ((sig > 0) && (sig<254))
	    pend_signals[sig]++;
	else
	    pend_signals[255]++;
    }

    int ConnectionPool::add_signal_handler (int sig) {
	if (SIG_ERR == signal(sig, signal_handler)) {
	    int e = errno;
//	    if (e != 22) {
		cerr << "could not set signal handler for sig=" << sig << " : [" << e << "]" << strerror (e) << endl;
//	    }
	    return 1;
	} else {
//	    cerr << "signal [" << sig << "] handled." << endl;
	    return 0;
	}
    }

    int ConnectionPool::init_signal (void) {
	int i;
	for (i=0 ; i<256 ; i++)
	    pend_signals[i] = 0;
	caught_signal = 0;

	return add_signal_handler (13);
    }

    void ConnectionPool::treat_signal (void) {
	int i;
	for (i=0 ; i<256 ; i++) {
	    if (pend_signals [i] != 0) {
		cerr << "got sig[" << i << "] " << pend_signals [i] << " time" << ((i==1) ? "" : "s") << "." << endl;
		pend_signals [i] = 0;
	    }
	}
	caught_signal = 0;
    }

    ConnectionPool::ConnectionPool (void) {
	debug_multiple_scheddestr = false;
	biggest_fd = 0;
	exitselect = false;
	scheddest = false;
	FD_ZERO (&w_fd);
	tnextspoll = 0;
    }


    void ConnectionPool::build_r_fd (void) {
	MConnections::iterator mi;
	FD_ZERO (&r_fd);
	for (mi=connections.begin() ; mi!=connections.end() ; mi++)
	    if ((mi->first >= 0) && (mi->second->readit)) FD_SET (mi->first, &r_fd);
    }

    int ConnectionPool::set_biggest (void) {
	if (connections.empty())
	    biggest_fd = 0;
	else {
	    biggest_fd = connections.begin()->first + 1;
	    if (biggest_fd < 0)
		biggest_fd = 0;
	}
	return biggest_fd;
    }

    void ConnectionPool::schedule_for_destruction (Connection * c) {
	if (connections.find (c->fd) == connections.end()) {
	    cerr << "warning: we were asked for destroying some unregistered connection[" << c->getname() << "]" << endl;
	    return;
	}
	if (debug_multiple_scheddestr) {
	    map<Connection*,int>::iterator mi = destroy_schedule.find(c);
	    if (mi == destroy_schedule.end()) {
		destroy_schedule[c] = 0;
	    } else {
cerr << "Connection " << mi->first->gettype() << "::" << mi->first->getname() << " schedulled for destruction more than once !" << endl;
		mi->second ++;
	    }
	} else {
	    destroy_schedule[c]++;
	}
	scheddest = true;
    }

    bool ConnectionPool::schedule_next_spoll (Connection * c, time_t when, TOccurences occurence, int jitter /* = 0 */) {
	if (c == NULL) return false;
	if (when < 2) when = 2;
	time_t now = time(NULL);
	time_t jit = (int)(((jitter * when) / 100) * ((double)rand())/RAND_MAX);

// JDJDJDJD JITTER MISSING
	spollsched.insert (pair<time_t, SPollEvent> (now+when+jit, SPollEvent(occurence, c, when, jitter)));
//	spollsched.emplace (now+when, occurence, c, when, jitter);
//	spollsched [now+when] = SPollEvent (occurence, c, when, jitter);
	c->spollchedulled ++;
	time_t min_nextspoll = spollsched.begin()->first;
	if (tnextspoll == 0)
	    tnextspoll = min_nextspoll;
	else if (min_nextspoll < tnextspoll)
	    tnextspoll = min_nextspoll;
	return true;
    }


    void ConnectionPool::unschedule_spoll (MConnections::iterator mci) {
	Connection *c = mci->second;
	multimap <time_t, SPollEvent>::iterator mi, mj;
	for (   mi = spollsched.begin() ;
		(mi != spollsched.end()) && (c->spollchedulled >0) ;
	    ) {
	    mj = mi;
	    mi++;
	    if (mj->second.pconn == c) {
		spollsched.erase (mj);
		c->spollchedulled --;
	    }
	}
    }

    void ConnectionPool::checklaunchspoll (void) {
	time_t now = time(NULL);
	if (now < tnextspoll) return;
	multimap <time_t, SPollEvent>::iterator mi, mj;

	for (	mi = spollsched.begin() ;
		(mi != spollsched.end()) && (mi->first <=now) ;
	    ) {
	    mi->second.pconn->schedpoll ();
	    
	    mj = mi;
	    if (mj->second.occurence == forever) {
		time_t after = time(NULL);
		int jitter = mi->second.jitter;
		time_t when = mi->second.delay;
		time_t jit = (int)(((jitter * when) / 100) * ((double)rand())/RAND_MAX);
		spollsched.insert (pair<time_t, SPollEvent> (after+when+jit, SPollEvent(mi->second.occurence, mi->second.pconn, when, jitter)));
//		spollsched.emplace (now+when, occurence, c, when, jitter);
//		spollsched [now+when] = SPollEvent (occurence, c, when, jitter);
	    }
	    mi++;
	    mj->second.pconn->spollchedulled --;
	    spollsched.erase (mj);
	}
	if (spollsched.empty())
	    tnextspoll = 0;
	else
	    tnextspoll = spollsched.begin()->first;
    }

    int ConnectionPool::select_poll (struct timeval *timeout) {
	if (scheddest) {
	    map<Connection*,int>::iterator li;
	    for (li=destroy_schedule.begin() ; li!=destroy_schedule.end() ; li++)
		delete (li->first);
	    //destroy_schedule.erase (destroy_schedule.begin(), destroy_schedule.end());
	    destroy_schedule.clear ();
	    scheddest = false;
	}

	MConnections::iterator mi;
	for (mi=connections.begin() ; mi!=connections.end() ; ) {
	    MConnections::iterator mj = mi;
	    mi++;
	    mj->second->poll();	    // using mj, in case poll calls some self-deregistration ...
	}
	
	fd_set cr_fd = r_fd,
	       cw_fd = w_fd;
	select (biggest_fd, &cr_fd, &cw_fd, NULL, timeout);

//	FD_ZERO (&w_fd);    // JDJDJDJD replaced below by more accurate FD_CLR

	if (caught_signal)
	    treat_signal ();
	else {
	    int i;
	    for (i=0 ; i<biggest_fd ; i++) {
		// we test both, because there is a suspicion of
		// erased connexions between cr_fd build and now !
		//		alternative:    if (connections.find(i) != connections.end())
		if ((FD_ISSET(i, &r_fd)) && (FD_ISSET(i, &cr_fd))) {
			connections[i]->effread();
		}
	    }
	    for (i=0 ; i<biggest_fd ; i++) {
		// the double check is for connection which are unreqw in between ...
		if (FD_ISSET(i, &cw_fd) && FD_ISSET(i, &w_fd)) {
		    FD_CLR (i, &w_fd);
		    connections[i]->effwrite();
		}
	    }
	    if (tnextspoll > 0)
		checklaunchspoll ();
	}
	if (exitselect)
	    return 1;
	else
	    return 0;
    }

    int ConnectionPool::select_loop (const struct timeval & timeout) {
	struct timeval t_out = timeout;
	build_r_fd ();
//	FD_ZERO (&w_fd);    that was a bug to erase here ... erase at construction rather ...

	while (select_poll(&t_out) == 0)  {
	    t_out = timeout;
	}
	return 0;
    }

    void ConnectionPool::closeall (void) {
if (true) {
	MConnections::iterator mi;
	cerr << "entering ConnectionPool::closeall..." << endl;
	for (mi=connections.begin() ; mi!=connections.end() ; ) {
	    MConnections::iterator mj = mi;
	    mi ++;
	    mj->second->close();
	}
	cerr << "...done." << endl;
	    
} else {    // this is the former code, it appears that going backward
	    // with iterator while deleting doesn't work ...

	MConnections::reverse_iterator rmi;
	cerr << "entering ConnectionPool::closeall..." << endl;
	for (rmi=connections.rbegin() ; rmi!=connections.rend() ; ) {
	    MConnections::reverse_iterator rmj = rmi;	// we do this because Connection::close deregisters from the map !
	    rmi ++;
cerr << "closing " << rmj->second->getname() << endl;
	    rmj->second->close();
cerr << "done." << endl << endl;
	}
	cerr << "...done." << endl;
}
    }

    void ConnectionPool::destroyall (void) {
	MConnections::reverse_iterator rmi;
	cerr << "entering ConnectionPool::destroyall..." << endl;
	for (rmi=connections.rbegin() ; rmi!=connections.rend() ; rmi++) {
	    rmi->second->schedule_for_destruction();
	}
	cerr << "...done." << endl;
    }

    ConnectionPool::~ConnectionPool (void) {
	closeall ();
    }


    //! pushes some Connection into the map of active connection via this ConnectionPool
    //! if the Connection fd is negative, the fd is changed to a unique negative 
    //! suitable value
    
    void ConnectionPool::push (Connection *c) {
	if (c->cp == NULL) {
	    c->cp = this;
	} else if (c->cp != this) {
	    cerr << "warning: connection[" << c->fd << ":" << c->getname() << "] already commited to another cp !" << endl;
	    return;
	}
	if (c->fd < 0) {  // we're setting a pending connection...
//WAS:	if (c->fd == -1) {  // we're setting a pending connection...

// cerr << "ici" << endl;
	    if (connections.empty()) {
// cerr << "set -2 because empty" << endl;
		c->fd = -2;
	    } else {
		c->fd = connections.rbegin()->first - 1;    // MConnections is reverse-ordered !! (and needs to be)
// cerr << "not empty so calculuus gives :" << c->fd << endl;
// {   MConnections::iterator mi;
//     for (mi=connections.begin() ; mi!=connections.end() ; mi++)
// 	cerr << "    fd[" << mi->first << "] used" << endl;
// }
		if (c->fd >= -1)
		    c->fd = -2;
// cerr << "not empty final value :" << c->fd << endl;
	    }
	}
	MConnections::iterator mi = connections.find (c->fd);
	if (mi == connections.end()) {
	    connections[c->fd] = c;
	    set_biggest ();
	    build_r_fd ();
	    reqw (c->fd);	/* we ask straightforwardly for write for welcome message */
	} else {
	    cerr << "warning: connection[" << c->getname() << ", fd=" << c->fd << "] was already in pool ???" << endl;
	}
    }

    void ConnectionPool::pull (Connection *c) {
	int fd = c->fd;
	MConnections::iterator mi = connections.find (fd);
	if (mi != connections.end()) {
	    if (c->spollchedulled > 0) {
		unschedule_spoll (mi);
		if (c->spollchedulled > 0) {
		    cerr << "warning: connection[" << c->getname() << ", fd=" << c->fd << "] "
			"still has some spool count after de-spollscheduling : spollchedulled="
			 << c->spollchedulled << endl;
		}
	    }

	    if (fd >=0 ) {
		reqnor (fd);
		reqnow (fd);
	    }

	    connections.erase (mi);
	    set_biggest ();
	    build_r_fd ();
	} else {
	    cerr << "warning: ConnectionPool::pull : cannot pull {" << c->getname() << "} !" << endl;
	}
    }

    ostream& operator<< (ostream& cout, ConnectionPool const& cp) {
	return cp.dump (cout);
    }

    ostream& ConnectionPool::dump (ostream& cout) const {
	MConnections::const_iterator mi;
	size_t ioactive = 0;
	for (mi=connections.begin() ; mi!=connections.end() ; mi++) {
	    int fd = mi->first;
	    cout << "    " 
		 << setw(5) << setfill (' ') << fd << " ";
	    cout << (mi->second->readit ? 'r' : ' ')
		 << (mi->second->issocket ? 'S' : ' ')
		 << (mi->second->isclosedalready ? 'C' : ' ');
	    if (fd >= 0) {
		ioactive ++;
		cout << (FD_ISSET(fd, &opened) ? 'O' : ' ')
		     << (FD_ISSET(fd, &r_fd) ? 'R' : ' ')
		     << (FD_ISSET(fd, &w_fd) ? 'W' : ' ');
	    } else {
		cout << "   ";
	    }
	    cout << " " << mi->second->gettype() << " " << mi->second->getname() << endl;
	}
	return cout << connections.size() << " connections (" << ioactive << " i/o-actives)" << endl;
    }

    /*
     *  ---------------------------- BufConnection : let's put some line buffering on top of Connection
     */

    #define BUFLEN 32768
    BufConnection::~BufConnection (void) {
    };

    BufConnection::BufConnection (int fd, bool issocket) : Connection (fd, issocket) {
	corking = false;
	raw = false;
	pdummybuffer = NULL;
	givenbuffer = false;
	givenbufferiswaiting = false;
	destroyatendofwrite = false;
	out = new stringstream ();
	if (out == NULL) {
	    int e = errno;
cerr << "BufConnection::BufConnection : could not allocate stringstream ? : " << strerror (e) << endl;
	    schedule_for_destruction();
	}
	maxpendsize = string::npos;
	wpos = 0;
    }

    void BufConnection::setrawmode (void) {
	raw = true;
    }

    void BufConnection::setlinemode (void) {
	raw = false;
    }

    void BufConnection::setmaxpendsize (size_t l) {
	maxpendsize = l;
    }

    size_t BufConnection::read (void) {
	char s[BUFLEN];
	ssize_t n = ::read (fd, (void *)s, BUFLEN);
	
	if (n == -1) {
	    int e = errno;
	    cerr << "read(" << fd << ") error : " << strerror (e) << endl;
	    return 0;
	}

	if (debug_transmit) {
	    int i;
	    clog << "fd=" << fd << "; ";
	    for (i=0 ; i<n ; i++)
		clog << s[i];
	    clog << endl;
	}
if (debug_dummyin)
{   int i;
    cerr << "BufConnection::read got[";
    for (i=0 ; i<n ; i++)
	cerr << s[i];
    cerr << endl;
}
	int i;
	for (i=0 ; i<n ; i++) {
	    if (!raw) {
		if ((s[i]==10) || (s[i]==13) || s[i]==0) {
		    if (i+1<n) {
			if ( ((s[i]==10) && (s[i+1]==13)) || ((s[i]==13) && (s[i+1]==10)) )
			    i++;
		    }
if (debug_lineread) {
    cerr << "BufConnection::read->lineread(" << bufin << ")" << endl;
}
		    lineread ();
		    bufin.clear();
		} else
		    bufin += s[i];
	    } else {
		bufin += s[i];
	    }
	}
	if (raw) {
if (debug_lineread) {
    cerr << "BufConnection::read->lineread(" << bufin << ")" << endl;
}
	    lineread ();
	    bufin.clear();
	} else if ((maxpendsize != string::npos) && (bufin.size() > maxpendsize)) {
	    cerr << "BufConnection::read " << gettype() << "::" << getname() << " bufin.size=" << bufin.size() << " > " << maxpendsize << " : closing connection" << endl;
	    schedule_for_destruction();
	}
	if (n==0) {
	    if (debug_newconnect) cerr << "read() returned 0. we may close the fd[" << fd << "] ????" << endl;
	    reconnect_hook();
	}
	return (size_t) n;
    }

    void BufConnection::reconnect_hook (void) {
	    schedule_for_destruction();
    }

//    void BufConnection::lineread (void) {
//	string::iterator si;
//	for (si=bufin.begin() ; si!=bufin.end() ; si++) {
//	    (*out) << setw(2) << setbase(16) << setfill('0') << (int)(*si) << ':' ;
//	}
//	(*out) << " | " << bufin << endl;
//	flush ();
//
//	if (bufin.find("shut") == 0) {
//	    if (cp != NULL) {
//		cerr << "shutting down on request from fd[" << getname() << "]" << endl;
//		cp->tikkle();
//		// cp->closeall();
//		// exit (0);
//	    }
//	    else
//		cerr << "could not shut down from fd[" << getname() << "] : no cp registered" << endl;
//	}
//
//	if (bufin.find("wall ") == 0) {
//	    list<BufConnection *>::iterator li;
//	    int i = 0;
//	    for (li=ldums.begin() ; li!=ldums.end() ; li++) {
//		(*(*li)->out) << i << " : " << bufin.substr(5) << endl;
//		(*li)->flush();
//		i++;
//	    }
//	}
//    }

    void BufConnection::flush(void) {
	if (cp != NULL)
	    cp->reqw (fd);
	bufout += out->str();
if (debug_dummyout) {
    cerr << "                                                                                      ->out=" << out->str() << endl ;
}
	delete (out); 
	out = new stringstream ();
    }


    void BufConnection::eow_hook (void) {
#if HAVE_TCP_CORK == 1
	if (corking && issocket && (fd >=0)) {
if (debug_corking) cout << "fd[" << fd << "] >> uncorking" << endl;
	    int flag = 0;		// JDJDJDJD uncork
	    if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(int)) != 0) {
	        int e = errno;
	        cerr << "could not setsockopt TCP_CORK=0 (for bufconnections " << fd << ") : " << strerror (e) << endl ;
	    }
	}
#endif
    }

    void BufConnection::cork (void) {
#if HAVE_TCP_CORK == 1
	if (corking && issocket && (fd >=0)) {
if (debug_corking) cout << "fd[" << fd << "] || corking" << endl;
	    int flag = 1;		// JDJDJDJD uncork
	    if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(int)) != 0) {
	        int e = errno;
	        cerr << "could not setsockopt TCP_CORK=1 (for bufconnections " << fd << ") : " << strerror (e) << endl ;
	    }
	}
#endif
    }

    void BufConnection::flushandclose(void) {
	destroyatendofwrite = true;
	flush();
    }

    int BufConnection::pushdummybuffer (DummyBuffer* pdb) {
	if (givenbufferiswaiting || givenbuffer) {
	    cerr << "BufConnection::pushdummybuffer : silly attempt to push a buffer onto another" << endl;
	    cerr << "     givenbufferiswaiting = " << (givenbufferiswaiting ? "true" : "false") << endl;
	    cerr << "     givenbuffer = " << (givenbuffer ? "true" : "false") << endl;
	    // JDJDJDJD ce serait pourtant tres pratique !!!! a faire ?
	    return -1;
	}
	givenbufferiswaiting = true;
	pdummybuffer = pdb;
//	if (cp != NULL)
//	    cp->reqw (fd);
	return 0;
    }

    size_t BufConnection::write (void) {
	ssize_t nt = 0;
	if (givenbuffer) {
	    ssize_t size = pdummybuffer->length - wpos,
		   nb;
	    if (size == 0) {
		givenbuffer = false;
		if (!bufout.empty()) {	// JDJDJDJD UGGLY !
		    if (cp != NULL)
			cp->reqw (fd);
		}
		delete (pdummybuffer);
		reachedeow = true;
		if (bufout.empty() && destroyatendofwrite)
		    schedule_for_destruction ();
		return 0;
	    }

	    if (size > BUFLEN) {
		if (cp != NULL)
		    cp->reqw (fd);
		nb = BUFLEN;
	    } else {
		nb = size;
	    }
	    nt = ::write (fd, pdummybuffer->start+wpos, nb);
	    if (nt != -1) {
		wpos += nt;
		if (nt != nb) {
		    cerr << "some pending chars" << endl;
		    if (cp != NULL)
			cp->reqw (fd);
		}
	    } else {
		int e = errno;

		if (e == EPIPE) {   /* we can assume the connection is shut (and wasn't detected by read) */
		    reconnect_hook();
		} else {
		    cerr << "error writing via givenbuffer to fd[" << fd << ":" << getname() << "] : (" << e << ") " << strerror (e) << endl ;
		}
	    }
	    if (wpos == (size_t)pdummybuffer->length) {
		wpos = 0;

		givenbuffer = false;
		if (!bufout.empty()) {	// JDJDJDJD UGGLY !
		    if (cp != NULL)
			cp->reqw (fd);
		}
		delete (pdummybuffer);
		reachedeow = true;
		if (bufout.empty() && destroyatendofwrite)
		    schedule_for_destruction ();
	    }
// =================================================================================
	} else {
	    ssize_t size = (ssize_t)bufout.size() - wpos,
		   nb;
	    if (size == 0) {
		if (givenbufferiswaiting) {
		    givenbuffer = true;
		    givenbufferiswaiting = false;
		    if (cp != NULL)
			cp->reqw (fd);
		} else {
		    reachedeow = true;
		    if (destroyatendofwrite)
			schedule_for_destruction ();
		}
		return 0;
	    }

	    if (size > BUFLEN) {
		if (cp != NULL)
		    cp->reqw (fd);
		nb = BUFLEN;
	    } else {
		nb = size;
	    }
	    nt = ::write (fd, bufout.c_str()+wpos, nb);
	    if (nt != -1) {
		wpos += nt;
		if (nt != nb) {
		    cerr << "some pending chars" << endl;
		    if (cp != NULL)
			cp->reqw (fd);
		}
	    } else {
		int e = errno;

		if (e == EPIPE) {   /* we can assume the connection is shut (and wasn't detected by read) */
		    reconnect_hook();
		} else {
		    cerr << "error writing to fd[" << fd << ":" << getname() << "] : (" << e << ") " << strerror (e) << endl ;
		}
	    }
	    if (wpos == bufout.size()) {
		wpos = 0;
		bufout = "";
		if (givenbufferiswaiting) {
		    givenbuffer = true;
		    givenbufferiswaiting = false;
		    if (cp != NULL)
			cp->reqw (fd);
		} else {
		    reachedeow = true;
		    if (destroyatendofwrite)
			schedule_for_destruction ();
		}
	    }
	}
	return (nt > 0) ? nt : 0;
    }


#ifdef USEDUMMYCONNECTION
    DummyConnection::~DummyConnection (void) {};
#endif

//    /*
//     *  ---------------------------- DummyConnection : connection that simply echo in hex what you just typed 
//     */
//
//    #define BUFLEN 1024
//    DummyConnection::~DummyConnection (void) {
//	ldums.erase (me);
//    };
//
//    DummyConnection::DummyConnection (int fd, struct sockaddr_in const &client_addr) : SocketConnection (fd, client_addr) {
//	raw = false;
//	pdummybuffer = NULL;
//	givenbuffer = false;
//	givenbufferiswaiting = false;
//	destroyatendofwrite = false;
//	out = new stringstream ();
//	wpos = 0;
//	ldums.push_back (this);
//	me = ldums.end();
//	me--;
//    }
//
//    void DummyConnection::setrawmode (void) {
//	raw = true;
//    }
//
//    void DummyConnection::setlinemode (void) {
//	raw = false;
//    }
//
//    void DummyConnection::read (void) {
//	char s[BUFLEN];
//	ssize_t n = ::read (fd, (void *)s, BUFLEN);
//	
//	if (debug_transmit) {
//	    int i;
//	    clog << "fd=" << fd << "; ";
//	    for (i=0 ; i<n ; i++)
//		clog << s[i];
//	    clog << endl;
//	}
//if (debug_dummyin)
//{   int i;
//    cerr << "DummyConnection::read got[";
//    for (i=0 ; i<n ; i++)
//	cerr << s[i];
//    cerr << endl;
//}
//	int i;
//	for (i=0 ; i<n ; i++) {
//	    if (!raw) {
//		if ((s[i]==10) || (s[i]==13) || s[i]==0) {
//		    if (i+1<n) {
//			if ( ((s[i]==10) && (s[i+1]==13)) || ((s[i]==13) && (s[i+1]==10)) )
//			    i++;
//		    }
//if (debug_lineread) {
//    cerr << "DummyConnection::read->lineread(" << bufin << ")" << endl;
//}
//		    lineread ();
//		    bufin = "";
//		} else
//		    bufin += s[i];
//	    } else {
//		bufin += s[i];
//	    }
//	}
//	if (raw) {
//if (debug_lineread) {
//    cerr << "DummyConnection::read->lineread(" << bufin << ")" << endl;
//}
//	    lineread ();
//	    bufin = "";
//	}
//	if (n==0) {
//	    cerr << "read() returned 0. we may close the fd[" << fd << "] ????" << endl;
//	    reconnect_hook();
//	}
//    }
//
//    void DummyConnection::reconnect_hook (void) {
//	    schedule_for_destruction();
//    }
//
//    void DummyConnection::lineread (void) {
//	string::iterator si;
//	for (si=bufin.begin() ; si!=bufin.end() ; si++) {
//	    (*out) << setw(2) << setbase(16) << setfill('0') << (int)(*si) << ':' ;
//	}
//	(*out) << " | " << bufin << endl;
//	flush ();
//
//	if (bufin.find("shut") == 0) {
//	    if (cp != NULL) {
//		cerr << "shutting down on request from fd[" << getname() << "]" << endl;
//		cp->tikkle();
//		// cp->closeall();
//		// exit (0);
//	    }
//	    else
//		cerr << "could not shut down from fd[" << getname() << "] : no cp registered" << endl;
//	}
//
//	if (bufin.find("wall ") == 0) {
//	    list<DummyConnection *>::iterator li;
//	    int i = 0;
//	    for (li=ldums.begin() ; li!=ldums.end() ; li++) {
//		(*(*li)->out) << i << " : " << bufin.substr(5) << endl;
//		(*li)->flush();
//		i++;
//	    }
//	}
//    }
//
//    void DummyConnection::flush(void) {
//	if (cp != NULL)
//	    cp->reqw (fd);
//	bufout += out->str();
//if (debug_dummyout) {
//    cerr << "                                                                                      ->out=" << out->str() << endl ;
//}
//	delete (out); 
//	out = new stringstream ();
//    }
//
//    void DummyConnection::flushandclose(void) {
//	destroyatendofwrite = true;
//	flush();
//    }
//
//    int DummyConnection::pushdummybuffer (DummyBuffer* pdb) {
//	if (givenbufferiswaiting || givenbuffer) {
//	    cerr << "DummyConnection::pushdummybuffer : silly attempt to push a buffer onto another" << endl;
//	    cerr << "     givenbufferiswaiting = " << (givenbufferiswaiting ? "true" : "false") << endl;
//	    cerr << "     givenbuffer = " << (givenbuffer ? "true" : "false") << endl;
//	    // JDJDJDJD ca devrait pouvoir ce faire pourtant !
//	    return -1;
//	}
//	givenbufferiswaiting = true;
//	pdummybuffer = pdb;
////	if (cp != NULL)
////	    cp->reqw (fd);
//	return 0;
//    }
//
//    void DummyConnection::write (void) {
//	if (givenbuffer) {
//	    ssize_t size = pdummybuffer->length - wpos,
//		   nb;
//	    if (size == 0) {
//		givenbuffer = false;
//		if (!bufout.empty()) {	// JDJDJDJD UGGLY !
//		    if (cp != NULL)
//			cp->reqw (fd);
//		}
//		delete (pdummybuffer);
//		if (bufout.empty() && destroyatendofwrite)
//		    schedule_for_destruction ();
//		return;
//	    }
//
//	    if (size > BUFLEN) {
//		if (cp != NULL)
//		    cp->reqw (fd);
//		nb = BUFLEN;
//	    } else {
//		nb = size;
//	    }
//	    ssize_t nt = ::write (fd, pdummybuffer->start+wpos, nb);
//	    if (nt != -1) {
//		wpos += nt;
//		if (nt != nb) {
//		    cerr << "some pending chars" << endl;
//		    if (cp != NULL)
//			cp->reqw (fd);
//		}
//	    } else {
//		int e = errno;
//
//		if (e == EPIPE) {   /* we can assume the connection is shut (and wasn't detected by read) */
//		    reconnect_hook();
//		} else {
//		    cerr << "error writing via givenbuffer to fd[" << fd << ":" << getname() << "] : (" << e << ") " << strerror (e) << endl ;
//		}
//	    }
//	    if (wpos == (size_t)pdummybuffer->length) {
//		wpos = 0;
//
//		givenbuffer = false;
//		if (!bufout.empty()) {	// JDJDJDJD UGGLY !
//		    if (cp != NULL)
//			cp->reqw (fd);
//		}
//		delete (pdummybuffer);
//		if (bufout.empty() && destroyatendofwrite)
//		    schedule_for_destruction ();
//		return;
//
//	    }
//// =================================================================================
//	} else {
//	    ssize_t size = (ssize_t)bufout.size() - wpos,
//		   nb;
//	    if (size == 0) {
//		if (givenbufferiswaiting) {
//		    givenbuffer = true;
//		    givenbufferiswaiting = false;
//		    if (cp != NULL)
//			cp->reqw (fd);
//		} else if (destroyatendofwrite) {
//		    schedule_for_destruction ();
//		}
//		return;
//	    }
//
//	    if (size > BUFLEN) {
//		if (cp != NULL)
//		    cp->reqw (fd);
//		nb = BUFLEN;
//	    } else {
//		nb = size;
//	    }
//	    ssize_t nt = ::write (fd, bufout.c_str()+wpos, nb);
//	    if (nt != -1) {
//		wpos += nt;
//		if (nt != nb) {
//		    cerr << "some pending chars" << endl;
//		    if (cp != NULL)
//			cp->reqw (fd);
//		}
//	    } else {
//		int e = errno;
//
//		if (e == EPIPE) {   /* we can assume the connection is shut (and wasn't detected by read) */
//		    reconnect_hook();
//		} else {
//		    cerr << "error writing to fd[" << fd << ":" << getname() << "] : (" << e << ") " << strerror (e) << endl ;
//		}
//	    }
//	    if (wpos == bufout.size()) {
//		wpos = 0;
//		bufout = "";
//		if (givenbufferiswaiting) {
//		    givenbuffer = true;
//		    givenbufferiswaiting = false;
//		    if (cp != NULL)
//			cp->reqw (fd);
//		} else if (destroyatendofwrite) {
//		    schedule_for_destruction ();
//		}
//	    }
//	}
//    }

    /*
     *  ---------------------------- ListeningSocket : the fd that watches incoming cnx ----------------------
     */


    int ListeningSocket::addconnect (int socket) {
	struct sockaddr_storage client_addr;
	socklen_t size_addr = sizeof(client_addr);
#ifdef SOCK_CLOEXEC
	int f = accept4 ( socket, (struct sockaddr *) &client_addr, &size_addr, SOCK_CLOEXEC );
#else
	int f = accept  ( socket, (struct sockaddr *) &client_addr, &size_addr);
#endif
	if (f < 0) {
	    int e = errno;
	    cerr << "could not accept connection : " << strerror (e) << endl ;
	    return -1;
	}
#ifndef SOCK_CLOEXEC
	{
	long s_flags = 0;
	if (fcntl (f, F_GETFD, s_flags) == -1) {
	    int e = errno;
	    cerr << "could not get socket flags (for accepting connection) : " << strerror (e) << endl ;
	    ::close (f);
	    return -1;
	}

	s_flags |= FD_CLOEXEC;
	if (fcntl (f, F_SETFD, s_flags)  == -1) {
	    int e = errno;
	    cerr << "could not set socket flags with FD_CLOEXEC (for accepting connection) : " << strerror (e) << endl ;
	    ::close (f);
	    return -1;
	}
	}
#endif
{
    int flag = 1;
    if (setsockopt(f, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) != 0) {
	int e = errno;
	cerr << "could not setsockopt TCP_NODELAY=1 (for accepting connection) : " << strerror (e) << endl ;
    }

#if HAVE_TCP_WINDOW_CLAMP == 1
    int clamp = 64*1024;
    if (setsockopt (f, IPPROTO_TCP, TCP_WINDOW_CLAMP, &clamp, sizeof(clamp)) != 0) {
	int e = errno;
	cerr << "could not setsockopt TCP_WINDOW_CLAMP (for accepting connection) : " << strerror (e) << endl ;
    }
#endif
}
	{
	long s_flags = 0;
	if (fcntl (f, F_GETFL, s_flags) == -1) {
	    int e = errno;
	    cerr << "could not get socket flags (for accepting connection) : " << strerror (e) << endl ;
	    ::close (f);
	    return -1;
	}

	s_flags |= O_NONBLOCK;
	if (fcntl (f, F_SETFL, s_flags)  == -1) {
	    int e = errno;
	    cerr << "could not set socket flags with O_NONBLOCK (for accepting connection) : " << strerror (e) << endl ;
	    ::close (f);
	    return -1;
	}
	}


	if (debug_newconnect) cerr << "new connection from fd[" << f << ":" << client_addr << "]" << endl;
	if (cp != NULL) {
	    SocketConnection * pdc = connection_binder (f, client_addr);
	    if (pdc == NULL) {
		cerr << "error: could not add connection : failed to allocate SocketConnection" << endl;
		return -1;
	    }
	    cp->push (pdc);
	} else
	    cerr << "error: could not add connection : no cp registered yet !" << endl;
	// connections[fd] = new DummyConnection (fd, client_addr);
	// cout << "now on, biggest_fd = " << set_biggest () << " / " << connections.size() << endl;
	// build_r_fd ();
	return 0;
    }

    ListeningSocket::~ListeningSocket (void) {}

//    SocketConnection * ListeningSocket::connection_binder (int fd, struct sockaddr_in const &client_addr) {
//	return new SocketConnection (fd, client_addr);
//    }
    
    void ListeningSocket::setname (const string & name) {
	ListeningSocket::name = name;
    }

    ListeningSocket::ListeningSocket (int fd) : Connection(fd, true) {}

    size_t ListeningSocket::read (void) {
	addconnect (fd);
	return 0;
    }

    ListeningSocket::ListeningSocket (int fd, const string & name) : Connection(fd, true) {
	setname (name);
    }

    string ListeningSocket::getname (void) {
	return name;
    }

    size_t ListeningSocket::write (void) { return 0; }

    /*
     *  ---------------------------- all-purpose string (and more) utilities ---------------------------------
     */

    bool getstring (istream & cin, string &s, size_t maxsize /* = 2048 */) {
	char c;
	size_t n = 0;
	while ((n < maxsize) && cin.get(c) && (c != 10) && (c != 13))
	    s += c, n++;
	return ((bool)cin);
    }

    size_t seekspace (const string &s, size_t p /* = 0 */) {
	if (p == string::npos)
	    return string::npos;

	size_t l = s.size();
	
	if (p >= l)
	    return string::npos;

	while ((p < l) && isspace (s[p])) p++;

	if (p >= l)
	    return string::npos;

	return p;
    }

    size_t getidentifier (const string &s, string &ident, size_t p /* = 0 */ ) {
	size_t l = s.size();
	ident = "";

	p = seekspace(s, p);
	
	if (p == string::npos)
	    return string::npos;
	
	if (p >= l)
	    return string::npos;

	if (! isalpha(s[p]))
	    return p;

	while (p < l) {
	    if (isalnum (s[p]) || (s[p]=='_') || (s[p]=='-')) {
		ident += s[p];
		p++;
	    } else
		break;
	}

	if (p >= l)
	    return string::npos;

	return p;
    }

    size_t getfqdn (const string &s, string &ident, size_t p /* = 0 */ ) {
	size_t l = s.size();
	ident = "";

	p = seekspace(s, p);
	
	if (p == string::npos)
	    return string::npos;
	
	if (p >= l)
	    return string::npos;

	if (! isalnum(s[p]))
	    return p;

	while (p < l) {
	    if (isalnum (s[p]) || (s[p]=='.') || (s[p]=='-')) {
		ident += s[p];
		p++;
	    } else
		break;
	}

	if (p >= l)
	    return string::npos;

	return p;
    }

    size_t getinteger (const string &s, long long &n, size_t p /* = 0 */ ) {
	size_t l = s.size();
	string buf;

	p = seekspace(s, p);
	
	if (p == string::npos)
	    return string::npos;
	
	if (p >= l)
	    return string::npos;

	if (! isdigit(s[p]))
	    return p;

	while (p < l) {
	    if (isdigit (s[p])) {
		buf += s[p];
		p++;
	    } else
		break;
	}

	n = atoll (buf.c_str());
	
	if (p >= l)
	    return string::npos;

	return p;
    }

    size_t getinteger (const string &s, long &n, size_t p /* = 0 */ ) {
	size_t l = s.size();
	string buf;

	p = seekspace(s, p);
	
	if (p == string::npos)
	    return string::npos;
	
	if (p >= l)
	    return string::npos;

	if (! isdigit(s[p]))
	    return p;

	while (p < l) {
	    if (isdigit (s[p])) {
		buf += s[p];
		p++;
	    } else
		break;
	}

	n = atol (buf.c_str());
	
	if (p >= l)
	    return string::npos;

	return p;
    }

    CharPP::CharPP (string const & s) {
	isgood = false;
	size_t size = s.size();
	size_t p;
	n = 0;
	char *buf = (char *) malloc (size+1);
	if (buf == NULL)
	    return;
	
	list <char *> lp;
	lp.push_back (buf);
	for (p=0 ; p<size ; p++) {
	    if (s[p] == 0)
		lp.push_back (buf + p + 1);
	    buf[p] = s[p];
	}
	n = lp.size() - 1;
	charpp = (char **) malloc ((n+1) * sizeof (char *));
	if (charpp == NULL) {
	    delete (buf);
	    n = 0;
	    return;
	}
	list <char *>::iterator li;
	int i;
	for (li=lp.begin(), i=0 ; (li!=lp.end()) && (i<n) ; li++, i++)
	    charpp[i] = *li;
	charpp[i] = NULL;
	isgood = true;
    }
    char ** CharPP::get_charpp (void) {
	if (isgood)
	    return charpp;
	else
	    return NULL;
    }
    size_t CharPP::size (void) {
	return n;
    }
    CharPP::~CharPP (void) {
	if (charpp != NULL) {
	    if (n>0)
		delete (charpp[0]);
	    delete (charpp);
	}
    }
    ostream& CharPP::dump (ostream& cout) const {
	int i = 0;
	while (charpp[i] != NULL)
	    cout << "    " << charpp[i++] << endl;
	return cout;
    }

    ostream & operator<< (ostream& cout, CharPP const & chpp) {
	return chpp.dump (cout);
    }
    

    ostream & operator<< (ostream& cout, ostreamMap const &m ) {
	if (m.m.empty()) {
	    cout << m.name << "[]={empty}" << endl;
	    return cout;
	}
	map<string,string>::const_iterator mi;
	for (mi=m.m.begin() ; mi!=m.m.end() ; mi++)
	    cout << m.name << "[" << mi->first << "]=\"" << hexdump(mi->second) << '"' << endl;
	
	return cout;
    }

    ostream & operator<< (ostream& cout, hexdump const &m ) {
	const char *hex = "0123456789ABCDEF";
	if (m.p == NULL) {
	    const string &s = m.s;
	    size_t i, size = s.size();
	    for (i=0 ; i< size ; ) {
		size_t j;
		cout << "| ";
		for (j=0 ; j<16 ; j++) {
		    if (i+j < size) {
			unsigned char c = s[i+j];
			cout << hex[(c & 0xf0) >> 4]
			     << hex[c & 0x0f]
			     << " ";
		    } else {
			cout << ".. ";
		    }
		    if (j == 7)
			cout << " ";
		}
		cout << "| ";
		for (j=0 ; j<16 ; j++) {
		    if (i+j < size) {
			unsigned char c = s[i+j];
			if (isgraph (c))
			    cout << c;
			else if (c == ' ')
			    cout << ' ';
			else
			    cout << ".";
		    } else {
			cout << " ";
		    }
		    if (j == 7)
			cout << " ";
		}
		cout << " |" << endl;
		i += 16;
	    }
	} else {
	    size_t i, size = m.n;
	    const char *s = m.p;
	    for (i=0 ; i< size ; ) {
		size_t j;
		cout << "| ";
		for (j=0 ; j<16 ; j++) {
		    if (i+j < size) {
			unsigned char c = s[i+j];
			cout << hex[(c & 0xf0) >> 4]
			     << hex[c & 0x0f]
			     << " ";
		    } else {
			cout << ".. ";
		    }
		    if (j == 7)
			cout << " ";
		}
		cout << "| ";
		for (j=0 ; j<16 ; j++) {
		    if (i+j < size) {
			unsigned char c = s[i+j];
			if (isgraph (c))
			    cout << c;
			else if (c == ' ')
			    cout << ' ';
			else
			    cout << ".";
		    } else {
			cout << " ";
		    }
		    if (j == 7)
			cout << " ";
		}
		cout << " |" << endl;
		i += 16;
	    }
	}
	return cout;
    }

} // namespace qiconn

