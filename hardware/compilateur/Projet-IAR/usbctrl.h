/*******************************************************************************
    usbctrl.h

    Copyright(C) 2003, Oki Electric Industry Co.,Ltd.
      All rights reserved.

    Mar.31,2003 rev.1.00
*******************************************************************************/

#ifndef _USBCTRL_H_
#define _USBCTRL_H_

#define STATE_IDLE  		1
#define STATE_PERI_ACTV		2
#define STATE_HOST_ACTV		3
#define STATE_OTG_SRP		4

#define USB_SE0             0
#define USB_K_STATE         1
#define USB_J_STATE         2
#define USB_SE1             3

int usbctrl_init(int);

void usbctrl_peri_driver_set(int (*initialize)(void), void (*interrupt)(void));
void usbctrl_host_driver_set(int (*initialize)(void), void (*interrupt)(void));

int usbctrl_mode_set(int);
  #define USB_PERIPHERAL	0
  #define USB_HOST		1
  #define USB_OTG           	2
  #define USB_IDLE		3
  #define USB_OTG_SRP       	4

void usbctrl_vbus_thress(int mode);
  #define VBUS_RISE         0x00
  #define VBUS_FALL         0x01
  #define VBUS_SESS         0x02
  #define VBUS_NC           0x03

void usbctrl_vbus_set(int mode);
  #define VBUS_ON		    0x00
  #define VBUS_OFF		    0x01
  #define VBUS_CHARGE	    0x02
  #define VBUS_DISCHARGE    0x03
  #define VBUS_HOSTSET      0x04

  #define V_ON              (F_VBUSMODE_DRIVE | B_DRVVBUS)	/*0x06*/
  #define V_OFF             (F_VBUSMODE_DRIVE)			/*0x02*/
  #define V_CHRG            (F_VBUSMODE_PULSE | B_CHRGVBUS)	/*0x0B*/
  #define V_DISCHRG         (F_VBUSMODE_PULSE | B_DISCHRGVBUS)	/*0x13*/
  #define V_HOST            (F_VBUSMODE_ROOTHUB)		/*0x00*/

void usbctrl_resistance_set(int);
  #define HIGH_Z            0
  #define PULLUP            1
  #define PULLDOWN          2

void usbctrl_se0_det_int(int);
  #define ENABLE	1
  #define DISABLE	0

int usbctrl_id_check(void);

int usbctrl_vbus_status(void);

void usbctrl_interrupt(void);

extern ulong usbctrl_state;

extern void usbhost_interrupt(void);

#endif /* _USBCTRL_H_ */
