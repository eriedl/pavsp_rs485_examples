/*
 * padec - interpreter for 'palog' datafile
 */
#include "aprs485.h"
#include "pa_iflo.h"
#include "pa_ctrl.h"
#include <stdlib.h>

char version[] = "@(#) padec 0.03";

int pa5pump(u08_t adr) { return (adr & 0xfc) == 0x60; }
int pa5bcst(u08_t adr) { return adr == 0x0f; }
int pa5ctrl(u08_t adr) { return adr == 0x10; }

#define	CHG(l,p,m)	(!l || p->m != l->m)

void pa5deco(FILE *fo, int ind, pa5_t *pm, pa5_t *pl)
{
	char	*l;
	u08_t	cfi;
	u16_t	adr, val;

	cfi = pm->cfi;
	if (cfi == 0xff && pm->len == 1) {
		fprintf(fo," ERROR(%d)",pm->dat[0]);
		return;
	}
	if (pa5ctrl(pm->dst) && (cfi & 0xc0) == 0xc0 && pm->len == 1) {
		fprintf(fo," SEND c=%02x,%02x",cfi&0x3f,pm->dat[0]);
		return;
	}
	if (pa5ctrl(pm->dst) && (cfi & 0xc0) == 0x80 && pm->len > 1) {
		fprintf(fo," WRITE c=%02x",cfi&0x3f);
		cfi &= 0x3f;
		/* FALLTHROUGH */
	}
	switch (cfi) {
	case 0x01:
		if (pa5pump(pm->dst) && pm->len == 4) {
			adr = (pm->dat[0]<<8) + pm->dat[1];
			val = (pm->dat[2]<<8) + pm->dat[3];
			l = "";
			switch (adr) {
			case IFLO_REG_SPGPM:  l = " GPM setpoint"; break;
			case IFLO_REG_EPRG:   l = " Ext.Ctrl"; break;
			case IFLO_REG_EP1RPM: l = " P1 RPM setpoint"; break;
			case IFLO_REG_EP2RPM: l = " P2 RPM setpoint"; break;
			case IFLO_REG_EP3RPM: l = " P3 RPM setpoint"; break;
			case IFLO_REG_EP4RPM: l = " P4 RPM setpoint"; break;
			}
			fprintf(fo," WRITE (%d) to 0x%04x%s",val,adr,l);
			break;
		}
		if (pa5pump(pm->src) && pm->len == 2) {
			val = (pm->dat[0]<<8) + pm->dat[1];
			fprintf(fo,"     VALIS (%d)",val);
			break;
		}
		if (pa5ctrl(pm->src) && pm->len == 1) {
			fprintf(fo," WRITE c=%02x ACK",pm->dat[0]&0x7f);
			break;
		}
		break;
	case 0x02:
		if (pa5bcst(pm->dst) && pa5ctrl(pm->src) && pm->len == sizeof(itv02_t)) {
			itv02_t	*p = (itv02_t *)pm->dat;
			itv02_t	*l = pl ? (itv02_t *)pl->dat : 0;
			if (CHG(l,p,clk[0]) || CHG(l,p,clk[1]))
				fprintf(fo,"\n%*sclck %02x%02x %02d:%02d",ind,"",p->clk[0],p->clk[1],p->clk[0],p->clk[1]);
			if (CHG(l,p,srly)) fprintf(fo,"\n%*ssrly %02x   relay status",ind,"",p->srly);
			if (CHG(l,p,b03 )) fprintf(fo,"\n%*s[ 3] %02x   ?",ind,"",p->b03);
			if (CHG(l,p,b04 )) fprintf(fo,"\n%*s[ 4] %02x   ?",ind,"",p->b04);
			if (CHG(l,p,b05 )) fprintf(fo,"\n%*s[ 5] %02x   ?",ind,"",p->b05);
			if (CHG(l,p,b06 )) fprintf(fo,"\n%*s[ 6] %02x   ?",ind,"",p->b06);
			if (CHG(l,p,b07 )) fprintf(fo,"\n%*s[ 7] %02x   ?",ind,"",p->b07);
			if (CHG(l,p,b08 )) fprintf(fo,"\n%*s[ 8] %02x   ?",ind,"",p->b08);
			if (CHG(l,p,srem)) fprintf(fo,"\n%*ssrem %02x   remote status",ind,"",p->srem);
			if (CHG(l,p,b10 )) fprintf(fo,"\n%*s[10] %02x   ?",ind,"",p->b10);
			if (CHG(l,p,b11 )) fprintf(fo,"\n%*s[11] %02x   ?",ind,"",p->b11);
			if (CHG(l,p,b12 )) fprintf(fo,"\n%*s[12] %02x   ?",ind,"",p->b12);
			if (CHG(l,p,b13 )) fprintf(fo,"\n%*s[13] %02x   ?",ind,"",p->b13);
			if (CHG(l,p,tpol)) fprintf(fo,"\n%*stpol %02x   %dF pool",ind,"",p->tpol,p->tpol);
			if (CHG(l,p,tspa)) fprintf(fo,"\n%*stspa %02x   %dF spa",ind,"",p->tspa,p->tspa);
			if (CHG(l,p,b16 )) fprintf(fo,"\n%*s[16] %02x   ?",ind,"",p->b16);
			if (CHG(l,p,b17 )) fprintf(fo,"\n%*s[17] %02x   ?",ind,"",p->b17);
			if (CHG(l,p,tair)) fprintf(fo,"\n%*stair %02x   %dF air",ind,"",p->tair,p->tair);
			if (CHG(l,p,tsol)) fprintf(fo,"\n%*stsol %02x   %dF solar",ind,"",p->tsol,p->tsol);
			if (CHG(l,p,b20 )) fprintf(fo,"\n%*s[20] %02x   ?",ind,"",p->b20);
			if (CHG(l,p,b21 )) fprintf(fo,"\n%*s[21] %02x   ?",ind,"",p->b21);
			if (CHG(l,p,b22 )) fprintf(fo,"\n%*s[22] %02x   ?",ind,"",p->b22);
			if (CHG(l,p,b23 )) fprintf(fo,"\n%*s[23] %02x   ?",ind,"",p->b23);
			if (CHG(l,p,b24 )) fprintf(fo,"\n%*s[24] %02x   ?",ind,"",p->b24);
			if (CHG(l,p,b25 )) fprintf(fo,"\n%*s[25] %02x   ?",ind,"",p->b25);
			if (CHG(l,p,b26 )) fprintf(fo,"\n%*s[26] %02x   ?",ind,"",p->b26);
			if (CHG(l,p,b27 )) fprintf(fo,"\n%*s[27] %02x   ?",ind,"",p->b27);
			if (CHG(l,p,b28 )) fprintf(fo,"\n%*s[28] %02x   ?",ind,"",p->b28);
			break;
		}
		break;
	case 0x04:
		if (pa5pump(pm->dst) && pm->len == 1) {
			if (pm->dat[0] == IFLO_DSP_LOC) fprintf(fo," SETCTRL local");
			else if (pm->dat[0] == IFLO_DSP_REM) fprintf(fo," SETCTRL remote");
			break;
		}
		if (pa5pump(pm->src) && pm->len == 1) {
			if (pm->dat[0] == IFLO_DSP_LOC) fprintf(fo," CTRL is local");
			else if (pm->dat[0] == IFLO_DSP_REM) fprintf(fo," CTRL is remote");
			break;
		}
		break;
	case 0x05:
		if (pa5pump(pm->dst) && pm->len == 1) {
			fprintf(fo," SETMOD %02x",pm->dat[0]);
			break;
		}
		if (pa5pump(pm->src) && pm->len == 1) {
			fprintf(fo," MOD is %02x",pm->dat[0]);
			break;
		}
		if (pa5ctrl(pm->src) && pm->len == sizeof(itv05_t)) {
			/* status vector 05 */
			itv05_t	*p = (itv05_t *)pm->dat;
			itv05_t	*l = pl ? (itv05_t *)pl->dat : 0;
			if (CHG(l,p,clk[0]) || CHG(l,p,clk[1]))
				fprintf(fo,"\n%*sclck %02x%02x %02d:%02d",ind,"",p->clk[0],p->clk[1],p->clk[0],p->clk[1]);
			if (CHG(l,p,b02 )) fprintf(fo,"\n%*s[ 2] %02x   ?",ind,"",p->b02);
			if (CHG(l,p,b03 )) fprintf(fo,"\n%*s[ 3] %02x   ?",ind,"",p->b03);
			if (CHG(l,p,b04 )) fprintf(fo,"\n%*s[ 4] %02x   ?",ind,"",p->b04);
			if (CHG(l,p,b05 )) fprintf(fo,"\n%*s[ 5] %02x   ?",ind,"",p->b05);
			if (CHG(l,p,b06 )) fprintf(fo,"\n%*s[ 6] %02x   ?",ind,"",p->b06);
			if (CHG(l,p,b07 )) fprintf(fo,"\n%*s[ 7] %02x   ?",ind,"",p->b07);
			break;
		}
		break;
	case 0x06:
		if (pa5pump(pm->dst) && pm->len == 1) {
			fprintf(fo," SETRUN %02x %s",pm->dat[0],
				pm->dat[0] == IFLO_RUN_STRT ? "Started" :
				pm->dat[0] == IFLO_RUN_STOP ? "Stopped" :
				"?");
			break;
		}
		if (pa5pump(pm->src) && pm->len == 1) {
			fprintf(fo," RUN is %02x %s",pm->dat[0],
				pm->dat[0]==IFLO_RUN_STRT ? "Started" :
				pm->dat[0]==IFLO_RUN_STOP ? "Stopped" :
				"?");
			break;
		}
		break;
	case 0x07:
		if (pa5pump(pm->dst) && pm->len == 0) {
			fprintf(fo," SEND status");
			break;
		}
		if (pa5pump(pm->src) && pm->len == sizeof(iflsr_t)) {
			iflsr_t	*p = (iflsr_t *)pm->dat;
			iflsr_t	*l = pl ? (iflsr_t *)pl->dat : 0;
			u16_t	v;
			if (CHG(l,p,run)) fprintf(fo,"\n%*sRUN %02x   %s",ind,"",p->run,
				p->run==IFLO_RUN_STRT ? "Started":
				p->run==IFLO_RUN_STOP ? "Stopped":
				"?");
			if (CHG(l,p,mod)) fprintf(fo,"\n%*sMOD %02x   %s",ind,"",p->mod,
				p->mod==IFLO_MOD_FILTER ? "Filter":
				p->mod==IFLO_MOD_MANUAL ? "Manual":
				p->mod==IFLO_MOD_BKWASH ? "Backwash":
				p->mod==IFLO_MOD_FEATR1 ? "Feature 1":
				p->mod==IFLO_MOD_EXT_P1 ? "Ext.Ctrl 1":
				p->mod==IFLO_MOD_EXT_P2 ? "Ext.Ctrl 2":
				p->mod==IFLO_MOD_EXT_P3 ? "Ext.Ctrl 3":
				p->mod==IFLO_MOD_EXT_P4 ? "Ext.Ctrl 4":
				"?");
			if (CHG(l,p,pmp)) fprintf(fo,"\n%*sPMP %02x   %s",ind,"",p->pmp,
				p->pmp==IFLO_PMP_READY ? "ready":
				"?");
			v = ntohs(p->pwr);
			if (CHG(l,p,pwr)) fprintf(fo,"\n%*sPWR %04x %d WATT",ind,"",v,v);
			v = ntohs(p->rpm);
			if (CHG(l,p,rpm)) fprintf(fo,"\n%*sRPM %04x %d RPM",ind,"",v,v);
			if (CHG(l,p,gpm)) fprintf(fo,"\n%*sGPM %02x   %d GPM",ind,"",p->gpm,p->gpm);
			if (CHG(l,p,ppc)) fprintf(fo,"\n%*sPPC %02x   %d %%",ind,"",p->ppc,p->ppc);
			if (CHG(l,p,b09)) fprintf(fo,"\n%*sb09 %02x   ?",ind,"",p->b09);
			if (CHG(l,p,err)) {
				fprintf(fo,"\n%*sERR %02x   ",ind,"",p->err);
				if (p->err==0x00) fprintf(fo,"ok");
				if (p->err &0x80) fprintf(fo,"!ESTOP");
				if (p->err &0x02) fprintf(fo,"!ALERT");
				if (p->err &0x7d) fprintf(fo,"(?)");
			}
			if (CHG(l,p,b11)) fprintf(fo,"\n%*sb11 %02x   ?",ind,"",p->b11);
			if (CHG(l,p,tmr)) fprintf(fo,"\n%*sTMR %02x   %d MIN",ind,"",p->tmr,p->tmr);
			if (CHG(l,p,clk[0]) || CHG(l,p,clk[1]))
				fprintf(fo,"\n%*sCLK %02x%02x %02d:%02d",ind,"",p->clk[0],p->clk[1],p->clk[0],p->clk[1]);
			break;
		}
		break;
	case 0x08:
		if (pa5ctrl(pm->src) && pm->len == sizeof(itv08_t)) {
			/* status vector 08 */
			itv08_t	*p = (itv08_t *)pm->dat;
			itv08_t	*l = pl ? (itv08_t *)pl->dat : 0;
			if (CHG(l,p,tcpol)) fprintf(fo,"\n%*stcpol %02x   %dF pool",ind,"",p->tcpol,p->tcpol);
			if (CHG(l,p,tcspa)) fprintf(fo,"\n%*stcspa %02x   %dF spa",ind,"",p->tcspa,p->tcspa);
			if (CHG(l,p,tcair)) fprintf(fo,"\n%*stcair %02x   %dF air",ind,"",p->tcair,p->tcair);
			if (CHG(l,p,tspol)) fprintf(fo,"\n%*stspol %02x   %dF pool setpoint",ind,"",p->tspol,p->tspol);
			if (CHG(l,p,tspol)) fprintf(fo,"\n%*stsspa %02x   %dF spa  setpoint",ind,"",p->tsspa,p->tsspa);
			if (CHG(l,p,b05  )) fprintf(fo,"\n%*s[ 5]  %02x   ?",ind,"",p->b05);
			if (CHG(l,p,b06  )) fprintf(fo,"\n%*s[ 6]  %02x   ?",ind,"",p->b06);
			if (CHG(l,p,b07  )) fprintf(fo,"\n%*s[ 7]  %02x   ?",ind,"",p->b07);
			if (CHG(l,p,tcsol)) fprintf(fo,"\n%*stcsol %02x   %dF solar",ind,"",p->tcsol,p->tcsol);
			if (CHG(l,p,b09  )) fprintf(fo,"\n%*s[ 9]  %02x   ?",ind,"",p->b09);
			if (CHG(l,p,b10  )) fprintf(fo,"\n%*s[10]  %02x   ?",ind,"",p->b10);
			if (CHG(l,p,b11  )) fprintf(fo,"\n%*s[11]  %02x   ?",ind,"",p->b11);
			if (CHG(l,p,b12  )) fprintf(fo,"\n%*s[12]  %02x   ?",ind,"",p->b12);
			break;
		}
		break;
	case 0x0a:
		if (pa5ctrl(pm->src) && pm->len > 0) {
			/* seems to be some kind of button label */
			char	*t;
			int	n;
			fprintf(fo," \"");
			for (t = (char *)pm->dat, n = pm->len; --n >= 0; t++)
				fprintf(fo,"%c",*t>=' '&&*t<='~'?*t:'.');
			fprintf(fo,"\"");
			break;
		}
		break;
	case 0x16:
		if (pa5ctrl(pm->src) && pm->len == sizeof(itv16_t)) {
			itv16_t	*p = (itv16_t *)pm->dat;
			itv16_t	*l = pl ? (itv16_t *)pl->dat : 0;
			if (CHG(l,p,b00 )) fprintf(fo,"\n%*s[ 0]  %02x   ?",ind,"",p->b00);
			if (CHG(l,p,b01 )) fprintf(fo,"\n%*s[ 1]  %02x   ?",ind,"",p->b01);
			if (CHG(l,p,rpm1)) fprintf(fo,"\n%*srpm1  %04x %d RPM P1",ind,"",ntohs(p->rpm1),ntohs(p->rpm1));
			if (CHG(l,p,b04 )) fprintf(fo,"\n%*s[ 4]  %02x   ?",ind,"",p->b04);
			if (CHG(l,p,rpm2)) fprintf(fo,"\n%*srpm2  %04x %d RPM P2",ind,"",ntohs(p->rpm2),ntohs(p->rpm2));
			if (CHG(l,p,b07 )) fprintf(fo,"\n%*s[ 7]  %02x   ?",ind,"",p->b07);
			if (CHG(l,p,rpm3)) fprintf(fo,"\n%*srpm3  %04x %d RPM P3",ind,"",ntohs(p->rpm3),ntohs(p->rpm3));
			if (CHG(l,p,b10 )) fprintf(fo,"\n%*s[10]  %02x   ?",ind,"",p->b10);
			if (CHG(l,p,rpm4)) fprintf(fo,"\n%*srpm4  %04x %d RPM P4",ind,"",ntohs(p->rpm4),ntohs(p->rpm4));
			break;
		}
		break;
	case 0x17:
		if (pa5ctrl(pm->src) && pm->len == sizeof(itv17_t)) {
			itv17_t	*p = (itv17_t *)pm->dat;
			itv17_t	*l = pl ? (itv17_t *)pl->dat : 0;
			if (CHG(l,p,b00)) fprintf(fo,"\n%*s[ 0]  %02x   ?",ind,"",p->b00);
			if (CHG(l,p,b01)) fprintf(fo,"\n%*s[ 1]  %02x   ?",ind,"",p->b01);
			if (CHG(l,p,b02)) fprintf(fo,"\n%*s[ 2]  %02x   ?",ind,"",p->b02);
			if (CHG(l,p,b03)) fprintf(fo,"\n%*s[ 3]  %02x   ?",ind,"",p->b03);
			if (CHG(l,p,b04)) fprintf(fo,"\n%*s[ 4]  %02x   ?",ind,"",p->b04);
			if (CHG(l,p,b05)) fprintf(fo,"\n%*s[ 5]  %02x   ?",ind,"",p->b05);
			if (CHG(l,p,b06)) fprintf(fo,"\n%*s[ 6]  %02x   ?",ind,"",p->b06);
			if (CHG(l,p,b07)) fprintf(fo,"\n%*s[ 7]  %02x   ?",ind,"",p->b07);
			if (CHG(l,p,b08)) fprintf(fo,"\n%*s[ 8]  %02x   ?",ind,"",p->b08);
			if (CHG(l,p,b09)) fprintf(fo,"\n%*s[ 9]  %02x   ?",ind,"",p->b09);
			if (CHG(l,p,b10)) fprintf(fo,"\n%*s[10]  %02x   ?",ind,"",p->b10);
			if (CHG(l,p,b11)) fprintf(fo,"\n%*s[11]  %02x   ?",ind,"",p->b11);
			if (CHG(l,p,b12)) fprintf(fo,"\n%*s[12]  %02x   ?",ind,"",p->b12);
			if (CHG(l,p,b13)) fprintf(fo,"\n%*s[13]  %02x   ?",ind,"",p->b13);
			if (CHG(l,p,b14)) fprintf(fo,"\n%*s[14]  %02x   ?",ind,"",p->b14);
			if (CHG(l,p,b15)) fprintf(fo,"\n%*s[15]  %02x   ?",ind,"",p->b15);
			break;
		}
		break;
	}
}

