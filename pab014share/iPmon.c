/*
 * iPmon - IntelliFlow status monitor on 'pabus'
 */
#define	APRS485_API	1
#include "aprs485.h"
#include "pa_iflo.h"

char version[] = "@(#) iPmon 0.03";

struct {
	u08_t	 cadr;	/* this controllers address */
	u08_t	 padr;	/* pump address */
	int	 ctrl;
	int	 poll;
	iflsr_t *pstat;
	u08_t	 sbuf[PA5SIZ];
}	gl;

pa5_t *pa5_hdr(void *buf, u08_t cfi)
{
	pa5_t	*ph;

	bzero(ph=buf,sizeof(*ph));
	ph->lpb = 0xa5;
	ph->dst = gl.padr;
	ph->src = gl.cadr;
	ph->cfi = cfi;
	return ph;
}

int pa5_snd(int bd, pa5_t *ps)
{
	int	n;
	u16_t	sum;
	u08_t	*b, *s, pa5[PA5SIZ];

	for (b = pa5, n = 3; --n >= 0; *b++ = 0xff); b[-2] = 0;
	n = sizeof(*ps) + ps->len;
	for (sum = 0, s = &ps->lpb; --n >= 0; b++) sum += *b = *s++;
	*b++ = sum>>8;
	*b++ = sum;
	return write(bd,pa5,b-pa5);
}

pa5_t *pa5_rcv(int bd, void *buf, int tmo)
{
	pa5_t	*pr;
	u08_t	*b, *c, *e;
	fd_set	rfd;
	tmv_t	tv;
	u16_t	sum;
	int	n, k;

	FD_ZERO(&rfd); FD_SET(bd,&rfd);
	tv.tv_usec = tmo * 1000;
	tv.tv_sec  = tv.tv_usec/1000000;
	tv.tv_usec -= tv.tv_sec*1000000;
	if ((k = select(bd+1,&rfd,0,0,&tv)) <= 0) return 0;
	if ((n = read(bd,buf,PA5SIZ)) < sizeof(pa5_t)) return 0;
	for (e = (b = buf) + n; b < e && *b != 0xa5; b++);
	if ((e - b) < sizeof(pa5_t)) return 0;
	pr = (pa5_t *)b;
	if (((c = &pr->dat[pr->len]) + 2) < e || pr->dst != gl.cadr) return 0;
	for (sum = 0; b < c; sum += *b++);
	if (sum != ((c[0]<<8)+c[1])) return 0;
	return pr;
}

int pump_cmd(int bd, int cmd, int val)
{
	pa5_t	*ps, *pr;
	char	snd[sizeof(pa5_t)+4], pa5[PA5SIZ];

	ps = pa5_hdr(snd,cmd);
	ps->dat[ps->len++] = val;
	if (pa5_snd(bd,ps) <= 0) return -1;
	if ((pr = pa5_rcv(bd,pa5,500)) == 0) return -2;
	if (pr->src != gl.padr) return -3;
	if (pr->cfi != ps->cfi || pr->len != ps->len) return -4;
	return pr->dat[0] == ps->dat[0] ? pr->dat[0] : -5;
}

iflsr_t *pump_stat(int bd, void *buf)
{
	pa5_t	*ps, *pr;
	int	ctrl;
	char	snd[sizeof(pa5_t)+4];

	if ((ctrl = gl.ctrl) != IFLO_DSP_REM) {
		gl.ctrl = pump_cmd(bd,IFLO_DSP,IFLO_DSP_REM);
		if (gl.ctrl != IFLO_DSP_REM) return 0;
	}
	ps = pa5_hdr(snd,IFLO_SRG);
	pr = pa5_snd(bd,ps) <= 0 ? 0 : pa5_rcv(bd,buf,500);
	if (ctrl != IFLO_DSP_REM)
		gl.ctrl = pump_cmd(bd,IFLO_DSP,IFLO_DSP_LOC);
	if (pr == 0) return 0;
	if (pr->src != gl.padr || pr->dst != gl.cadr) return 0;
	if (pr->cfi != ps->cfi || pr->len != sizeof(iflsr_t)) return 0;
	return (iflsr_t *)&pr->dat;
}

void pr_pstat(iflsr_t *p)
{
	if (p == 0) printf("no status!\n");
	else {
		printf("%02d:%02d",p->clk[0],p->clk[1]);
		printf(" run=%02x",p->run);
		printf(" mod=%02x",p->mod);
		printf(" pmp=%02x",p->pmp);
		printf(" pwr=%d",ntohs(p->pwr));
		printf(" rpm=%d",ntohs(p->rpm));
		printf(" gpm=%d",p->gpm);
		printf(" ppc=%d",p->ppc);
		printf(" b09=%02x",p->b09);
		printf(" err=%02x",p->err);
		printf(" b11=%02x",p->b11);
		printf(" tmr=%d\n",p->tmr);
	}
}

int uifc(int bd)
{
	fd_set	rfd;
	tmv_t	tv;
	int	don, tic, rep, n;
	char	buf[64], pa5[PA5SIZ];

	rep = 15;
	for (tic = 1, don = 0; !don; ) {
		if (--tic < 0) {
			tic = (gl.pstat = pump_stat(bd,&gl.sbuf)) ? (rep-1) : 0;
			pr_pstat(gl.pstat);
		}
		printf("\r%s ",version+5);
		printf("%2d",tic+1);
		printf(gl.ctrl==IFLO_DSP_REM?"R> ":
		       gl.ctrl==IFLO_DSP_LOC?"L> ":
		                             "?> ");
		fflush(stdout);
		tv.tv_sec = 1; tv.tv_usec = 0;
		FD_ZERO(&rfd); FD_SET(0,&rfd); FD_SET(bd,&rfd);
		if ((n = select(bd+1,&rfd,0,0,&tv)) < 0 && errno != EINTR) break;
		if (n <= 0) continue;
		if (FD_ISSET(bd,&rfd)) {
			if ((n = read(bd,pa5,sizeof(pa5))) == 2 && pa5[0] == EOT)
				printf("\rEOT from hub                  \n");
			else	printf("\rWe are not alone on the bus...\n");
			break;
		}
		if (!FD_ISSET(0,&rfd)) continue;
		if (read(0,buf,sizeof(buf)) <= 0) continue;
		switch (buf[0]) {
		case 'q': don = 1; break;
		default:
			printf("q - quit\n");
			break;
		}
	}
	return errno;
}

int main(int ac, char **av)
{
	int	bd;
	char	msg[128];

	bzero(&gl,sizeof(gl));
	gl.cadr = 0x11;
	gl.padr = 0x60;
	bd = hub_at(ac>1?av[1]:0,msg);
	printf("%s: %s\n",av[0],msg);
	if (bd < 0) return -1;
	uifc(bd);
	hub_dt(bd);
	return 0;
}


