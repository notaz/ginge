/*
 * include/asm-arm/arch-mmsp2/mmsp2-regs.h
 *
 * Copyright (C) 2004,2005 DIGNSYS Inc. (www.dignsys.com)
 * Kane Ahn < hbahn@dignsys.com >
 * hhsong < hhsong@dignsys.com >
 */

#ifndef _MMSP2_H
#define _MMSP2_H

static const char * const regnames[0x10000] = {

/*
 * BANK C (Static) Memory Control Register
 */
[0x3a00]	= "MEMCFG",	/* BANK C configuration */
[0x3a02]	= "MEMTIME0",	/* BANK C Timing #0 */
[0x3a04]	= "MEMTIME1",	/* BANK C Timing #1 */
[0x3a06]	= "MEMTIME2",	/* BANK C Timing #2 */
[0x3a08]	= "MEMTIME3",	/* BANK C Timing #3 */
[0x3a0a]	= "MEMTIME4",	/* BANK C Timing #4 */
[0x3a0e]	= "MEMWAITCTRL",	/* BANK C Wait Control */
[0x3a10]	= "MEMPAGE",	/* BANK C Page Control */
[0x3a12]	= "MEMIDETIME",	/* BANK C IDE Timing Control */

[0x3a14]	= "MEMPCMCIAM",	/* BANK C PCMCIA Timing */
[0x3a16]	= "MEMPCMCIAA",	/* PCMCIA Attribute Timing */
[0x3a18]	= "MEMPCMCIAI",	/* PCMCIA I/O Timing */
[0x3a1a]	= "MEMPCMCIAWAIT",	/* PCMCIA Wait Timing */

[0x3a1c]	= "MEMEIDEWAIT",	/* IDE Wait Timing */

[0x3a20]	= "MEMDTIMEOUT",	/* DMA Timeout */
[0x3a22]	= "MEMDMACTRL",	/* DMA Control */
[0x3a24]	= "MEMDMAPOL",	/* DMA Polarity */
[0x3a26]	= "MEMDMATIME0",	/* DMA Timing #0 */
[0x3a28]	= "MEMDMATIME1",	/* DMA Timing #1 */
[0x3a2a]	= "MEMDMATIME2",	/* DMA Timing #2 */
[0x3a2c]	= "MEMDMATIME3",	/* DMA Timing #3 */
[0x3a2e]	= "MEMDMATIME4",	/* DMA Timing #4 */
[0x3a30]	= "MEMDMATIME5",	/* DMA Timing #5 */
[0x3a32]	= "MEMDMATIME6",	/* DMA Timing #6 */
[0x3a34]	= "MEMDMATIME7",	/* DMA Timing #7 */
[0x3a36]	= "MEMDMATIME8",	/* DMA Timing #8 */
[0x3a38]	= "MEMDMASTRB",	/* DMA Strobe Control */

[0x3a3a]	= "MEMNANDCTRL",	/* NAND FLASH Control */
[0x3a3c]	= "MEMNANDTIME",	/* NAND FLASH Timing */
[0x3a3e]	= "MEMNANDECC0",	/* NAND FLASH ECC0 */
[0x3a40]	= "MEMNANDECC1",	/* NAND FLASH ECC1 */
[0x3a42]	= "MEMNANDECC2",	/* NAND FLASH ECC2 */
[0x3a44]	= "MEMNANDCNT",	/* NAND FLASH Data Counter */

/* Bank A Memory (SDRAM) Control Register */
[0x3800]	= "MEMCFGX",	/* SDRAM Configuration */
[0x3802]	= "MEMTIMEX0",	/* SDRAM Timing #0 */
[0x3804]	= "MEMTIMEX1",	/* SDRAM Timing #1 */
[0x3806]	= "MEMACTPWDX",	/* Active Power Down Ctrl */
[0x3808]	= "MEMREFX",	/* Refresh Ctrl */

/*
 * Chapter 5
 * Clocks and Power Manager
 */
[0x0900]	= "PWMODE",	/* Power Mode */
[0x0902]	= "CLKCHGST",	/* Clock Change Status */
[0x0904]	= "SYSCLKEN",	/* System Clock Enable */
[0x0908]	= "COMCLKEN", /* Communication Device Clk En */
[0x090a]	= "VGCLKEN", /* Video & Graphic Device Clk En */
[0x090c]	= "ASCLKEN", /* Audio & Storage Device Clk En */
[0x0910]	= "FPLLSETV", /* FCLK PLL Setting Value Write */
[0x0912]	= "FPLLVSET", /* FCLK PLL Value Setting */
[0x0914]	= "UPLLSETV", /* UCLK PLL Setting Value Write */
[0x0916]	= "UPLLVSET", /* UCLK PLL Value Setting */
[0x0918]	= "APLLSETV", /* ACLK PLL Setting Value Write */
[0x091a]	= "APLLVSET", /* ACLK PLL Value Setting */
[0x091c]	= "SYSCSET", /* System CLK PLL Divide Value */
[0x091e]	= "ESYSCSET", /* External System Clk Time Set */
[0x0920]	= "UIRMCSET", /* USB/IRDA/MMC Clk Gen */
[0x0922]	= "AUDICSET", /* Audio Ctrl Clk Gen */
[0x092e]	= "SPDICSET", /* SPDIF Ctrl Clk Gen */
[0x0924]	= "DISPCSET", /* Display Clk Gen */
[0x0926]	= "IMAGCSET", /* Image Pixel Clk Gen */
[0x0928]	= "URT0CSET", /* UART 0/1 Clk Gen */
[0x092a]	= "UAR1CSET", /* UART 2/3 Clk Gen */
[0x092c]	= "A940TMODE", /* ARM940T CPU Power Manage Mode */

/*
 * Interrupt
 */
[0x0800]	= "SRCPEND",
[0x0804]	= "INTMOD",
[0x0808]	= "INTMASK",
[0x080c]	= "IPRIORITY",
[0x0810]	= "INTPEND",
[0x0814]	= "INTOFFSET",

/*
 * DMA
 */
[0x0000]	= "DMAINT",

/*
 * UART
 */

[0x1200]	= "ULCON0",
[0x1202]	= "UCON0",
[0x1204]	= "UFCON0",
[0x1206]	= "UMCON0",
[0x1208]	= "UTRSTAT0",
[0x120a]	= "UERRSTAT0",
[0x120c]	= "UFIFOSTAT0",
[0x120e]	= "UMODEMSTAT0",
[0x1210]	= "UTHB0",
[0x1212]	= "URHB0",
[0x1214]	= "UBRD0",
[0x1216]	= "UTIMEOUTREG0",

[0x1220]	= "ULCON1",
[0x1222]	= "UCON1",
[0x1224]	= "UFCON1",
[0x1226]	= "UMCON1",
[0x1228]	= "UTRSTAT1",
[0x122a]	= "UERRSTAT1",
[0x122c]	= "UFIFOSTAT1",
[0x122e]	= "UMODEMSTAT1",
[0x1230]	= "UTHB1",
[0x1232]	= "URHB1",
[0x1234]	= "UBRD1",
[0x1236]	= "UTIMEOUTREG1",

[0x1240]	= "ULCON2",
[0x1242]	= "UCON2",
[0x1244]	= "UFCON2",
[0x1246]	= "UMCON2",
[0x1248]	= "UTRSTAT2",
[0x124a]	= "UERRSTAT2",
[0x124c]	= "UFIFOSTAT2",
[0x124e]	= "UMODEMSTAT2",
[0x1250]	= "UTHB2",
[0x1252]	= "URHB2",
[0x1254]	= "UBRD2",
[0x1256]	= "UTIMEOUTREG2",

[0x1260]	= "ULCON3",
[0x1262]	= "UCON3",
[0x1264]	= "UFCON3",
[0x1266]	= "UMCON3",
[0x1268]	= "UTRSTAT3",
[0x126a]	= "UERRSTAT3",
[0x126c]	= "UFIFOSTAT3",
[0x126e]	= "UMODEMSTAT3",
[0x1270]	= "UTHB3",
[0x1272]	= "URHB3",
[0x1274]	= "UBRD3",
[0x1276]	= "UTIMEOUTREG3",

[0x1280]	= "UINTSTAT",
[0x1282]	= "UPORTCON",

/*
 * Timer / Watch-dog
 */
[0x0a00]	= "TCOUNT",
[0x0a04]	= "TMATCH0",
[0x0a08]	= "TMATCH1",
[0x0a0c]	= "TMATCH2",
[0x0a10]	= "TMATCH3",
[0x0a14]	= "TCONTROL",
[0x0a16]	= "TSTATUS",
[0x0a18]	= "TINTEN",

/*
 * Real Time Clock (RTC)
 */
[0x0c00]	= "RTCTSET",
[0x0c04]	= "RTCTCNT",
[0x0c08]	= "RTCSTCNT",
[0x0c0a]	= "TICKSET",
[0x0c0c]	= "ALARMT",
[0x0c10]	= "PWRMGR",
[0x0c12]	= "CLKMGR",
[0x0c14]	= "RSTCTRL",
[0x0c16]	= "RSTST",
[0x0c18]	= "BOOTCTRL",
[0x0c1a]	= "LOCKTIME",
[0x0c1c]	= "RSTTIME",
[0x0c1e]	= "EXTCTRL",
[0x0c20]	= "STOPTSET",
[0x0c22]	= "RTCCTRL",
[0x0c24]	= "RTSTRL",

/*
 * I2C
 */
[0x0d00]	= "IICCON",
[0x0d02]	= "IICSTAT",
[0x0d04]	= "IICADD",
[0x0d06]	= "IICDS",

/*
 * AC97
 */
[0x0E00]	= "AC_CTL",	/* Control Register */
[0x0E02]	= "AC_CONFIG",	/* Config Register */
[0x0E04]	= "AC_STA_EN",	/* Status Enable Register */
[0x0E06]	= "AC_GSR",	/* Global Status Register */
[0x0E08]	= "AC_ST_MCH",	/* State Machine */
[0x0E0C]	= "AC_ADDR",	/* Codec Address Register */
[0x0E0E]	= "AC_DATA",	/* Codec Read Data Register */
[0x0E10]	= "AC_CAR",	/* Codec Access Register */
[0x0F00]	= "AC_REG_BASE",	/* AC97 Codec Register Base */

/* USB Device */
[0x1400]	= "FUNC_ADDR_REG",
[0x1402]	= "PWR_REG",
[0x1404]	= "EP_INT_REG",
[0x140C]	= "USB_INT_REG",
[0x140E]	= "EP_INT_EN_REG",
[0x1416]	= "USB_INT_EN_REG",
[0x1418]	= "FRAME_NUM1_REG",
[0x141A]	= "FRAME_NUM2_REG",
[0x141C]	= "INDEX_REG",
[0x1440]	= "EP0_FIFO_REG",
[0x1442]	= "EP1_FIFO_REG",
[0x1444]	= "EP2_FIFO_REG",
[0x1446]	= "EP3_FIFO_REG",
[0x1448]	= "EP4_FIFO_REG",
[0x1460]	= "EP1_DMA_CON",
[0x1464]	= "EP1_DMA_FIFO",
[0x1466]	= "EP1_DMA_TTC_L",
[0x1468]	= "EP1_DMA_TTC_M",
[0x146A]	= "EP1_DMA_TTC_H",
[0x146C]	= "EP2_DMA_CON",
[0x1470]	= "EP2_DMA_FIFO",
[0x1472]	= "EP2_DMA_TTC_L",
[0x1474]	= "EP2_DMA_TTC_M",
[0x1476]	= "EP2_DMA_TTC_H",
[0x1480]	= "EP3_DMA_CON",
[0x1484]	= "EP3_DMA_FIFO",
[0x1486]	= "EP3_DMA_TTC_L",
[0x1488]	= "EP3_DMA_TTC_M",
[0x148A]	= "EP3_DMA_TTC_H",
[0x148C]	= "EP4_DMA_CON",
[0x1490]	= "EP4_DMA_FIFO",
[0x1492]	= "EP4_DMA_TTC_L",
[0x1494]	= "EP4_DMA_TTC_M",
[0x1496]	= "EP4_DMA_TTC_H",
[0x1420]	= "MAXP_REG",
[0x1426]	= "OUT_MAXP_REG",
[0x1422]	= "EP0_CSR",
[0x1422]	= "IN_CSR1_REG",
[0x1424]	= "IN_CSR2_REG",
[0x1428]	= "OUT_CSR1_REG",
[0x142A]	= "OUT_CSR2_REG",
[0x142C]	= "OUT_FIFO_CNT1_REG",
[0x142E]	= "OUT_FIFO_CNT2_REG",

/* ADC/TP */
[0x4600]	= "TPC_ADCCON",
[0x4604]	= "TPC_ADCDAT",
[0x4640]	= "TPC_CNTL",
[0x4644]	= "TPC_INTR",
[0x4648]	= "TPC_COMP_TP",
[0x464c]	= "TPC_COMP_U1",
[0x4650]	= "TPC_COMP_U2",
[0x4654]	= "TPC_CLK_CNTL",
[0x4658]	= "TPC_CH_SEL",
[0x465c]	= "TPC_TIME_PARM1",
[0x4660]	= "TPC_TIME_PARM2",
[0x4664]	= "TPC_TIME_PARM3",
[0x4668]	= "TPC_X_VALUE",
[0x466c]	= "TPC_Y_VALUE",
[0x4670]	= "TPC_AZ_VALUE",
[0x4674]	= "TPC_U1_VALUE",
[0x4678]	= "TPC_U2_VALUE",

/* Dual CPU Interface: mmsp20_type.h */
[0x3B40]	= "DINT920",
[0x3B42]	= "DINT940",
[0x3B44]	= "DPEND920",
[0x3B46]	= "DPEND940",
[0x3B48]	= "DCTRL940",

/* FDC: mmsp20_type.h */
[0x1838]	= "DFDC_CNTL",
[0x183A]	= "DFDC_FRAME_SIZE",
[0x183C]	= "DFDC_LUMA_OFFSET",
[0x183E]	= "DFDC_CB_OFFSET",
[0x1840]	= "DFDC_CR_OFFSET",
[0x1842]	= "DFDC_DST_BASE_L",
[0x1844]	= "DFDC_DST_BASE_H",
[0x1846]	= "DFDC_STATUS",
[0x1848]	= "DFDC_DERING",
[0x184A]	= "DFDC_OCC_CNTL",

/*
 * Chapter 11
 * General Purpose I/O (GPIO)
 */
[0x1020]	= "GPIOAALTFNLOW",
[0x1022]	= "GPIOBALTFNLOW",
[0x1024]	= "GPIOCALTFNLOW",
[0x1026]	= "GPIODALTFNLOW",
[0x1028]	= "GPIOEALTFNLOW",
[0x102a]	= "GPIOFALTFNLOW",
[0x102c]	= "GPIOGALTFNLOW",
[0x102e]	= "GPIOHALTFNLOW",
[0x1030]	= "GPIOIALTFNLOW",
[0x1032]	= "GPIOJALTFNLOW",
[0x1034]	= "GPIOKALTFNLOW",
[0x1036]	= "GPIOLALTFNLOW",
[0x1038]	= "GPIOMALTFNLOW",
[0x103a]	= "GPIONALTFNLOW",
[0x103c]	= "GPIOOALTFNLOW",

[0x1040]	= "GPIOAALTFNHI",
[0x1042]	= "GPIOBALTFNHI",
[0x1044]	= "GPIOCALTFNHI",
[0x1046]	= "GPIODALTFNHI",
[0x1048]	= "GPIOEALTFNHI",
[0x104a]	= "GPIOFALTFNHI",
[0x104c]	= "GPIOGALTFNHI",
[0x104e]	= "GPIOHALTFNHI",
[0x1050]	= "GPIOIALTFNHI",
[0x1052]	= "GPIOJALTFNHI",
[0x1054]	= "GPIOKALTFNHI",
[0x1056]	= "GPIOLALTFNHI",
[0x1058]	= "GPIOMALTFNHI",
[0x105a]	= "GPIONALTFNHI",
[0x105c]	= "GPIOOALTFNHI",

[0x1180]	= "GPIOAPINLVL",
[0x1182]	= "GPIOBPINLVL",
[0x1184]	= "GPIOCPINLVL",
[0x1186]	= "GPIODPINLVL",
[0x1188]	= "GPIOEPINLVL",
[0x118a]	= "GPIOFPINLVL",
[0x118c]	= "GPIOGPINLVL",
[0x118e]	= "GPIOHPINLVL",
[0x1190]	= "GPIOIPINLVL",
[0x1192]	= "GPIOJPINLVL",
[0x1194]	= "GPIOKPINLVL",
[0x1196]	= "GPIOLPINLVL",
[0x1198]	= "GPIOMPINLVL",
[0x119a]	= "GPIONPINLVL",
[0x119c]	= "GPIOOPINLVL",

/* DPC */
[0x2800]	= "DPC_CNTL",
[0x2802]	= "DPC_FPICNTL",
[0x2804]	= "DPC_FPIPOL1",
[0x2806]	= "DPC_FPIPOL2",
[0x280a]	= "DPC_FPIATV1",
[0x280c]	= "DPC_FPIATV2",
[0x280e]	= "DPC_FPIATV3",
[0x2816]	= "DPC_X_MAX",
[0x2818]	= "DPC_Y_MAX",
[0x281a]	= "DPC_HS_WIDTH",
[0x281c]	= "DPC_HS_STR",
[0x281e]	= "DPC_HS_END",
[0x2820]	= "DPC_V_SYNC",
[0x2822]	= "DPC_V_END",
[0x2826]	= "DPC_DE",
[0x2828]	= "DPC_PS",
[0x282a]	= "DPC_FG",
[0x282c]	= "DPC_LP",
[0x2830]	= "DPC_CLKV2",
[0x2832]	= "DPC_POL",
[0x2834]	= "DPC_CISSYNC",
[0x283a]	= "DPC_Y_BLANK",
[0x283c]	= "DPC_C_BLANK",
[0x283e]	= "DPC_YP_CSYNC",
[0x2840]	= "DPC_YN_CSYNC",
[0x2842]	= "DPC_CP_CSYNC",
[0x2844]	= "DPC_CN_CSYNC",
[0x2846]	= "DPC_INTR",
[0x2848]	= "DPC_CLKCNTL",

/* MLC */
[0X2884]	= "MLC_YUV_CNTL",
[0X2886]	= "MLC_YUVA_TP_HSC",
[0X2888]	= "MLC_YUVA_BT_HSC",
[0X2898]	= "MLC_VLA_ENDX",
[0X288a]	= "MLC_VLA_TP_VSCL",
[0X288c]	= "MLC_VLA_TP_VSCH",
[0X2892]	= "MLC_YUVA_TP_PXW",
[0X2894]	= "MLC_YUVA_BT_PXW",
[0X28da]	= "MLC_STL_CNTL",
[0X28dc]	= "MLC_STL_MIXMUX",
[0X28de]	= "MLC_STL_ALPHAL",
[0X28e0]	= "MLC_STL_ALPHAH",
[0X28e2]	= "MLC_STL1_STX",
[0X28e4]	= "MLC_STL1_ENDX",
[0X28e6]	= "MLC_STL1_STY",
[0X28e8]	= "MLC_STL1_ENDY",
[0X28ea]	= "MLC_STL2_STX",
[0X28ec]	= "MLC_STL2_ENDX",
[0X28ee]	= "MLC_STL2_STY",
[0X28f0]	= "MLC_STL2_ENDY",
[0X28f2]	= "MLC_STL3_STX",
[0X28f4]	= "MLC_STL3_ENDX",
[0X28f6]	= "MLC_STL3_STY",
[0X28f8]	= "MLC_STL3_ENDY",
[0X28fa]	= "MLC_STL4_STX",
[0X28fc]	= "MLC_STL4_ENDX",
[0X28fe]	= "MLC_STL4_STY",
[0X2900]	= "MLC_STL4_ENDY",
[0X2902]	= "MLC_STL_CKEY_GB",
[0X2904]	= "MLC_STL_CKEY_R",
[0X2906]	= "MLC_STL_HSC",
[0X2908]	= "MLC_STL_VSCL",
[0X290a]	= "MLC_STL_VSCH",
[0X290c]	= "MLC_STL_HW",
[0X290e]	= "MLC_STL_OADRL",
[0X2910]	= "MLC_STL_OADRH",
[0X2912]	= "MLC_STL_EADRL",
[0X2914]	= "MLC_STL_EADRH",
[0X291e]	= "MLC_HWC_CNTL",
[0X2920]	= "MLC_HWC_STX",
[0X2922]	= "MLC_HWC_STY",
[0X2924]	= "MLC_HWC_FGR",
[0X2926]	= "MLC_HWC_FB",
[0X2928]	= "MLC_HWC_BGR",
[0X292a]	= "MLC_HWC_BB",
[0X292c]	= "MLC_HWC_OADRL",
[0X292e]	= "MLC_HWC_OADRH",
[0X2930]	= "MLC_HWC_EADRL",
[0X2932]	= "MLC_HWC_EADRH",
[0X2958]	= "MLC_STL_PALLT_A",
[0X295a]	= "MLC_STL_PALLT_D",

};

#endif /* _MMSP2_H */
