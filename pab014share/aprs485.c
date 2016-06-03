/*
 * aprs485 - RS485 access point/bridge/hub
 */
#include "aprs485.h"
#include <termios.h>
char version[] = "@(#) aprs485 0.09 20100514";

typedef struct {	/* log entry */
	tmv_t	tim;
	char	buf[256-8];
}	sle_t;
void slog(char *fmt, ...);
void slog_dump();

typedef struct {
	tmv_t	tim;	/* pwr on time */
	int	pwr;
	soa_t	adr;
	int	pks;
	u08_t	pkb[UDPSIZ];
}	tab_t;

struct {
	int	 exit;
	tmv_t	 tboot;
	char	 bus[128]; /* bus adapter (tty or TCP socket) */
	int	 esel;	/* consecutive error count select() */
	int	 esin;	/* consecutive error count soc_in() */

	int	 dbug;
	char	*ldir;	/* log directory */
	int	 lprd;	/* log partial reads on bus */

	int	 nrcv;	/* bus receive buffer */
	u08_t	 brcv[UDPSIZ];

	tab_t	 tabs[8]; /* tab clients */
	tab_t	*tsnd;	/* tab waiting to send */

	int	 nsle;  /* log mechanism */
	sle_t	 sles[16];
}	gl;

/*
 * log mechanism
 */
static int tm2yymmdd(struct tm *tm)
{
	return (tm->tm_year%100)*10000+(tm->tm_mon+1)*100+tm->tm_mday;
}

void slog(char *fmt, ...)
{
	sle_t	*se;
	int	*a, i;

	se = &gl.sles[gl.nsle++];
	if (gl.nsle >= NEL(gl.sles)) return;
	gettimeofday(&se->tim,0);
	a = (int *)&fmt; a++;
	i = snprintf(se->buf,NEL(se->buf)-1,fmt,a[0],a[1],a[2],a[3],a[4],a[5]);
	se->buf[i] = 0;
}

void slog_dump()
{
	sle_t	*se;
	int	ymd, lf, fd, k;
	char	*p, buf[BUFSIZ], lfn[BUFSIZ];
	struct tm tm;

	fd = lf = -1;
	for (se = gl.sles, k = MIN(NEL(gl.sles),gl.nsle); --k >= 0; se++) {
		p = buf;
		localtime_r(&se->tim.tv_sec,&tm);
		ymd = tm2yymmdd(&tm);
		p += sprintf(p,"%06d",ymd);
		p += sprintf(p," %02d:%02d:%02d.%03lu",tm.tm_hour,tm.tm_min,tm.tm_sec,se->tim.tv_usec/1000);
		p += sprintf(p," %.*s",NEL(se->buf),se->buf);
		if (p[-1] != '\n' && p[-1] != '\r') *p++ = '\n';
		if (gl.ldir) {
			if (fd < 0 || lf != ymd) {
				if (fd >= 0) close(fd);
				lf = ymd;
				sprintf(lfn,"%s/%06d.log",gl.ldir,lf);
				fd = open(lfn,O_WRONLY|O_CREAT|O_APPEND,0644);
			}
			if (fd >= 0) write(fd,buf,p-buf);
		}
		if (gl.dbug) *p++ = '\r', write(1,buf,p-buf);
	}
	if (fd >= 0) close(fd);
	gl.nsle = 0;
}

/*
 * tab management
 */
tab_t *soa2tab(soa_t *sa)
{
	tab_t	*t;
	int	n;

	for (t = gl.tabs, n = NEL(gl.tabs); --n >= 0; t++)
		if (t->pwr != 1) continue;
		else if (t->adr.sin_addr == sa->sin_addr &&
		         t->adr.sin_port == sa->sin_port) return t;
	return 0;
}

/*
 * tab[] client/server communication
 * messages are 2 bytes:
 *
 * client	server
 * BEL <x>	ACK <tab#>	client request to attach to hub tab
 *		NAK <#tabs>	error, no tab available
 *
 * DEL <x>	ACK <x>		client detach request
 *		NAK <x>		error, not on a tab
 *
 *		EOT 0		server exit
 */
