/*
 * iFlow - IntelliFlow simulator on <bus>
 */
#define	APRS485_API	1
#include "aprs485.h"
#include "pa_iflo.h"

char version[] = "@(#) iFlow 0.04";

struct {
	u08_t	adr;
	u08_t	dsp;
	int	eprg;
	int	chg;
	u08_t	sp_man_gpm;
	u08_t	sp_gpm;
	u08_t	sp_epgpm[5];
	int	sp_eprpm[5];
	iflsr_t	isr;
}	gl;

pa5_t *pa5_rcv(u08_t *buf, int nbu)
{
	pa5_t	*pr;
	u08_t	*b, *c, *e;
	u16_t	sum;
	int	n;

	if ((n = nbu) < sizeof(pa5_t)) return 0;
	for (e = (b = buf) + n; b < e && *b != 0xa5; b++);
	if ((e - b) < sizeof(pa5_t)) return 0;
	pr = (pa5_t *)b;
	if (((c = &pr->dat[pr->len]) + 2) < e || pr->dst != gl.adr) return 0;
	for (sum = 0; b < c; sum += *b++);
	if (sum != ((c[0]<<8)+c[1])) return 0;
	return pr;
}

void pa5_snd(int bd, pa5_t *ps)
{
	int	n, k;
	u16_t	sum;
	u08_t	*b, *s, snd[PA5SIZ];

	for (b = snd, n = 3; --n >= 0; *b++ = 0xff); b[-2] = 0;
	n = sizeof(*ps) + ps->len;
	for (sum = 0, s = &ps->lpb; --n >= 0; b++) sum += *b = *s++;
	*b++ = sum>>8;
	*b++ = sum>>0;
	n = b - snd;
	k = write(bd,snd,n);
	if (k != n) printf("write(%d)=%d!!!\n",n,k);
}

void cmd_in(int bd, pa5_t *pr)
{
	pa5_t	*ps;
	u16_t	adr, val;
	iflsr_t	isn;
	u08_t	buf[64];

	*(ps = (pa5_t *)buf) = *pr;
	ps->dst = pr->src;
	ps->src = gl.adr;
	switch (pr->cfi) {
	default: return; break;
	case IFLO_REG:
		if (pr->len != 4) return;
		adr = (pr->dat[0]<<8)+pr->dat[1];
		val = (pr->dat[2]<<8)+pr->dat[3];
		switch (adr) {
		default: return; break;
		case IFLO_REG_SPGPM: gl.sp_gpm = val; break;
		case IFLO_REG_EPRG:
			switch (val) {
			default: return; break;
			case IFLO_EPRG_P0:
			case IFLO_EPRG_P1:
			case IFLO_EPRG_P2:
			case IFLO_EPRG_P3:
			case IFLO_EPRG_P4:
				gl.eprg = val;
				break;
			}
			break;
		case IFLO_REG_EP1RPM: gl.sp_eprpm[1] = val; break;
		case IFLO_REG_EP2RPM: gl.sp_eprpm[2] = val; break;
		case IFLO_REG_EP3RPM: gl.sp_eprpm[3] = val; break;
		case IFLO_REG_EP4RPM: gl.sp_eprpm[4] = val; break;
		}
		ps->len = 2;
		ps->dat[0] = val>>8;
		ps->dat[1] = val>>0;
		break;
	case IFLO_DSP:
		if (pr->len != 1) return;
		switch (pr->dat[0]) {
		default: return; break;
		case IFLO_DSP_LOC:
		case IFLO_DSP_REM:
			if (gl.dsp != pr->dat[0]) gl.chg = 5;
			gl.dsp = ps->dat[0] = pr->dat[0];
			break;
		}
		break;
	case IFLO_MOD:
		if (pr->len != 1) return;
		if (gl.isr.mod != pr->dat[0]) gl.chg = 6;
		gl.isr.mod = ps->dat[0] = pr->dat[0];
		break;
	case IFLO_RUN:
		if (pr->len != 1) return;
		switch (pr->dat[0]) {
		default: return;
		case IFLO_RUN_STRT:
		case IFLO_RUN_STOP:
			if (gl.isr.run != pr->dat[0]) gl.chg = 7;
			gl.isr.run = ps->dat[0] = pr->dat[0];
			break;
		}
		break;
	case IFLO_SRG:
		if (gl.dsp != IFLO_DSP_REM) return;
		if (pr->len != 0) return;
		gl.isr.gpm = 0;
		if (gl.isr.run == IFLO_RUN_STRT) {
			switch (gl.isr.mod) {
			case IFLO_MOD_MANUAL:
				if (gl.isr.gpm != gl.sp_man_gpm) gl.chg = 8;
				gl.isr.gpm = gl.sp_man_gpm;
				break;
			case IFLO_MOD_FILTER:
			case IFLO_MOD_EXT_P1:
			case IFLO_MOD_EXT_P2:
			case IFLO_MOD_EXT_P3:
			case IFLO_MOD_EXT_P4:
				switch (gl.eprg) {
				case IFLO_EPRG_P0: gl.isr.gpm = gl.sp_epgpm[0]; gl.isr.mod = IFLO_MOD_FILTER; break;
				case IFLO_EPRG_P1: gl.isr.gpm = gl.sp_epgpm[1]; gl.isr.mod = IFLO_MOD_EXT_P1; break;
				case IFLO_EPRG_P2: gl.isr.gpm = gl.sp_epgpm[2]; gl.isr.mod = IFLO_MOD_EXT_P2; break;
				case IFLO_EPRG_P3: gl.isr.gpm = gl.sp_epgpm[3]; gl.isr.mod = IFLO_MOD_EXT_P3; break;
				case IFLO_EPRG_P4: gl.isr.gpm = gl.sp_epgpm[4]; gl.isr.mod = IFLO_MOD_EXT_P4; break;
				}
				break;
			case 6:
				if (gl.isr.gpm != gl.sp_gpm) gl.chg = 9;
				gl.isr.gpm = gl.sp_gpm;
				break;
			}
		}
		else {
			switch (gl.isr.mod) {
			case IFLO_MOD_EXT_P1:
			case IFLO_MOD_EXT_P2:
			case IFLO_MOD_EXT_P3:
			case IFLO_MOD_EXT_P4:
				gl.chg++;
				gl.isr.mod = IFLO_MOD_FILTER;
				break;
			}
		}
		if (gl.isr.gpm == 0) gl.isr.rpm = 0;
		else gl.isr.rpm = gl.isr.gpm * 50;
		gl.isr.pwr = (2000 * gl.isr.rpm) / 3450;
		isn = gl.isr;
		isn.pwr = htons(isn.pwr);
		isn.rpm = htons(isn.rpm);
		bcopy(&isn,ps->dat,ps->len=sizeof(isn));
		break;
	}
	pa5_snd(bd,ps);
}