#define	NOSYN	0x00000001
#define	NOTIM	0x00000002
#define	NOHUB	0x00000004
#define	NOADR	0x00000008
#define	NODEC	0x00000010
#define	NOREP	0x00000020

u08_t *findpat(u08_t *b, u08_t *e, u08_t *p, int np)
{
	int	n, k;

	if ((n = (e - p) - np) < 0) return 0;
	do for (k = np; --k >= 0 && b[k] == p[k]; );
	while (k >= 0 && --n >= 0 && ++b < e);
	return k < 0 ? b : 0;
}

typedef struct { /* message cache */
	u32_t	pos;
	u08_t	msg[64-4];
}	mce_t;

typedef struct {
	u32_t	pos;	/* current record position */
	tmv_t	tim;	/* time stamp */
	u32_t	flg;	/* command line flags */
	int	afl;	/* address match */
	int	ind;	/* print indent for pa5deco() */
	int	nmce;	/* number of messages in cache */
	mce_t	mces[64];
}	ctx_t;

char *tmv2str(tmv_t *tc, char *str)
{
	char	*s = str;
	struct tm	tm;

	localtime_r(&tc->tv_sec,&tm);
	s += sprintf(s,"%02d%02d ",tm.tm_mon+1,tm.tm_mday);
	s += sprintf(s,"%02d:%02d:%02d.%03lu",tm.tm_hour,tm.tm_min,tm.tm_sec,tc->tv_usec/1000);
	return str;
}

