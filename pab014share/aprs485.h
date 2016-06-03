#ifndef	__pabus_h__
#define	__pabus_h__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define	APRS485_PORT	10485

#define	EOT		0x04
#define	EOT_STR		"\004"
#define	ENQ		0x05
#define	ACK		0x06
#define	BEL		0x07
#define	NAK		0x15
#define	CAN		0x18
#define	ESC		0x1b
#define	DEL		0x7f

#include <netdb.h>
#include <netinet/in.h>
typedef union {
	struct sockaddr     sa;
	struct sockaddr_in  sin;
}	soa_t;
#define	sin_fmly	sin.sin_family
#define	sin_port	sin.sin_port
#define	sin_addr	sin.sin_addr.s_addr
#define	UDPSIZ	1460	/* UDP package pay load before it gets chopped */

#include <sys/time.h>
#include <time.h>
typedef struct timeval	tmv_t;

#define	NEL(x)		((int)(sizeof(x)/sizeof(x[0])))
#define	MAX(a,b)	((a)>=(b)?(a):(b))
#define	MIN(a,b)	((a)<=(b)?(a):(b))

typedef unsigned char	u08_t;
typedef unsigned short	u16_t;
typedef unsigned long	u32_t;

/* Pentair package header
 */
typedef struct {
	u08_t	lpb;
	u08_t	sub;
	u08_t	dst;
	u08_t	src;
	u08_t	cfi;
	u08_t	len;
	u08_t	dat[0];
}	pa5_t;
#define	PA5SIZ	(32+6+256+2)

/* if you are using this in a program working with aprs485, these may come in handy */
#if APRS485_API == 1

int hub_at(char *ap, char *err)
{
	char	*p, *r;
	fd_set	fds;
	soa_t	soa;
	tmv_t	tmv;
	int	hd, k;
	u08_t	buf[BUFSIZ];
	struct hostent	*h;

	if (ap == 0 || *ap == 0) ap = "localhost";
	if (strlen(ap) >= (int)sizeof(buf)) {
		if (err) sprintf(err,"name too long");
		return -1;
	}
	strcpy((char *)buf,ap);
	bzero(&soa,sizeof(soa));
	soa.sin_fmly = AF_INET;
	soa.sin_port = htons(APRS485_PORT);
	if ((p = strrchr((char *)buf,':'))) {
		*p++ = 0;
		k = strtol(p,&r,10);
		if (r <= p || *r || k <= 0 || k >= 0xffff) {
			if (err) sprintf(err,"bad port '%s'",p);
			return -1;
		}
		soa.sin_port = htons(k);
	}
	if (*(p = (char *)buf) == 0) p = "localhost";
	if ((h = gethostbyname(p)) == 0) {
		if (err) sprintf(err,"%s %s",p,hstrerror(h_errno));
		return -1;
	}
	bcopy(h->h_addr_list[0],&soa.sin_addr,h->h_length);

	if ((hd = socket(soa.sin_fmly,SOCK_DGRAM,0)) <= 0) {
		if (err) sprintf(err,"socket: %s",strerror(errno));
		return -1;
	}
	FD_ZERO(&fds); FD_SET(hd,&fds);
	tmv.tv_sec = 0; tmv.tv_usec = 250000;
	buf[0] = BEL;
	buf[1] = 0xff;
	if ((k = connect(hd,&soa.sa,sizeof(soa.sa))) ||
	    (k = write(hd,buf,2)) != 2 ||
	    (k = select(hd+1,&fds,0,0,&tmv)) <= 0 ||
	    (k = read(hd,buf,sizeof(buf))) != 2 ||
	    (buf[0] != ACK)) {
		if (err) sprintf(err,"HUB %s",k<0?strerror(errno):k==0?"timeout":"busy");
		close(hd);
		return -1;
	}
	if (err) sprintf(err,"hub tab[%d]",buf[1]);
	return hd;
}

void hub_dt(int hd)
{
	fd_set	fds;
	tmv_t	tmv;
	u08_t	buf[32];

	FD_ZERO(&fds); FD_SET(hd,&fds);
	tmv.tv_sec = 0; tmv.tv_usec = 250000;
	buf[0] = DEL;
	buf[1] = 0;
	write(hd,buf,2);
	select(hd+1,&fds,0,0,&tmv);
	close(hd);
}

#endif
#endif

