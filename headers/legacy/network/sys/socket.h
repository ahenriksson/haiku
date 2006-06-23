/* sys/socket.h */


#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#if __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef uint32_t socklen_t;

/* These are the address/protocol families we'll be using... */
/* NB these should be added to as required... */

/* If we want to have Binary compatability we may need to alter these
 * to agree with the Be versions...
 */
#define	AF_UNSPEC       0
#define AF_INET         1
#define AF_LOCAL        2
#define AF_UNIX         AF_LOCAL /* for compatability */
#define AF_ROUTE        3
#define AF_LINK         4
#define AF_INET6        5
#define AF_IPX          7
#define AF_IMPLINK      20

#define AF_MAX          24

#define PF_UNSPEC		AF_UNSPEC
#define PF_INET			AF_INET
#define PF_ROUTE		AF_ROUTE
#define PF_LINK			AF_LINK
#define PF_INET6		AF_INET6	
#define PF_IPX			AF_IPX
#define PF_IMPLINK      AF_IMPLINK

/* Types of socket we can create (eventually) */
#ifndef BUILDING_R5_LIBNET
#define	SOCK_STREAM		1
#define	SOCK_DGRAM		2
#define	SOCK_RAW		3
#define SOCK_MISC		255
#else /* BUILDING_R5_LIBNET */
/* XXX: HACK: we use socket emulation for libnet.so */
#define SOCK_DGRAM     10
#define SOCK_STREAM    11
#define SOCK_RAW       12
#define SOCK_MISC     255
#define	SOCK_NATIVE_STREAM	1
#define	SOCK_NATIVE_DGRAM	2
#define	SOCK_NATIVE_RAW		3
#define SOCK_NATIVE_MISC	255
#endif /* BUILDING_R5_LIBNET */

/*
 * Option flags per-socket.
 */
#define	SOL_SOCKET		0xffffffff		

#define SO_ACCEPTCONN	0x00000001	/* socket has had listen() */
#define SO_BROADCAST	0x00000002	/* permit sending of broadcast msgs */
#define	SO_DEBUG		0x00000004	/* turn on debugging info recording */
#define	SO_DONTROUTE	0x00000008	/* just use interface addresses */
#define	SO_KEEPALIVE	0x00000010	/* keep connections alive */
#define SO_OOBINLINE	0x00000020	/* leave received OOB data in line */
#define	SO_REUSEADDR	0x00000040	/* allow local address reuse */
#define SO_REUSEPORT	0x00000080	/* allow local address & port reuse */
#define SO_USELOOPBACK	0x00000100	/* bypass hardware when possible */
#define SO_LINGER		0x00000200	/* linger on close if data present */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF		0x40000001	/* send buffer size */
#define SO_SNDLOWAT		0x40000002	/* send low-water mark */
#define SO_SNDTIMEO		0x40000003	/* send timeout */
#define SO_RCVBUF		0x40000004	/* receive buffer size */
#define SO_RCVLOWAT		0x40000005	/* receive low-water mark */
#define SO_RCVTIMEO		0x40000006	/* receive timeout */
#define	SO_ERROR		0x40000007	/* get error status and clear */
#define	SO_TYPE			0x40000008	/* get socket type */

/* not handled by OpenBeOS */
#define SO_NONBLOCK		0x40000009
#define SO_BINDTODEVICE	0x4000000a

/* only defined in OpenBeOS */
#define SO_NETPROC		0x00001020	/* multiplex; network processing */

/*
 * These are the valid values for the "how" field used by shutdown(2).
 */
#define SHUT_RD			0
#define SHUT_WR			1
#define SHUT_RDWR		2
/* for BONE compatibility */
#define SHUTDOWN_RECV	SHUT_RD
#define SHUTDOWN_SEND	SHUT_WR
#define SHUTDOWN_BOTH	SHUT_RDWR


struct linger {
	int l_onoff;
	int l_linger;
};

struct sockaddr {
	uint8_t	sa_len;	
	uint8_t	sa_family;
	uint8_t	sa_data[30];
};

/* this can hold ANY sockaddr we care to throw at it! */
struct sockaddr_storage {
	uint8_t       ss_len;         /* total length */
	uint8_t       ss_family;      /* address family */
	uint8_t       __ss_pad1[6];   /* align to quad */
	uint64_t      __ss_pad2;      /* force alignment for stupid compilers */
	uint8_t       __ss_pad3[240]; /* pad to a total of 256 bytes */
};

