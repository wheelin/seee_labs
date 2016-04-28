#ifndef __REPTAR_SP6_H__
#define __REPTAR_SP6_H__

/* Only LEDS 0 to 5 are under CPU control. 6 and 7 are used by the FPGA itself */
#define SP6_NUM_LEDS 6

struct reptar_sp6_led_platdata {
	char	*name;
	int		bit;		/* bit number in LEDS register */
	int 	reg_offset;	/* offset from FPGA base address */
};

#define SP6_NUM_KEYS 8

struct reptar_sp6_buttons_platdata {
	unsigned int btns_reg_offset;
	unsigned int irq_reg_offset;
	unsigned char keys[SP6_NUM_KEYS];
};

extern struct reptar_sp6_buttons_platdata reptar_sp6_btns_pdata;
extern struct reptar_sp6_led_platdata reptar_sp6_leds_pdata[];

extern int reptar_sp6_leds_init(struct platform_device *);
extern void reptar_sp6_leds_exit(void);

extern int reptar_sp6_buttons_init(struct platform_device *);
extern void reptar_sp6_buttons_exit(void);

#endif /* __REPTAR_SP6_H__ */