void pr_hdr(FILE *fo, ctx_t *c, char *typ)
{
	char	tbu[32];

	c->ind =  (c->flg & NOADR) ? 0 : fprintf(fo,"%08lx:",c->pos);
	c->ind += (c->flg & NOTIM) ? fprintf(fo,"%3s: ",typ?typ:"???") : fprintf(fo,"%s ",tmv2str(&c->tim,tbu));
}

mce_t *mce_a5lup(ctx_t *c, u08_t *msg, int msz)
{
	mce_t	*mc;
	int	n;

	if (msz > sizeof(mc->msg)) return 0;
	if (!(c->flg & NOREP)) return 0;
	n = (c->flg & NODEC) || c->afl ? 0 : 5;
	if (((pa5_t *)msg)->len < n) return 0;
	for (mc = c->mces, n = c->nmce; --n >= 0; mc++)
		if (!bcmp(msg,mc->msg,sizeof(pa5_t))) return mc;
	if (c->nmce < NEL(c->mces)) {
		mc->pos = c->pos;
		bcopy(msg,mc->msg,msz);
		c->nmce++;
	}
	return 0;
}

int a5xx_msg(FILE *fo, ctx_t *c, u08_t *b, u08_t *e)
{
	u08_t	*s = b, *t;
	pa5_t	*pm;
	mce_t	*mc;
	u16_t	sum, cks;
	int	n, msz;

	if ((e - b) < 8) return 0;
	if (&b[(n = 6 + b[5]) + 2] > e) return 0;
	for (sum = 0; --n >= 0; sum += *s++);
	cks = (s[0]<<8)+s[1];
	if (sum != cks) return 0;
	msz = (s - b) + 2;
	pm = (pa5_t *)b;
	if (c->afl && pm->dst != c->afl && pm->src != c->afl) return msz;
	pr_hdr(fo,c,"msg");
	fprintf(fo,"%02X%02X ",pm->lpb,pm->sub);
	fprintf(fo,"d=%02x s=%02x ",pm->dst,pm->src);
	fprintf(fo,"c=%02x l=%02x ",pm->cfi,pm->len);
	if ((mc = mce_a5lup(c,b,msz)) && !bcmp(b,mc->msg,msz)) {
		fprintf(fo,"REPEAT");
		if (!(c->flg & NOADR)) fprintf(fo," %08lx",mc->pos);
	}
	else {
		for (t = pm->dat, n = pm->len; --n >= 0; t++) fprintf(fo,"%02X",*t);
		fprintf(fo,"%s<%02X%02X>",pm->len?" ":"",s[0],s[1]);
		if (!(c->flg & NODEC)) pa5deco(fo,c->ind,pm,mc?(pa5_t *)mc->msg:0);
		if (mc) mc->pos = c->pos, bcopy(b,mc->msg,msz);
	}
	return msz;
}