struct sockproto {
	uint16_t sp_family;
	uint16_t sp_protocol;
};

#define CTL_NET         4

#define CTL_NET_NAMES { \
        { 0, 0 }, \
        { "unix", CTLTYPE_NODE }, \
        { "inet", CTLTYPE_NODE }, \
        { "implink", CTLTYPE_NODE }, \
        { "pup", CTLTYPE_NODE }, \
        { "chaos", CTLTYPE_NODE }, \
        { "xerox_ns", CTLTYPE_NODE }, \
        { "iso", CTLTYPE_NODE }, \
        { "emca", CTLTYPE_NODE }, \
        { "datakit", CTLTYPE_NODE }, \
        { "ccitt", CTLTYPE_NODE }, \
        { "ibm_sna", CTLTYPE_NODE }, \
        { "decnet", CTLTYPE_NODE }, \
        { "dec_dli", CTLTYPE_NODE }, \
        { "lat", CTLTYPE_NODE }, \
        { "hylink", CTLTYPE_NODE }, \
        { "appletalk", CTLTYPE_NODE }, \
        { "route", CTLTYPE_NODE }, \
        { "link_layer", CTLTYPE_NODE }, \
        { "xtp", CTLTYPE_NODE }, \
        { "coip", CTLTYPE_NODE }, \
        { "cnt", CTLTYPE_NODE }, \
        { "rtip", CTLTYPE_NODE }, \
        { "ipx", CTLTYPE_NODE }, \
        { "inet6", CTLTYPE_NODE }, \
        { "pip", CTLTYPE_NODE }, \
        { "isdn", CTLTYPE_NODE }, \
        { "natm", CTLTYPE_NODE }, \
        { "encap", CTLTYPE_NODE }, \
        { "sip", CTLTYPE_NODE }, \
        { "key", CTLTYPE_NODE }, \
}

/*
 * PF_ROUTE - Routing table
 *
 * Three additional levels are defined:
 *      Fourth: address family, 0 is wildcard
 *      Fifth: type of info, defined below
 *      Sixth: flag(s) to mask with for NET_RT_FLAGS
 */
#define NET_RT_DUMP     1               /* dump; may limit to a.f. */
#define NET_RT_FLAGS    2               /* by flags, e.g. RESOLVING */
#define NET_RT_IFLIST   3               /* survey interface list */
#define NET_RT_MAXID    4

#define CTL_NET_RT_NAMES { \
        { 0, 0 }, \
        { "dump", CTLTYPE_STRUCT }, \
        { "flags", CTLTYPE_STRUCT }, \
        { "iflist", CTLTYPE_STRUCT }, \
}

                        /* Max listen queue for a socket */
#define SOMAXCONN	5	/* defined as 128 in OpenBSD */

struct msghdr {
	char *	msg_name;         /* address we're using (optional) */
	uint msg_namelen;         /* length of address */
	struct iovec *msg_iov;    /* scatter/gather array we'll use */
	uint msg_iovlen;          /* # elements in msg_iov */
	char * msg_control;       /* extra data */
	uint msg_controllen;      /* length of extra data */
	int msg_flags;            /* flags */
};

/* Defines used in msghdr structure. */
#define MSG_OOB			0x1				/* process out-of-band data */
#define MSG_PEEK		0x2				/* peek at incoming message */
#define MSG_DONTROUTE	0x4				/* send without using routing tables */
#define MSG_EOR			0x8				/* data completes record */
#define MSG_TRUNC		0x10			/* data discarded before delivery */
#define MSG_CTRUNC		0x20			/* control data lost before delivery */
#define MSG_WAITALL		0x40			/* wait for full request or error */
#define MSG_DONTWAIT	0x80			/* this message should be nonblocking */
#define MSG_BCAST		0x100			/* this message rec'd as broadcast */
#define MSG_MCAST		0x200			/* this message rec'd as multicast */
/* not defind in OpenBeOS */
#define	MSG_EOF			0x400			/* data completes connection */


struct cmsghdr {
	uint	cmsg_len;
	int	cmsg_level;
	int	cmsg_type;
	/* there now follows uchar[] cmsg_data */
};


