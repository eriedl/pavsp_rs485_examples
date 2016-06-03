/*
 * iPump - experimental controller for IntelliFlow pumps
 */
#define	APRS485_API	1
#include "aprs485.h"
#include "pa_iflo.h"

char version[] = "@(#) iPump 0.03";

struct {
	u08_t	cadr;	/* this controllers address */
	u08_t	padr;	/* pump address */
	int	ctrl;
}	gl;

int str2i(char *str, char **rem)
{
	int	i, k;

	while (*str == ' ') str++;
	for (i = k = 0; *str >= '0' && *str <= '9'; str++)
		i = i * 10 + (*str - '0'), k++;
	if (rem) *rem = str;
	return k ? i : -1;
}

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
	if (pr->src != ps->dst || pr->dst != ps->src) return -3;
	if (pr->cfi != ps->cfi || pr->len != ps->len) return -4;
	return pr->dat[0] == ps->dat[0] ? pr->dat[0] : -5;
}

int pump_reg(int bd, int adr, int val)
{
	pa5_t	*ps, *pr;
	char	snd[sizeof(pa5_t)+4], pa5[PA5SIZ];

	ps = pa5_hdr(snd,0x01);
	ps->dat[ps->len++] = adr>>8;
	ps->dat[ps->len++] = adr>>0;
	ps->dat[ps->len++] = val>>8;
	ps->dat[ps->len++] = val>>0;
	if (pa5_snd(bd,ps) <= 0) return -1;
	if ((pr = pa5_rcv(bd,pa5,500)) == 0) return -2;
	if (pr->src != ps->dst || pr->dst != ps->src) return -3;
	if (pr->cfi != ps->cfi || pr->len != 2) return -4;
	adr = (pr->dat[0]<<8)+pr->dat[1];
	return adr == val ? val : -5;
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
		printf("run=%02x",p->run);
		printf(" mod=%02x",p->mod);
		printf(" pmp=%02x",p->pmp);
		printf(" pwr=%d",ntohs(p->pwr));
		printf(" rpm=%d",ntohs(p->rpm));
		printf(" gpm=%d",p->gpm);
		printf(" ppc=%d",p->ppc);
		printf(" err=%02x",p->err);
		printf(" tmr=%d",p->tmr);
		printf(" %02d:%02d\n",p->clk[0],p->clk[1]);
	}
}

typedef struct {
	int	rep;
	int	run;
	int	rr;
	int	gpm;
}	ltc_t;

void ltic6(int bd, ltc_t *l)
{
	iflsr_t	*p;
	int	k;
	char	pa5[PA5SIZ];

	if (l->run == 0 && l->rr == 0) return;
	while ((k = 1)) {
		if (l->rr != 6) break;
		if ((gl.ctrl = pump_cmd(bd,IFLO_DSP,IFLO_DSP_REM)) != IFLO_DSP_REM) break;
		if (pump_reg(bd,IFLO_REG_SPGPM,l->gpm) != l->gpm) break;
		if (pump_cmd(bd,IFLO_MOD,IFLO_MOD_FEATR1) != IFLO_MOD_FEATR1) break;
		if (pump_cmd(bd,IFLO_RUN,IFLO_RUN_STRT) != IFLO_RUN_STRT) break;
		if ((p = pump_stat(bd,pa5)) == 0) break;
		if (p->err) break;
		if (p->run != IFLO_RUN_STRT) break;
		l->run = l->rr;
		k = 0;
		break;
	}
	if (k) { /* shut it down */
		pump_cmd(bd,IFLO_RUN,IFLO_RUN_STOP);
		pump_cmd(bd,IFLO_MOD,IFLO_MOD_MANUAL);
		gl.ctrl = pump_cmd(bd,IFLO_DSP,IFLO_DSP_LOC);
		l->run = l->rr = 0;
	}
}

int uifc(int bd)
{
	fd_set	rfd;
	tmv_t	tv;
	ltc_t	*l, ltc;
	int	don, tic, n, k;
	char	buf[16], pa5[PA5SIZ];

	bzero(l=&ltc,sizeof(*l));
	l->rep = 16;
	l->gpm = 20;
	for (tic = don = 0; !don; ) {
		if (--tic < 0) {
			switch (gl.ctrl) {
			case IFLO_DSP_REM:
			case IFLO_DSP_LOC:
				ltic6(bd,l);
				tic = l->run >= 0 ? (l->rep-1) : 0;
				break;
			default:
				gl.ctrl = pump_cmd(bd,IFLO_DSP,IFLO_DSP_LOC);
				tic = 0;
				break;
			}
		}
		printf("\r%s ",version+5);
		printf(l->run==0?"-- ":"%2d ",tic+1);
		printf(gl.ctrl==IFLO_DSP_REM?"R> ":
		       gl.ctrl==IFLO_DSP_LOC?"L> ":
		                             "?> ");
		fflush(stdout);
		tv.tv_sec = 1; tv.tv_usec = 0;
		FD_ZERO(&rfd); FD_SET(0,&rfd); FD_SET(bd,&rfd);
		if ((n = select(bd+1,&rfd,0,0,&tv)) < 0 && errno != EINTR)
			break;
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
		case 's': pr_pstat(pump_stat(bd,pa5)); break;
		case 'S': l->rr = 0, tic = 0; break;
		case 'R': l->rr = 6, tic = 0; break;
		case 'f':
			/* this puposefully suspends tick! */
			printf("gpm [%d]: ",l->gpm); fflush(stdout);
			if ((n = read(0,buf,sizeof(buf))) <= 0) break;
			if ((k = str2i(buf,0)) >= 15 && k <= 130) l->gpm = k;
			break;
		default:
			printf("q - quit\n");
			printf("s - pump status\n");
			printf("S - stop\n");
			printf("R - run\n");
			printf("f - set flow rate\n");
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
	gl.cadr = 0x11; /* standard is 0x10 */
	gl.padr = 0x60;
	gl.ctrl = -1;
	bd = hub_at(ac>1?av[1]:0,msg);
	printf("%s: %s\n",av[0],msg);
	if (bd < 0) return -1;
	uifc(bd);
	hub_dt(bd);
	return 0;
}