int m1090_msg(FILE *fo, ctx_t *c, u08_t *b, u08_t *e)
{
	union { char *c; u08_t *b; u16_t *w; u32_t *l; } p;
	u08_t	*s = b, *t, eom[2], sum;
	int	msz, n;
	/*
	 * 'palog' context message
	 *  1090 ..... <cks> 1091
	 */
	if ((e - b) < 13) return 0;
	eom[0] = 0x10; eom[1] = 0x91;
	if ((t = b + 64) > e) t = e;
	if ((s = findpat(b+2,t,eom,2)) == 0) return 0;
	if ((msz = (s - b) + 2) < 13) return 0;
	for (sum = 0x12, s--, t = b + 2; t < s; sum += *t++);
	if (sum != *s) return 0;
	p.b = &b[2];
	c->tim.tv_sec  = ntohl(*p.l); p.l++;
	c->tim.tv_usec = ntohl(*p.l); p.l++;
	if (c->flg & NOHUB) return msz;
	if (c->afl) return msz;
	pr_hdr(fo,c,"pab");
	n = s - &p.b[1];
	fprintf(fo,"%c %.*s\n",*p.c,n<0?0:n,&p.c[1]);
	return msz;
}

int m10xx_msg(FILE *fo, ctx_t *c, u08_t *b, u08_t *e)
{
	u08_t	*s = b, *t, eom[2], sum;
	int	msz;
	/*
	 *  1002 ..... <cks> 1003
	 */
	if ((e - b) < 4) return 0;
	eom[0] = 0x10; eom[1] = 0x03;
	if ((t = b + 64) > e) t = e;
	if ((s = findpat(b+2,t,eom,2)) == 0) return 0;
	msz = (s - b) + 2;
	for (sum = 0x12, s--, t = b + 2; t < s; sum += *t++);
	if (sum != *s) return 0;
	if (c->afl) return msz;
	pr_hdr(fo,c,"msg");
	fprintf(fo,"%02X%02X ",b[0],b[1]);
	for (b += 2; b < s; b++) fprintf(fo,"%02X",*b);
	fprintf(fo," <%02X> %02X%02X ",b[0],b[1],b[2]);
	return msz;
}

