#ifndef __pa_ctrl_h__
#define __pa_ctrl_h__

/* Controllers: EasyTouch, IntelliTouch */

typedef struct { /* status vector 02, the one that gets sent on a regular basis */
	u08_t	clk[2];
	u08_t	srly;	/* relay status */
	u08_t	b03;
	u08_t	b04;
	u08_t	b05;
	u08_t	b06;
	u08_t	b07;
	u08_t	b08;
	u08_t	srem;	/* remote status */
	u08_t	b10;
	u08_t	b11;
	u08_t	b12;
	u08_t	b13;
	u08_t	tpol;	/* pool temp */
	u08_t	tspa;	/* spa temp */
	u08_t	b16;
	u08_t	b17;
	u08_t	tair;	/* air temp */
	u08_t	tsol;	/* solar temp */
	u08_t	b20;
	u08_t	b21;
	u08_t	b22;
	u08_t	b23;
	u08_t	b24;
	u08_t	b25;
	u08_t	b26;
	u08_t	b27;
	u08_t	b28;
} __attribute__ ((packed)) itv02_t;

typedef struct {
	u08_t	clk[2];
	u08_t	b02;
	u08_t	b03;
	u08_t	b04;
	u08_t	b05;
	u08_t	b06;
	u08_t	b07;
} __attribute__ ((packed)) itv05_t;

typedef struct {
	u08_t	tcpol;
	u08_t	tcspa;
	u08_t	tcair;
	u08_t	tspol;
	u08_t	tsspa;
	u08_t	b05;
	u08_t	b06;
	u08_t	b07;
	u08_t	tcsol;
	u08_t	b09;
	u08_t	b10;
	u08_t	b11;
	u08_t	b12;
} __attribute__ ((packed)) itv08_t;

typedef struct {
	u08_t	b00;
	u08_t	b01;
	u16_t	rpm1;
	u08_t	b04;
	u16_t	rpm2;
	u08_t	b07;
	u16_t	rpm3;
	u08_t	b10;
	u16_t	rpm4;
} __attribute__ ((packed)) itv16_t;

typedef struct {
	u08_t	b00;
	u08_t	b01;
	u08_t	b02;
	u08_t	b03;
	u08_t	b04;
	u08_t	b05;
	u08_t	b06;
	u08_t	b07;
	u08_t	b08;
	u08_t	b09;
	u08_t	b10;
	u08_t	b11;
	u08_t	b12;
	u08_t	b13;
	u08_t	b14;
	u08_t	b15;
} __attribute__ ((packed)) itv17_t;

#endif