void iflow(int bd)
{
	pa5_t	*pr;
	fd_set	pfds, rfds;
	tmv_t	tc, tv;
	int	pks, n;
	u08_t	pkb[PA5SIZ];
	struct tm tm;

	FD_ZERO(&pfds);
	FD_SET(0,&pfds);
	FD_SET(bd,&pfds);
	gettimeofday(&tc,0);
	localtime_r(&tc.tv_sec,&tm);
	while (1) {
		tv.tv_sec = 30; tv.tv_usec = 0;
		rfds = pfds;
		if (gl.chg) printf("\n");
		printf("\r%02d:%02d:%02d dsp=%02x run=%02x mod=%02x rpm=%-4d pwr=%-4d gpm=%-3d err=%02x ",
			tm.tm_hour,tm.tm_min,tm.tm_sec,
			gl.dsp,gl.isr.run,gl.isr.mod,gl.isr.rpm,gl.isr.pwr,gl.isr.gpm,gl.isr.err);
		fflush(stdout);
		gl.chg = 0;
		if ((n = select(bd+1,&rfds,0,0,&tv)) < 0 && errno != EINTR) break;
		if (n <= 0) continue;
		gettimeofday(&tc,0);
		localtime_r(&tc.tv_sec,&tm);
		gl.isr.clk[0] = tm.tm_hour;
		gl.isr.clk[1] = tm.tm_min;
		if (FD_ISSET(bd,&rfds)) {
			pks = read(bd,pkb,sizeof(pkb));
			if (pks == 2 && pkb[0] == EOT) {
				printf("EOT from hub\n");
				break;
			}
			if ((pr = pa5_rcv(pkb,pks))) cmd_in(bd,pr);
		}
		if (FD_ISSET(0,&rfds)) {
			n = read(0,pkb,sizeof(pkb));
			if (n < 1 || pkb[0] == 'q') break;
			if (pkb[0] == 'E') gl.isr.err ^= 0x80;
			if (pkb[0] == 'S' && gl.dsp == IFLO_DSP_LOC) {
				if (gl.isr.run != IFLO_RUN_STOP)
					gl.isr.run = IFLO_RUN_STOP;
				else	gl.isr.run = IFLO_RUN_STRT;
			}
		}
	}
}

int main(int ac, char **av)
{
	int	bd;
	char	msg[128];

	bzero(&gl,sizeof(gl));
	gl.adr = 0x60;
	gl.dsp = IFLO_DSP_LOC;
	gl.sp_man_gpm  = 30;
	gl.sp_epgpm[1] = 20;
	gl.sp_epgpm[2] = 25;
	gl.sp_epgpm[3] = 30;
	gl.sp_epgpm[4] = 36;
	gl.sp_eprpm[1] = 1000;
	gl.sp_eprpm[2] = 2000;
	gl.sp_eprpm[3] = 2500;
	gl.sp_eprpm[4] = 3450;
	gl.isr.run = IFLO_RUN_STOP;
	gl.isr.pmp = IFLO_PMP_READY;
	gl.isr.mod = IFLO_MOD_MANUAL;
	bd = hub_at(ac>1?av[1]:0,msg);
	printf("%s: %s\n",av[0],msg);
	if (bd < 0) return -1;
	iflow(bd);
	hub_dt(bd);
	return 0;
}