int tab_man(soa_t *sa, u08_t *pkb, int pks)
{
	tab_t	*t;
	int	n;

	if (pks != 2) return 0;
	switch (pkb[0]) {
	case BEL: /* request tab */
		if ((t = soa2tab(sa)) == 0) {
			for (t = gl.tabs, n = NEL(gl.tabs); --n >= 0; t++)
				if (t->pwr == 0) {
					t->pwr = 1;
					t->adr = *sa;
					t->pks = 0;
					gettimeofday(&t->tim,0);
					break;
				}
			if (n < 0) t = 0;
			else slog("T __ add [%d]",t-gl.tabs);
		}
		pkb[0] = t ? ACK : NAK;
		pkb[1] = t ? (t-gl.tabs) : NEL(gl.tabs);
		return 2;
		break;
	case DEL: /* drop tab */
		if ((t = soa2tab(sa))) {
			if (t == gl.tsnd) gl.tsnd = 0;
			t->pwr = 0;
			pkb[0] = ACK;
			slog("T __ del [%d]",t-gl.tabs);
		}
		else pkb[0] = NAK;
		return 2;
		break;
	}
	return 0;
}

void snd2tabs(int sd, tab_t *ts, u08_t *pkb, int pks)
{
	tab_t	*t;
	int	n, k;

	if (pks <= 0) return;
	for (t = gl.tabs, n = NEL(gl.tabs); --n >= 0; t++)
		if (t->pwr == 1 && t != ts) {
			k = sendto(sd,pkb,pks,0,&t->adr.sa,sizeof(t->adr.sa));
			if (k != pks) {
				t->pwr = 0;
				slog("T __ drop [%d] %s",t-gl.tabs,k<0?strerror(errno):"");
			}
		}
}

char *hex(u08_t *b, int nb, char *hb, int nh)
{
	char	*h = hb;
	int	i;

	for (i = MIN((nh/2-1),nb); --i >= 0; b++) {
		*h++ = "0123456789abcdef"[(*b>>4)&0x0f];
		*h++ = "0123456789abcdef"[(*b>>0)&0x0f];
	}
	*h = 0;
	return hb;
}

/*
 * debug I/O
 */
void usr_in(fd)
{
	int	k;
	char	buf[32];

	if ((k = read(fd,buf,sizeof(buf))) != 1) {
		slog("E __ term read()=%d %s",k,k<0?strerror(errno):"");
		gl.exit = 1;
		return;
	}
	switch (buf[0]) {
	default:  slog("D __ hit <Escape> to exit"); break;
	case ESC: gl.exit  = 1; break;
	case 'D': gl.lprd ^= 1; slog("D __ log partial read %d now",gl.lprd); break;
	}
}

/*
 * bus I/O
 */
void bus_in(int bd)
{
	int	k;
	char	hb[128];

	if ((k = NEL(gl.brcv) - gl.nrcv) <= 0) {
		slog("E __ bus buffer overrun drop %d bytes",gl.nrcv);
		gl.nrcv = 0; /* drop everything */
		k = NEL(gl.brcv);
	}
	if ((k = read(bd,&gl.brcv[gl.nrcv],k)) <= 0) {
		slog("E __ bus read()=%d %s",k,k<0?strerror(errno):"disconnect!");
		gl.exit = 1;
		return;
	}
	if (gl.lprd) slog("r %2d %s",k,hex(&gl.brcv[gl.nrcv],k,hb,sizeof(hb)));
	gl.nrcv += k;
}

void bus_out(int sd, int bd)
{
	tab_t	*t;
	int	k;
	char	hb[UDPSIZ*2+4];

	if ((t = gl.tsnd) == 0) return;
	slog("S %2d %s",t->pks,hex(t->pkb,t->pks,hb,sizeof(hb)));
	if ((k = write(bd,t->pkb,t->pks)) != t->pks) {
		slog("E __ bus write(%d)=%d %s",t->pks,k,k<0?strerror(errno):"");
		if (k < 0) gl.exit = 1;
	}
	else snd2tabs(sd,t,t->pkb,t->pks);
	t->pks = 0;
	gl.tsnd = 0;
}