int dumplog(int fd, ctx_t *c, FILE *fo)
{
	int	eof, n, k, syn;
	u32_t	iop;
	u08_t	*t, *b, *r, *w, *e, iob[2*BUFSIZ];

	for (iop = 0, e = (r = w = iob) + sizeof(iob), eof = 0; !eof || r != w; ) {
		if ((n = w - r) <= (NEL(iob)/2)) {
			iop += r - iob;
			if (n <= 0) r = w = iob;
			else if (r > iob) bcopy(r,iob,n), w = (r = iob) + n;
			if (!eof) {
				if ((n = read(fd,w,k=e-w)) <= 0) eof = 1;
				else w += n, eof = n < k;
			}
		}
		/* r->(data), w->(end of data present) */
		for (b = r; b < w && (b - r) < 16; b++) {
			if (*b == 0xa5) break;
			if (*b == 0x10 && (w - b) >= 2) {
				if (b[1] == 0x02) break;
				if (b[1] == 0x90) break;
			}
		}
		/* r->(data), b->(a potential message), w->(end of data present) */
		c->pos = iop + (r - iob);
		c->ind = 0;
		if ((n = b - r) > 0) {
			if ((syn = n >= 3)) {
				for (t = r; t < b && *t == 0xff; t++);
				syn = (b - t) == 2 && t[0] == 0x00 && t[1] == 0xff;
			}
			if (!syn) {
				for (b = r; ++b < w && (b - r) < 16; )
					if (*b == 0xa5 || *b == 0x10 || *b == 0xff) break;
				n = b - r;
			}
			while (1) {
				if (syn && (c->flg & NOSYN)) break;
				if (c->afl) break;
				pr_hdr(fo,c,syn?"syn":"??b");
				for (t = r; t < b; t++) fprintf(fo,"%02X",*t);
				fprintf(fo,"\n");
				break;
			}
			r += n;
			continue;
		}
		/* r->(data, potential message), w->(end of data present) */
		if (*r == 0xa5) n = a5xx_msg(fo,c,r,w);
		else if (*r == 0x10) {
			if (r[1] == 0x90) {
				if ((n = m1090_msg(fo,c,r,w)) > 0) {
					r += n;
					continue;
				}
			}
			else n = m10xx_msg(fo,c,r,w);
		}
		else n = 0;
		/* n is the number of bytes in a recognized message */
		if (n <= 0) {
			for (b = r; ++b < w && (b - r) < 16; )
				if (*b == 0xa5 || *b == 0x10 || *b == 0xff) break;
			n = b - r;
			if (c->afl == 0) {
				pr_hdr(fo,c,"???");
				for (t = r; t < b; t++) fprintf(fo,"%02X",*t);
			}
		}
		r += n;
		if (c->ind) fprintf(fo,"\n");
	}
	return 0;
}