#define SIOCSHIWAT      _IOW('s',  0, int)              /* set high watermark */
#define SIOCGHIWAT      _IOR('s',  1, int)              /* get high watermark */
#define SIOCSLOWAT      _IOW('s',  2, int)              /* set low watermark */
#define SIOCGLOWAT      _IOR('s',  3, int)              /* get low watermark */
#define SIOCATMARK      _IOR('s',  7, int)              /* at oob mark? */

#define SIOCADDRT       _IOW('r', 10, struct ortentry)  /* add route */
#define SIOCDELRT       _IOW('r', 11, struct ortentry)  /* delete route */

#define SIOCSIFADDR     _IOW('i', 12, struct ifreq)     /* set ifnet address */
#define OSIOCGIFADDR    _IOWR('i', 13, struct ifreq)    /* get ifnet address */
#define SIOCGIFADDR     _IOWR('i', 33, struct ifreq)    /* get ifnet address */
#define SIOCSIFDSTADDR  _IOW('i', 14, struct ifreq)     /* set p-p address */
#define OSIOCGIFDSTADDR _IOWR('i', 15, struct ifreq)    /* get p-p address */
#define SIOCGIFDSTADDR  _IOWR('i', 34, struct ifreq)    /* get p-p address */
#define SIOCSIFFLAGS    _IOW('i', 16, struct ifreq)     /* set ifnet flags */
#define SIOCGIFFLAGS    _IOWR('i', 17, struct ifreq)    /* get ifnet flags */
#define OSIOCGIFBRDADDR _IOWR('i', 18, struct ifreq)    /* get broadcast addr */
#define SIOCGIFBRDADDR  _IOWR('i', 35, struct ifreq)    /* get broadcast addr */
#define SIOCSIFBRDADDR  _IOW('i', 19, struct ifreq)     /* set broadcast addr */
#define OSIOCGIFCONF    _IOWR('i', 20, struct ifconf)   /* get ifnet list */
#define SIOCGIFCONF     _IOWR('i', 36, struct ifconf)   /* get ifnet list */
#define OSIOCGIFNETMASK _IOWR('i', 21, struct ifreq)    /* get net addr mask */
#define SIOCGIFNETMASK  _IOWR('i', 37, struct ifreq)    /* get net addr mask */
#define SIOCSIFNETMASK  _IOW('i', 22, struct ifreq)     /* set net addr mask */
#define SIOCGIFMETRIC   _IOWR('i', 23, struct ifreq)    /* get IF metric */
#define SIOCSIFMETRIC   _IOW('i', 24, struct ifreq)     /* set IF metric */
#define SIOCDIFADDR     _IOW('i', 25, struct ifreq)     /* delete IF addr */
#define SIOCAIFADDR     _IOW('i', 26, struct ifaliasreq)/* add/chg IF alias */
#define SIOCGIFDATA     _IOWR('i', 27, struct ifreq)    /* get if_data */

#define SIOCGIFMTU      _IOWR('i', 126, struct ifreq)   /* get ifnet MTU */
#define SIOCSIFMTU      _IOW('i', 127, struct ifreq)    /* set ifnet MTU */

#define SIOCADDMULTI    _IOW('i', 49, struct ifreq)     /* add m'cast addr */
#define SIOCDELMULTI    _IOW('i', 50, struct ifreq)     /* del m'cast addr */


#ifndef _KERNEL_MODE
/* Function declarations */
int     socket (int, int, int);
int	socketpair(int domain, int type, int protocol, int socket_vector[2]);
int     bind(int, const struct sockaddr *, int);
int     connect(int, const struct sockaddr *, int);
int 	listen(int, int);
int 	accept(int, struct sockaddr *, int *);
int     closesocket(int);
int     shutdown(int sock, int how);

ssize_t send(int, const void *, size_t, int);
ssize_t recv(int, void *, size_t, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, int);
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, int *);

int     setsockopt(int, int, int, const void *, int);
int     getsockopt(int, int, int, void *, int *);
int     getpeername(int, struct sockaddr *, int *);
int     getsockname(int, struct sockaddr *, int *);
#endif /* _KERNEL_MODE */

#if __cplusplus
}
#endif /* __cplusplus */

#endif /* _SYS_SOCKET_H */

