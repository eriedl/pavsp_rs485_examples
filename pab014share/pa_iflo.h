#ifndef	__pa_iflo_h__
#define	__pa_iflo_h__

#define	IFLO_REG	1
#define	IFLO_REG_SPGPM	0x02e4

#define	IFLO_REG_EPRG	0x0321
#define	IFLO_EPRG_P0	0x0000
#define	IFLO_EPRG_P1	0x0008
#define	IFLO_EPRG_P2	0x0010
#define	IFLO_EPRG_P3	0x0018
#define	IFLO_EPRG_P4	0x0020

#define	IFLO_REG_EP1RPM	0x0327	/* 4x160 */
#define	IFLO_REG_EP2RPM	0x0328	/* 4x160 */
#define	IFLO_REG_EP3RPM	0x0329	/* 4x160 */
#define	IFLO_REG_EP4RPM	0x032a	/* 4x160 */

#define	IFLO_DSP	4
#define	IFLO_DSP_LOC	0x00
#define	IFLO_DSP_REM	0xff

#define	IFLO_MOD	5
#define	IFLO_MOD_FILTER	0x00 /* Filter */
#define	IFLO_MOD_MANUAL	0x01 /* Manual */
#define	IFLO_MOD_BKWASH	0x02
#define	IFLO_MOD______3	0x03 /* never seen */
#define	IFLO_MOD______4	0x04 /* never seen */
#define	IFLO_MOD______5	0x05 /* never seen */
#define	IFLO_MOD_FEATR1	0x06 /* Feature 1 */
#define	IFLO_MOD______7	0x07 /* never seen */
#define	IFLO_MOD______8	0x08 /* never seen */
#define	IFLO_MOD_EXT_P1	0x09
#define	IFLO_MOD_EXT_P2	0x0a
#define	IFLO_MOD_EXT_P3	0x0b
#define	IFLO_MOD_EXT_P4	0x0c

#define	IFLO_RUN	6
#define IFLO_RUN_STRT	0x0a
#define IFLO_RUN_STOP	0x04

/* IntelliFlow VS status command response */
#define	IFLO_SRG	7
typedef struct {
	u08_t	run;
	u08_t	mod;
	u08_t	pmp;	/* looks like drive status */
#define	IFLO_PMP_READY	0x02
	u16_t	pwr;
	u16_t	rpm;
	u08_t	gpm;
	u08_t	ppc;
	u08_t	b09;
	u08_t	err;
	u08_t	b11;
	u08_t	tmr;
	u08_t	clk[2];
} __attribute__ ((packed)) iflsr_t;

#endif