int main(int ac, char **av)
{
	FILE	*fo = stdout;
	ctx_t	ctx;
	int	rv, fd, i, n;
	char	*p, *r;

	if (ac <= 1) goto USAGE;
	bzero(&ctx,sizeof(ctx));
	ctx.flg = NOSYN|NOTIM|NOHUB|NOADR|NODEC|NOREP;
	for (rv = i = 0; !rv && ++i < ac; ) {
		if (*(p = av[i]) == '-')
			while (*(++p)) switch (*p) {
			case 's': ctx.flg ^= NOSYN; break;
			case 't': ctx.flg ^= NOTIM; break;
			case 'h': ctx.flg ^= NOHUB; break;
			case 'a': ctx.flg ^= NOADR; break;
			case 'd': ctx.flg ^= NODEC; break;
			case 'r': ctx.flg ^= NOREP; break;
			case 'f':
				if (++i >= ac) goto USAGE;
				if ((n = strtol(av[i],&r,16)) <= 0 || n > 0xff || *r) goto USAGE;
				ctx.afl = n;
				break;
			default:
USAGE:				fprintf(fo,"%s\n",version);
				fprintf(fo,"usage: [-<options>] <datafile>\n");
				fprintf(fo,"s     - print sync bytes\n");
				fprintf(fo,"t     - print time stamps\n");
				fprintf(fo,"h     - print 'palog' messages\n");
				fprintf(fo,"a     - print record positions in file\n");
				fprintf(fo,"d     - decode messages\n");
				fprintf(fo,"r     - print full decode of repeated messages\n");
				fprintf(fo,"f <#> - print only messages from/to address <#>\n");
				return EINVAL;
				break;
			}
		else if ((fd = open(av[i],0)) < 0)
			rv = errno, fprintf(fo,"%s: %s\n",av[i],strerror(errno));
		else {
			fprintf(fo,"# %s:",version+5);
			for (n = 0; ++n <= i; ) fprintf(fo," %s",av[n]);
			fprintf(fo,"\n");
			rv = dumplog(fd,&ctx,fo);
			close(fd);
		}
	}
	return rv;
}