/*
 * client interface
 */
int str2av(char *line, char **av, int nav)
{
	char	*b, *s, *d, del;
	int	ac;

	for (b = line; *b && *b != '\n' && *b != '\r'; b++);
	*b = 0;
	for (b = line, ac = 0; ac < nav; ) {
		while (*b == ' ' || *b == '\t') b++;
		if (*b == 0 || *b == '#') break;
		if (*b == '\"') {
			del = *b++;
			av[ac] = b;
			while (*b != 0 && (*b != del || b[-1] == '\\')) b++;
			if (*b == del) *b++ = 0; else av[ac] -= 1;
			for (s = d = av[ac]; *s && s < b; s++)
				if (s > av[ac] && *s == del && d[-1] == '\\') d[-1] = del;
				else if (s == d) d++;
				else *d++ = *s;
			*d = 0;
			ac++;
		}
		else {
			av[ac++] = b;
			while (*b != 0 && *b != ' ' && *b != '\t') b++;
		}
		if (*b == 0) break;
		*b++ = 0;
	}
	return ac;
}

void srv_client(int sd, soa_t *fa, int na, char **av)
{
	tab_t	*t;
	int	n;
	u08_t	*u;
	char	*r, *a, ans[UDPSIZ];
	struct tm tm;

	a = ans;
	if (na <= 0 || !strcmp(av[0],"stat")) {
		a += sprintf(a,"%s bus:%s",version+5,gl.bus);
		localtime_r(&gl.tboot.tv_sec,&tm);
		a += sprintf(a,"\nboot:  %04d/%02d/%02d %02d:%02d:%02d",
			tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
		for (t = gl.tabs, n = NEL(gl.tabs); --n >= 0; t++) {
			if (t->pwr == 0) continue;
			a += sprintf(a,"\ntab[%d]",t-gl.tabs);
			localtime_r(&t->tim.tv_sec,&tm);
			a += sprintf(a," %04d/%02d/%02d %02d:%02d:%02d",
				tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
			u = (u08_t *)&t->adr.sin_addr;
			a += sprintf(a," %d.%d.%d.%d:%d",u[0],u[1],u[2],u[3],ntohs(t->adr.sin_port));
		}
	}
	else if (!strcmp(av[0],"kill")) {
		if (na != 2 || (n = strtol(av[1],&r,0)) < 0 || n >= NEL(gl.tabs) || r == av[1] || *r)
			a += sprintf(a,"which one?");
		else {
			t = &gl.tabs[n];
			if (t->pwr == 0) a += sprintf(a,"tab[%d] is not active",n);
			else {
				sendto(sd,EOT_STR,2,0,&t->adr.sa,sizeof(t->adr.sa));
				t->pwr = 0;
				slog("T __ kill [%d]",t-gl.tabs);
				a += sprintf(a,"done");
			}
		}
	}
	else if (!strcmp(av[0],"exit")) {
		a += sprintf(a,"bye!");
		gl.exit = 1;
	}
	else if (!strcmp(av[0],"l") || !strcmp(av[0],"logdir")) {
		a += sprintf(a,"%s",gl.ldir?gl.ldir:"none");
	}
	else a += sprintf(a,"what?");
	if (a > ans) sendto(sd,ans,a-ans,0,&fa->sa,sizeof(fa->sa));
}

/*
 * socket I/O
 */
void soc_in(int sd)
{
	tab_t	*t;
	soa_t	fa;
	int	pks, k;
	u08_t	pkb[UDPSIZ+4];
	char	*av[32];
	socklen_t len;

	len = sizeof(fa.sa);
	pks = recvfrom(sd,pkb,sizeof(pkb)-4,0,&fa.sa,&len);
	if (pks <= 0) {
		if (++gl.esin >= 3) gl.exit = 1;
		slog("E __ socket read()=%d %s",pks,pks<0?strerror(errno):"");
		return;
	}
	gl.esin = 0;
	if (pks <= 2 && (k = tab_man(&fa,pkb,pks)) > 0) {
		sendto(sd,pkb,k,0,&fa.sa,len);
		return;
	}
	if (pkb[0] == 'c' && pks >= 6 && !strncmp((char *)pkb,"client",6)) {
		pkb[pks] = 0;
		if ((k = str2av((char *)pkb+6,av,NEL(av))) > 0)
			srv_client(sd,&fa,k,av);
		return;
	}
	if (gl.tsnd) return; /* busy, drop it */
	if ((t = soa2tab(&fa)) == 0) return; /* not registered */
	bcopy(pkb,t->pkb,t->pks=pks);
	gl.tsnd = t;
}

int a5end(u08_t *pk, int np)
{
	pa5_t	*p;
	u08_t	*b, *e;

	for (e = (b = pk) + np; b < e && *b != 0xa5; b++);
	if (b >= e) return np;
	p = (pa5_t *)b;
	b = &p->dat[p->len+2];
	if (b >= e || *b != 0xff) return np; 
	return b - pk;
}

void soc_out(int sd)
{
	int	n, i, k;
	char	hb[UDPSIZ*2+4];

	if ((n = gl.nrcv) > 2) {
		for (i = 0; i < n; i += k) {
			k = a5end(gl.brcv+i,n-i);
			slog("R %2d %s",k,hex(gl.brcv+i,k,hb,sizeof(hb)));
			snd2tabs(sd,0,gl.brcv+i,k);
		}
	}
	else slog("E %2d %s noise?",n,hex(gl.brcv,n,hb,sizeof(hb)));
	gl.nrcv = 0;
}

/*
 * bridge server
 */
#define	MTICK	(15*60)
#define	TBRTO	20000	/* bus read timeout (end of package) [microseconds] */

int bridge(int bd, int sd)
{
	fd_set	pfds, rfds;
	tmv_t	tc, tv;
	int	md, ct, lt, n;

	FD_ZERO(&pfds);
	if (gl.dbug) FD_SET(0,&pfds);
	FD_SET(bd,&pfds);
	FD_SET(sd,&pfds);
	md = MAX(bd,sd) + 1;
	ct = lt = -1;
	slog("B __ %s bus:%s",version+5,gl.bus);
	while (!gl.exit) {
		gettimeofday(&tc,0);
		if ((ct = (tc.tv_sec%(60*60))/MTICK) != lt) { /* mark tick */
			if (lt >= 0) slog("M __ %s bus:%s",version+5,gl.bus);
			lt = ct;
		}
		slog_dump();
		tv.tv_sec = tv.tv_usec = 0;
		if (gl.nrcv <= 0 && gl.tsnd == 0) {
			tv.tv_sec = ((tc.tv_sec/MTICK)+1)*MTICK;
			timersub(&tv,&tc,&tv);
		}
		if (tv.tv_sec == 0 && tv.tv_usec < TBRTO)
			tv.tv_usec = TBRTO;
		rfds = pfds;
		if ((n = select(md,&rfds,0,0,&tv)) < 0) {
			if (++gl.esel >= 3) gl.exit = 1;
			slog("E __ select() %s",strerror(errno));
			continue;
		}
		gl.esel = 0;
		if (n == 0) {
			if (gl.nrcv > 0) soc_out(sd);
			else if (gl.tsnd) bus_out(sd,bd);
		}
		else {
			if (gl.dbug && FD_ISSET(0,&rfds)) usr_in(0);
			if (FD_ISSET(bd,&rfds)) bus_in(bd);
			if (FD_ISSET(sd,&rfds)) soc_in(sd);
		}
	}
	snd2tabs(sd,0,(u08_t *)EOT_STR,2);
	slog("X __ %s bus:%s",version+5,gl.bus);
	slog_dump();
	return 0;
}

int open_aptty(char *dev)
{
	int	bd, eno;
	struct termios tio;

	if ((bd = open(dev,O_RDWR|O_NOCTTY|O_NDELAY)) < 0)
		return -1;
	while (1) {
		tcflush(bd,TCIOFLUSH);
		if (tcgetattr(bd,&tio) < 0) break;
		tio.c_iflag = IGNPAR;
		tio.c_oflag = 0;
		tio.c_cflag = CREAD|CLOCAL|B9600|CS8;
		tio.c_lflag = 0;
		tio.c_line  = 0;
		tio.c_cc[VMIN]  = 0;
		tio.c_cc[VTIME] = 0;
		if (tcsetattr(bd,TCSANOW,&tio) < 0) break;
		return bd;
	}
	eno = errno;
	close(bd);
	errno = eno;
	return -1;
}

int open_apsoc(char *ip, char *err)
{
	soa_t	sa;
	int	bd, i;
	char	*p, *r, buf[BUFSIZ];
	struct hostent	*h;

	if (ip == 0 || strlen(ip) >= NEL(buf)) {
		if (err) sprintf(err,"name too long");
		return -1;
	}
	strcpy(buf,ip);
	bzero(&sa,sizeof(sa));
	sa.sin_fmly = AF_INET;
	if ((p = strrchr(buf,':')) == 0) {
		if (err) sprintf(err,"port not specified");
		return -1;
	}
	*p++ = 0;
	i = strtol(p,&r,10);
	if (r <= p || *r || i <= 0 || i >= 0xffff) {
		if (err) sprintf(err,"bad port '%s'",p);
		return -1;
	}
	sa.sin_port = htons(i);
	if (*(p = buf) == 0) p = "localhost";
	if ((h = gethostbyname(p)) == 0) {
		if (err) sprintf(err,"%s",hstrerror(h_errno));
		return -1;
	}
	bcopy(h->h_addr_list[0],&sa.sin_addr,h->h_length);
	if ((bd = socket(sa.sin_fmly,SOCK_STREAM,0)) < 0) {
		if (err) sprintf(err,"%s",strerror(errno));
		return -1;
	}
	if (connect(bd,&sa.sa,sizeof(sa.sa))) {
		if (err) sprintf(err,"%s",strerror(errno));
		close(bd);
		return -1;
	}
	fcntl(bd,F_SETFL,O_NONBLOCK);
	return bd;
}

#include <signal.h>

void sighandler(int sig)
{
	switch (sig) {
	case SIGHUP:
	case SIGTERM: gl.exit++; break;
	}
}

void sigsetup()
{
	struct sigaction action;

	sigaction(SIGHUP,NULL,&action);
	action.sa_handler = sighandler;
	action.sa_flags   = 0;
	sigaction(SIGHUP, &action,0);
	sigaction(SIGTERM,&action,0);
	sigaction(SIGALRM,&action,0);
}

int av2str(int na, char **av, char *str, int sob)
{
	char	*s, *d, *e;
	int	i;

	if ((d = str) && sob > 0) {
		for (e = d + sob - 1, i = 0; i < na && d < e; i++) {
			if (i) *d++ = ' ';
			for (s = av[i]; d < e && (*d = *s++); d++);
		}
		*d = 0;
	}
	return d - str;
}

int client(int sd, soa_t *sa, int ac, char **av)
{
	fd_set	rds;
	tmv_t	tv;
	int	n;
	char	buf[UDPSIZ];

	n = sprintf(buf,"client ");
	if (ac <= 0) n += sprintf(buf+n,"stat");
	else n += av2str(ac,av,buf+n,sizeof(buf)-1-n);
	if (sendto(sd,buf,n,0,&sa->sa,sizeof(sa->sa)) != n) return errno;
	FD_ZERO(&rds);
	FD_SET(sd,&rds);
	tv.tv_sec = 1; tv.tv_usec = 0;
	if ((n = select(sd+1,&rds,0,0,&tv)) < 0) return errno;
	if (n == 0) return ETIME;
	if ((n = read(sd,buf,sizeof(buf))) < 0) return errno;
	printf("%.*s\n",n,buf);
	return 0;
}

int main(int ac, char **av)
{
	char	*p;
	soa_t	sa;
	int	bd, sd, k, eno;
	struct termios	tio[2];

	bzero(&gl,sizeof(gl));
	gettimeofday(&gl.tboot,0);
	bzero(&sa,sizeof(sa));
	sa.sin_fmly = AF_INET;
	sa.sin_port = htons(APRS485_PORT);
	for (k = 0; ++k < ac; )
		if (*(p = av[k]) == '-')
			switch (*(++p)) {
			default:
USAGE:				printf("usage: [-<options>] <bus>\n");
				printf("-p <port>   - server port (default: %d)\n",APRS485_PORT);
				printf("-l <logdir> - log directory\n");
				printf("-d          - debug, don't fork\n");
				printf("-D          - log partial reads from <bus>\n");
				printf("<bus>       - <ttydev> or <ipaddr:port>\n");
				printf("-c <cmd ..> - send <cmd> to running server\n");
				return EINVAL;
				break;
			case 'd': gl.dbug++; break;
			case 'D': gl.lprd++; break;
			case 'c':
				if ((sd = socket(sa.sin_fmly,SOCK_DGRAM,0)) < 0) {
					printf("socket: %s\n",strerror(eno=errno));
					return eno;
				}
				k++;
				eno = client(sd,&sa,ac-k,av+k);
				close(sd);
				if (eno) printf("client: %s\n",strerror(eno));
				return 0;
				break;
			case 'p':
				if (++k >= ac) goto USAGE;
				eno = strtol(av[k],0,0);
				if (eno <= 1024 || eno > 0xffff) goto USAGE;
				sa.sin_port = htons(eno);
				break;
			case 'l':
				if (++k >= ac) goto USAGE;
				gl.ldir = av[k];
				break;
			}
		else if (gl.bus[0]) goto USAGE;
		else if (*p == 'S' || (*p >= '0' && *p <= '9'))
			sprintf(gl.bus,"/dev/tty%s",p);
		else	sprintf(gl.bus,"%.*s",sizeof(gl.bus)-1,p);
	if (gl.bus[0] == 0) goto USAGE;

	if (tcgetattr(0,&tio[0]) < 0) {
		printf("tcgetattr: %s\n",strerror(eno=errno));
		return eno;
	}

	if ((sd = socket(sa.sin_fmly,SOCK_DGRAM,0)) <= 0) {
		printf("socket: %s\n",strerror(eno=errno));
		return eno;
	}
	if (fcntl(sd,F_SETFL,O_NONBLOCK)) {
		printf("fcntl(O_NONBLOCK): %s\n",strerror(eno=errno));
		return eno;
	}
	if (bind(sd,&sa.sa,sizeof(sa.sa))) {
		printf("bind to port %d: %s\n",ntohs(sa.sin_port),strerror(eno=errno));
		return eno;
	}
	k = 1; setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&k,sizeof(k));

	if (strchr(gl.bus,':')) {
		char	msg[128];
		if ((bd = open_apsoc(gl.bus,msg)) <= 0) {
			printf("%s: %s\n",gl.bus,msg);
			close(sd);
			return EINVAL;
		}
	}
	else if ((bd = open_aptty(gl.bus)) <= 0) {
		printf("%s: %s\n",gl.bus,strerror(eno=errno));
		close(sd);
		return eno;
	}

	if (!gl.dbug) {
		if ((k = fork()) < 0) {
			printf("fork: %s\n",strerror(eno=errno));
			close(bd);
			close(sd);
			return eno;
		}
		if (k > 0) return 0; /* parent, go away */
		/* child continues */
		setsid();
		close(0);
		close(1);
		close(2);
	}
	else {
		tio[1] = tio[0];
		cfmakeraw(&tio[1]);
		tcsetattr(0,TCSANOW,&tio[1]);
		printf("hit <Escape> to exit...\r\n");
	}
	sigsetup();
	eno = bridge(bd,sd);
	if (gl.dbug) tcsetattr(0,TCSANOW,tio);
	close(bd);
	close(sd);
	return eno;
}

