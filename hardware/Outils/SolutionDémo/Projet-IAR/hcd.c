/*******************************************************************************
    hcd.c
    Copyright(C) 2003, Oki Electric Industry Co.,Ltd.
      All rights reserved.

    Mar.31,2003 rev1.00
*******************************************************************************/

#include <intrinsics.h>
#include <string.h>
#include <stdio.h>
#include "ML674061.h"
#include "common.h"
#include "hcd.h"
#include "mem.h"
#include "hcdmem.h"
#include "debug.h"
#include "delay.h"

#define RevisonNumber	0x00000010
#define TD_BUFFER_MAX	4096

struct hcd_info hcd_info;

void usbhost_interrupt(void);

int hcd_init(void)
{
	ulong rev;
	ulong fminterval;
	ulong mask;

#ifdef DEBUG_USB
	sprintf(dbg_buffer,"HCD: Controller Address = %08lX\r\n", (ulong)HostCtl_addr);
	DBG_USB(dbg_buffer);
#endif

	//Check chip revision
	rev = get_wvalue(HcRevision);
	if(rev != RevisonNumber){
#ifdef DEBUG_USB
		sprintf(dbg_buffer,"Bad Revision Register : %08lX\r\n", (ulong)rev);
		DBG_USB(dbg_buffer);
#endif
		return -1;
	}

	//Embedded RAM address setup register
	put_wvalue(RamAdr, ComRAMAddr);

	//Set default address for allocation in RAM of USB chip
	hcd_malloc_init(ComRAMAddr, ComRAMSize, 16, COMRAM);

	//Init hcd_info
	memset(&hcd_info, 0, sizeof(struct hcd_info));

	//Set RAM for HCCA and send address to USB chip
	hcd_info.hcca = (struct hcca *)hcd_malloc(sizeof(struct hcca), COMRAM,2);
	if (!hcd_info.hcca){
		return -1;
	}
	memset((long *)hcd_info.hcca, 0, sizeof(struct hcca));
	put_wvalue(HcHCCA, (ulong)hcd_info.hcca);

	//Set addres of the control ED
	put_wvalue(HcControlHeadED, 0);
	//Set addres of the bulk ED
	put_wvalue(HcBulkHeadED, 0);

	//Set SOF frame interval
	fminterval = 0x2edf;
	put_wvalue(HcPeriodicStart,(fminterval * 9) / 10);
	fminterval |= ((((fminterval - MAXIMUM_OVERHEAD) * 6) / 7) << 16);
	put_wvalue(HcFmInterval, fminterval);
	put_wvalue(HcLSThreshold, 0x628);

	INIT_LIST_ENTRY(&hcd_info.ed_control);
	INIT_LIST_ENTRY(&hcd_info.ed_bulk);
	INIT_LIST_ENTRY(&hcd_info.ed_pause0);
	INIT_LIST_ENTRY(&hcd_info.ed_pause1);

	hcd_info.hc_control = OHCI_CTRL_PLE | OHCI_USB_OPER;
	put_wvalue(HcControl, hcd_info.hc_control);

	//Config interrupt Enable
	mask = OHCI_INTR_MIE | OHCI_INTR_UE | OHCI_INTR_WDH | OHCI_INTR_RHSC | OHCI_INTR_SO;
	put_wvalue(HcInterruptEnable, mask);
	put_wvalue(HostCtl, 0x0);
	//here comes the first interrupt ! after initialisation

	put_wvalue(HcRhStatus, RH_HS_LPSC);

	return 0;
}

int hcd_rh_events(void)
{
	if(hcd_info.rh_status & HcdRH_DISCONNECT){
		hcd_info.rh_status &= ~HcdRH_DISCONNECT;

		DBG_USB("HCD: disconnected the device.\r\n");
     		usbh_disconnect((PDEVINFO *)&hcd_info.rh_device);
        hcd_info.rh_device = NULL;

		return HcdRH_DISCONNECT;
	}

	else if(hcd_info.rh_status & HcdRH_CONNECT) {
	        DelayMs(40);
		hcd_info.rh_status &= ~HcdRH_CONNECT;

		DBG_USB(" HCD: connected a new device.\r\n");
		hcd_info.rh_device = usbh_connect((unsigned char)((hcd_info.rh_status & HcdRH_SPEED) ? 1 : 0));

		return HcdRH_CONNECT;
	}

	return 0;
}

#ifdef DEBUG_USB
static char *hcd_cc_string(ulong cc)
{
	char *cc_s;

	switch(cc){
        case CC_NOERROR        : cc_s = "NOERROR";				break;
        case CC_CRC            : cc_s = "CRC";					break;
        case CC_BITSTUFFING    : cc_s = "BITSTUFFING";			break;
        case CC_TOGGLEMISMATCH : cc_s = "DATATOGGLEMISMATCH ";	break;
        case CC_STALL          : cc_s = "STALL"; 				break;
        case CC_NOTRESPONDING  : cc_s = "DEVICENOTRESPONDING";	break;
        case CC_PIDCHECKFAILURE: cc_s = "PIDCHECKFAILURE";		break;
        case CC_UNEXPECTEDPID  : cc_s = "UNEXPECTEDPID";		break;
        case CC_DATAOVERRUN    : cc_s = "DATAOVERRUN";			break;
        case CC_DATAUNDERRUN   : cc_s = "DATAUNDERRUN";			break;
        case CC_BUFFEROVERRUN  : cc_s = "BUFFEROVERRUN";		break;
        case CC_BUFFERUNDERRUN : cc_s = "BUFFERUNDERRUN ";		break;
        case CC_NOTACCESSED    : cc_s = "NOTACCESSED";			break;
        default                : cc_s = "UNKNOWN CODE";         break;
	}

	return cc_s;
}
#endif

/* Must be called with OHCI IRQ masked */
static void hcd_delete_td(PHCD_ED ed)
{
	PURB urb;
	PHCD_TD td_next, td_prev;
	ulong HeadP = ed->HcED.HeadP;
	ulong TailP = ed->HcED.TailP;

	td_next = (PHCD_TD)(HeadP & 0xFFFFFFF0);
	while((ulong)td_next != TailP){
	 	urb = td_next->urb;
		if(urb){
			list_del(&td_next->list);
			if(urb->status>=0) {
				DBG("ABORT 1\r\n");
				urb->status = URB_ABORT;
			}
		}
		td_prev	= td_next;
		td_next = (PHCD_TD)td_prev->HcTD.NextTD;
		hcd_free(td_prev);
	}

	ed->HcED.HeadP = (HeadP & 0xF) | TailP;
}

/* Must be called with OHCI IRQ masked */
static int hcd_add_td(PHCD_ED ed, ulong control, void *buffer, int length,
	 PURB urb, int index)
{
	PHCD_TD td_tail;
	PHCD_TD td_next;

	td_tail = (PHCD_TD)ed->HcED.TailP;
	if(!td_tail){
		return -1;
	}

	td_next = (PHCD_TD)(td_tail->HcTD.NextTD);
	while(td_next){
		td_tail = td_next;
		td_next = (PHCD_TD)(td_tail ->HcTD.NextTD);
	}

	td_next = (PHCD_TD)hcd_malloc(sizeof(HCD_TD), COMRAM,3);	/* new TD */
	if (!td_next){
		return -1;
	}
	memset((long *)td_next, 0, sizeof(HCD_TD));

	td_tail->HcTD.Control = control;
	td_tail->HcTD.CBP = (!buffer || !length) ? 0: buffer;
	td_tail->HcTD.NextTD = (ulong)td_next;
	td_tail->HcTD.BE = (!buffer || !length ) ? 0: (char *)buffer + length - 1;
	td_tail->urb = urb;
	td_tail->index = index;

	ed->HcED.TailP = (ulong)td_next;

	list_add(&td_tail->list, &urb->td_list);

	return 0;
}

static void hcd_unlink_ed(PHCD_ED ed)
{
	PHCD_ED ed_pre;

	if(ed->type == USB_CTRL){

		if(ed->ed_list.Blink == &hcd_info.ed_control){
			put_wvalue(HcControlHeadED, ed->HcED.NextED);
		}
		else{
			ed_pre = list_struct_entry(ed->ed_list.Blink, HCD_ED, ed_list);
			ed_pre->HcED.NextED = ed->HcED.NextED;
		}

		list_del(&ed->ed_list);

		if(list_empty(&hcd_info.ed_bulk)){
			hcd_info.hc_control &= ~OHCI_CTRL_CLE;
			put_wvalue(HcControl, hcd_info.hc_control);
		}


	}
	else if(ed->type == USB_BULK){

		if(ed->ed_list.Blink == &hcd_info.ed_bulk){
			put_wvalue(HcBulkHeadED, ed->HcED.NextED);
		}
		else{
			ed_pre = list_struct_entry(ed->ed_list.Blink, HCD_ED, ed_list);
			ed_pre->HcED.NextED = ed->HcED.NextED;
		}

		list_del(&ed->ed_list);

		if(list_empty(&hcd_info.ed_bulk)){
			hcd_info.hc_control &= ~OHCI_CTRL_BLE;
			put_wvalue(HcControl, hcd_info.hc_control);
		}
	}

	ed->HcED.NextED = NULL;
	ed->flag = ED_UNLINK;
}

static void hcd_pause_ed(PHCD_ED ed)
{
	int TailP;

	if(ed->HcED.Control & HcED_SKIP) {
		TailP = ed->HcED.TailP;
		if((ed->HcED.HeadP & 0xFFFFFFF0) != TailP)
			hcd_delete_td(ed);

		hcd_unlink_ed(ed);
	} else {
	  		TailP = ed->HcED.TailP;
			if((ed->HcED.HeadP & 0xFFFFFFF0) == TailP) {
				ed->HcED.Control |= HcED_SKIP;

				hcd_unlink_ed(ed);
			} else {
				ed->flag = ED_PAUSE;

				ed->HcED.Control |= HcED_SKIP;

				list_add(&ed->ed_list, &hcd_info.ed_pause0);

				put_wvalue(HcInterruptEnable, OHCI_INTR_SF);
			}
	}
}

/* Must be called with OHCI IRQ masked */
void hcd_delete_ed(PHCD_ED ed)
{
	int timeout = 0;

#ifdef DEBUG_USB
	DBG_USB(" hcd_delete_ed:\n");
#endif

	if(ed->flag == ED_LINK)
		hcd_pause_ed(ed);

	while(ed->flag == ED_LINK || ed->flag == ED_PAUSE){
		DelayMs(1);
		if(timeout++ > 100){
			DBG_USB(" HCD: hcd_delete_ed timeout!!\n");
			return;
		}
	}

	hcd_free((long *)ed->HcED.TailP);
	hcd_free(ed);
}

int hcd_update_ed(PHCD_ED ed, uchar dev_addr, ushort maxpacket)
{
	ulong control;

#ifdef DEBUG_USB
	DBG_USB(" hcd_update_ed:\r\n");
#endif

	if(ed->HcED.Control & HcED_SKIP){
		control = ed->HcED.Control & (HcED_FA | HcED_EN | HcED_SPEED | HcED_SKIP);
		ed->HcED.Control = control | ((ulong)maxpacket << 16) | dev_addr;
		return 0;
	}

	return -1;
}

/* Must be called with OHCI IRQ masked */
PHCD_ED hcd_create_ed(uchar speed, uchar dev_addr, uchar type, uchar ep_num,
	ushort maxpacket, uchar interval)
{
	PHCD_ED ed;
	PHCD_TD td;
	ulong dir;

#ifdef DEBUG_USB
	DBG_USB(" hcd_create_ed:\r\n");
#endif

	ed = (PHCD_ED)hcd_malloc(sizeof(HCD_ED), COMRAM,4);
	if (!ed){
		return NULL;
	}

	td = (PHCD_TD)hcd_malloc(sizeof(HCD_TD), COMRAM,5);	/* dumy TD */
	if (!td){
		hcd_free(ed);
		return NULL;
	}
	memset((long *)td, 0, sizeof(HCD_TD));

	dir = (type == USB_CTRL) ? (ulong)0:
		((ep_num & USB_DIR_IN) ? (ulong)HcED_DIR_IN : (ulong)HcED_DIR_OUT);
	ed->HcED.Control = ((ulong)maxpacket << 16) | ((ulong)speed << 13 ) | ((ulong)ep_num << 7)
					 | dir | ((ulong)dev_addr & 0x7Fl) | HcED_SKIP;
	ed->HcED.TailP = (ulong)td;
	ed->HcED.HeadP = (ulong)td;
	ed->HcED.NextED = NULL;
	ed->type = type;
	ed->interval = interval;
	ed->flag = ED_IDLE;

	return ed;
}

static void hcd_transfer_wait(PURB urb)
{
	int timeout = 0;

#ifdef DEBUG_USB
	DBG_USB(" hcd_transfer_wait:\r\n");
#endif

	while(urb->status == URB_PENDING) {
		DelayMs(1);
		if(timeout++ > urb->timeout){
			PHCD_ED ed = (PHCD_ED)urb->ed;
			DBG_USB(" HCD: hcd_transfer_wait timeout!!\r\n");
			hcd_pause_ed(ed);
			urb->status = URB_TIMEOUT;
			return;
		}
	}

	if(!list_empty(&urb->td_list))
		urb->status = URB_SYSERR;
#ifdef DEBUG_USB
#ifdef DEBUG_RAMAC_ERR
    	hcd_info.hc_control |= OHCI_CTRL_PLE;
	    put_wvalue(HcControl, hcd_info.hc_control);
#endif
	DBG_USB(" hcd_transfer_wait: done.\r\n");
#endif

	return;
}

static void hcd_control_transfer_start(PURB urb)
{
	ulong control;
	PHCD_ED ed = (PHCD_ED)urb->ed;
	PHCD_ED ed_list_tail;
	int ret;

#ifdef DEBUG_USB
	DBG_USB(" hcd_control_transfer_start:\r\n");
#endif

	urb->result = 0;
	urb->status = URB_PENDING;
	INIT_LIST_ENTRY(&urb->td_list);

	disable_ohci_irq();

	control = HcTD_CC | HcTD_DP_SETUP | HcTD_T_DATA0 | HcTD_DI;
	ret = hcd_add_td(ed, control, urb->setup, 8, urb, TD_SETUP);
	if(ret<0){
		urb->status = URB_ADDTDERR;
		return;
	}

	if (urb->length> 0) {
		if(urb->setup->bmRequestType & USB_DIR_IN)
			control = HcTD_CC | HcTD_R | HcTD_DP_IN  | HcTD_T_DATA1 | HcTD_DI;
		else
			control = HcTD_CC | HcTD_R | HcTD_DP_OUT | HcTD_T_DATA1 | HcTD_DI;
		ret = hcd_add_td(ed, control, urb->buffer, (int)urb->length, urb, TD_DATA);
		if(ret<0){
			urb->status = URB_ADDTDERR;
			return;
		}
	}

	if(urb->setup->bmRequestType & USB_DIR_IN)
		control = HcTD_CC | HcTD_DP_OUT | HcTD_T_DATA1;
	else
		control = HcTD_CC | HcTD_DP_IN  | HcTD_T_DATA1;
	ret = hcd_add_td(ed, control, NULL, 0, urb, TD_STATUS);
	if(ret<0){
		urb->status = URB_ADDTDERR;
		return;
	}

	if(ed->flag == ED_IDLE || ed->flag == ED_UNLINK){
		if(list_empty(&hcd_info.ed_control)){
			put_wvalue(HcControlHeadED, (ulong)ed);
		}
		else{
			ed_list_tail = (PHCD_ED)get_wvalue(HcControlHeadED);
			while(ed_list_tail->HcED.NextED){
				ed_list_tail = (PHCD_ED)ed_list_tail->HcED.NextED;
			}
			ed_list_tail->HcED.NextED = (ulong)ed;
		}
		ed->HcED.NextED = NULL;
        ed->flag = ED_LINK;

		list_add(&ed->ed_list, &hcd_info.ed_control);

	}

	if(ed->HcED.HeadP & HcED_HeadP_HALT){
		ed->HcED.HeadP &= (ulong)(~HcED_HeadP_HALT);
	}

	if(ed->HcED.Control & HcED_SKIP){
		ed->HcED.Control &= (ulong)(~HcED_SKIP);
	}

	if( (hcd_info.hc_control & OHCI_CTRL_CLE) == 0 ){
		hcd_info.hc_control |= OHCI_CTRL_CLE;
		put_wvalue(HcControl, hcd_info.hc_control);
	}

	enable_ohci_irq();

	put_wvalue(HcCommandStatus, OHCI_CLF);

	return;
}

static void hcd_control_transfer(PURB urb)
{
	struct usb_setup *setup, *setup_saved;
	void *buffer, *buffer_saved;

	if(hcd_malloc_check(urb->setup) != COMRAM) {
		disable_ohci_irq();
		setup = (struct usb_setup *)hcd_malloc(8, COMRAM,6);
		enable_ohci_irq();
		if(!setup){
			urb->status = URB_NOMEM;
			return;
		}
		memcpy((void *)setup, (void *)(urb->setup), 8);
		setup_saved = urb->setup;
		urb->setup = setup;
	} else {
		setup = NULL;
        	setup_saved = NULL;
	}

	if(urb->setup->wLength > 0 && (hcd_malloc_check(urb->buffer) != COMRAM)) {
		disable_ohci_irq();
		buffer = hcd_malloc(urb->length, COMRAM,7);
		enable_ohci_irq();
		if (!buffer){
			urb->status = URB_NOMEM;
			return;
		}
		buffer_saved = urb->buffer;
		if((urb->setup->bmRequestType & USB_DIR_IN) == 0 )
			memcpy((long *)buffer, (long *)buffer_saved, urb->length);
		urb->buffer = buffer;
	} else {
		buffer = NULL;
        	buffer_saved = NULL;
	}

	hcd_control_transfer_start(urb);

	if(urb->timeout || setup || buffer)
		hcd_transfer_wait(urb);

	if(setup) {
		urb->setup = setup_saved;
		disable_ohci_irq();
		hcd_free(setup);
		enable_ohci_irq();
	}

	if(buffer){
		urb->buffer = buffer_saved;
		if(urb->setup->bmRequestType & USB_DIR_IN)
			memcpy((long *)buffer_saved, (long *)buffer, urb->length);
		disable_ohci_irq();
		hcd_free(buffer);
		enable_ohci_irq();
	}

	return;
}

static void hcd_bulk_transfer_start(PURB urb)
{
	PHCD_ED ed = (PHCD_ED)urb->ed;
	PHCD_ED ed_list_tail;
	char *buffer = (char *)urb->buffer;
	ulong length = urb->length;
	uchar cnt = 0;
	int ret;

	DBG_USB(" hcd_bulk_transfer_start:\r\n");

	urb->result = 0;
	urb->status = URB_PENDING;
	INIT_LIST_ENTRY(&urb->td_list);

	disable_ohci_irq();

	while(length > TD_BUFFER_MAX) {
		ret = hcd_add_td(ed, HcTD_CC | HcTD_DI, buffer, TD_BUFFER_MAX, urb, cnt);
		if(ret < 0){
			urb->status = URB_ADDTDERR;
			return;
		}
		buffer += TD_BUFFER_MAX;
		length -= TD_BUFFER_MAX;
		cnt++;
	}

	ret = hcd_add_td(ed, HcTD_CC, buffer, (int)length, urb, cnt);
	if(ret<0){
		urb->status = URB_ADDTDERR;
		return;
	}

	if(ed->flag == ED_IDLE || ed->flag == ED_UNLINK){
		if(list_empty(&hcd_info.ed_bulk)){
			put_wvalue(HcBulkHeadED, (ulong)ed);
		} else {
			ed_list_tail = (PHCD_ED)get_wvalue(HcBulkHeadED);
			while(ed_list_tail->HcED.NextED){
				ed_list_tail = (PHCD_ED)ed_list_tail->HcED.NextED;
			}
			ed_list_tail->HcED.NextED = (ulong)ed;
		}
		ed->HcED.NextED = NULL;
        	ed->flag = ED_LINK;

		list_add(&ed->ed_list, &hcd_info.ed_bulk);
	}

	if(ed->HcED.HeadP & HcED_HeadP_HALT){
		ed->HcED.HeadP &= (ulong)(~HcED_HeadP_HALT);
	}

	if(ed->HcED.Control & HcED_SKIP){
		ed->HcED.Control &= (ulong)(~HcED_SKIP);
	}

	if((hcd_info.hc_control & OHCI_CTRL_BLE) == 0) {
		hcd_info.hc_control |= OHCI_CTRL_BLE;
		put_wvalue(HcControl, hcd_info.hc_control);
	}

	enable_ohci_irq();

	put_wvalue(HcCommandStatus, OHCI_BLF);
}

void hcd_bulk_transfer(PURB urb)
{
	if(hcd_malloc_check(urb->buffer) != COMRAM) {
		urb->status = URB_ADDTDERR;
		return;
	} else {
		hcd_bulk_transfer_start(urb);
		if(urb->timeout) hcd_transfer_wait(urb);
	}
}

int hcd_transfer_request(PURB urb)
{
	DBG_USB(" hcd_transfer_request:\r\n");

	if (hcd_info.disabled) {
		DBG("ABORT 2\r\n");
		urb->status = URB_ABORT;
		return urb->status;
	}

	switch(((PHCD_ED)urb->ed)->type){
	case USB_CTRL:
#ifdef DEBUG_RAMAC_ERR
    	hcd_info.hc_control &= ~OHCI_CTRL_PLE;
	    put_wvalue(HcControl, hcd_info.hc_control);
#endif
		hcd_control_transfer(urb);
    	break;

	case USB_BULK:
		hcd_bulk_transfer(urb);
		break;

	default:
		DBG("ABORT 3\r\n");
		urb->status = URB_ABORT;
	}

	return urb->status;
}

static PHCD_TD hcd_get_done_list(void)
{
	PHCD_TD td_done, td_next, td_temp;

	td_done = NULL;
	td_next = (PHCD_TD)((hcd_info.hcca->donehead) & 0xfffffff0);
	hcd_info.hcca->donehead = 0;

	while (td_next) {

#ifdef DEBUG_USB
            sprintf(dbg_buffer,"HCD: Done TD = %08lX\r\n", (ulong)td_next);
            DBG_USB(dbg_buffer);
#endif

		if( hcd_malloc_check((long *)td_next) != COMRAM){
			DBG_USB("\r\nHCD: DoneHead is bad address!!\r\n");
			break;
		}

		td_temp = td_done;
		td_done = td_next;
		td_next = (PHCD_TD)(td_next->HcTD.NextTD & 0xFFFFFFF0);
		td_done->HcTD.NextTD = (ulong)td_temp;
	}

	return td_done;
}

static ulong hcd_transfer_count(PHCD_TD td, void *buffer)
{
	ulong result;

	if (td->HcTD.CBP == 0 && td->HcTD.BE != 0){
		result = (ulong)td->HcTD.BE - (ulong)buffer + 1;
	}
	else{
		result = (ulong)td->HcTD.CBP - (ulong)buffer;
	}

	return result;
}

/* Must be called with OHCI IRQ masked */
static PHCD_TD hcd_control_transfer_done(PHCD_TD td)
{
	PURB urb = td->urb;
	PHCD_ED ed = (PHCD_ED)urb->ed;
	PHCD_TD td_next;
	ulong cc;

	if(td->index == TD_DATA){
		urb->result = hcd_transfer_count(td, urb->buffer);
	}
	else if(td->index == TD_STATUS){
		urb->status = (int)urb->result;
		hcd_pause_ed(ed);
	}

	cc = (td->HcTD.Control >> 28);
	if(cc){

#ifdef DEBUG_USB
                sprintf(dbg_buffer,"\r\nHCD: USB-error/status: %02X(%s): %08lX\r\n", cc, hcd_cc_string(cc), (ulong)td);
                DBG_USB(dbg_buffer);
#endif
		urb->result = cc;
		urb->status = URB_XFERERR;
		if( ((ed->HcED.HeadP & HcED_HeadP_HALT) != 0) && ((ed->HcED.HeadP & 0xFFFFFFF0) != 0) ){
			hcd_delete_td((PHCD_ED)ed);
			hcd_pause_ed(ed);
		}
	}

	td_next = (PHCD_TD)td->HcTD.NextTD;
	list_del(&td->list);
	hcd_free(td);

	return td_next;
}

/* Must be called with OHCI IRQ masked */
static PHCD_TD hcd_bulk_transfer_done(PHCD_TD td)
{
	PURB urb = td->urb;
	PHCD_ED ed = (PHCD_ED)urb->ed;
	PHCD_TD td_next;
	ulong cc;

	urb->result = hcd_transfer_count(td, urb->buffer);

	cc = (td->HcTD.Control >> 28);

	if((cc != 0) && (cc != CC_DATAOVERRUN) && (cc != CC_DATAUNDERRUN)) {

#ifdef DEBUG_USB
                sprintf(dbg_buffer,"\r\nHCD: USB-error/status: %02lX: %08lX\r\n", cc, (ulong)HostCtl_addr);
                DBG_USB(dbg_buffer);
#endif

                urb->result = cc;
		urb->status = URB_XFERERR;
		if( ((ed->HcED.HeadP & HcED_HeadP_HALT) != 0) && ((ed->HcED.HeadP & 0xFFFFFFF0) != 0) ){
			hcd_delete_td((PHCD_ED)ed);
			hcd_pause_ed(ed);
		}
	}


	urb->status = (int)urb->result;

	#ifdef BULK_UNLINK
		hcd_pause_ed(ed);
	#endif

	if(urb->callback)
		urb->callback(urb);


	td_next = (PHCD_TD)td->HcTD.NextTD;
	list_del(&td->list);
	hcd_free(td);

	return td_next;
}

static void hcd_writeback_done(void)
{
	PURB	urb;
	PHCD_TD td_list, td_next;
	PHCD_ED ed;

	td_list = hcd_get_done_list();

	while(td_list) {
		urb = td_list->urb;
		ed = (PHCD_ED)urb->ed;
		if(ed->type == USB_CTRL) {
			td_next = hcd_control_transfer_done(td_list);
		} else if(ed->type == USB_BULK) {
			td_next = hcd_bulk_transfer_done(td_list);
		} else {
#ifdef DEBUG_USB
			sprintf(dbg_buffer,"\r\nIllegal Type! %x\r\n", ed->type);
                	DBG_USB(dbg_buffer);
#endif
            		return;
        	}
		td_list = td_next;
	}
}

static void hcd_rh_irq(void)
{
	unsigned long hub_status;
	unsigned long port_status;

	hub_status = get_wvalue(HcRhStatus);

	if(hub_status & RH_HS_OCIC) {
		DBG(" root hub: Overcurrent Indicator Changed.\r\n");
	 	DBG(" root hub: please debug this code.\r\n");
		return;
	} else if(hub_status) {
	 	DBG(" root hub: please debug this code.\r\n");
		return;
	}

	port_status = get_wvalue(HcRhPortStatus);

	if(port_status & RH_PS_CSC) {
	 	DBG_USB(" root hub: Port Connect Status Changed.\r\n");
		put_wvalue(HcRhPortStatus, RH_PS_CSC|RH_PS_PRS);
	}

	if(port_status & RH_PS_PRSC) {
	 	DBG_USB(" root hub: Port Reset Status Changed.\r\n");
		put_wvalue(HcRhPortStatus, RH_PS_PRSC);
		if( ((port_status & RH_PS_PES) != 0) && ((port_status & RH_PS_CCS) != 0) ){
			DBG_USB(" root hub: Port Connect Status = 1\r\n");

#ifdef DEBUG_USB
              sprintf(dbg_buffer," %s speed device connected.\r\n",((port_status & RH_PS_LSDA) ? "low" : "full"));
              DBG_USB(dbg_buffer);
#endif


                        hcd_info.rh_status |= HcdRH_CONNECT;
			if(port_status & RH_PS_LSDA)
				hcd_info.rh_status |= HcdRH_SPEED;
			else
				hcd_info.rh_status &= ~HcdRH_SPEED;
		}
	}

	if(port_status & RH_PS_PESC){
	 	DBG_USB(" root hub: Port Enable Status Changed.\r\n");
		put_wvalue(HcRhPortStatus, RH_PS_PESC);
		if( ((port_status & RH_PS_PES) == 0) || ((port_status & RH_PS_CCS) == 0) ){
			DBG_USB(" root hub: Port Connect Status = 0\r\n");
			hcd_info.rh_status |= HcdRH_DISCONNECT;
		}
	}

	else if(port_status & RH_PS_PSSC){
	 	DBG(" root hub: Port Suspend Status Changed.\r\n");
	 	DBG(" root hub: please debug this code.\r\n");
		/*writel_reg(HcRhPortStatus0, RH_PS_PSSC);*/
	}

	else if(port_status & RH_PS_OCIC){
	 	DBG(" root hub: Port Overcurrent Indicator Status Changed.\r\n");
	 	DBG(" root hub: please debug this code.\r\n");
		put_wvalue(HcRhPortStatus, RH_PS_PRS);
		/*writel_reg(HcRhPortStatus, RH_PS_OCIC);*/
	}
}

void hcd_ed_delete_list(void)
{
	PHCD_ED ed;

	while(!list_empty(&hcd_info.ed_pause1)){
		ed = list_struct_entry(hcd_info.ed_pause1.Flink, HCD_ED, ed_list);
		list_del(&ed->ed_list);
		hcd_pause_ed(ed);
	}

	while(!list_empty(&hcd_info.ed_pause0)){
		ed = list_struct_entry(hcd_info.ed_pause0.Flink, HCD_ED, ed_list);
		list_del(&ed->ed_list);
		list_add(&ed->ed_list, &hcd_info.ed_pause1);
	}

	if(!list_empty(&hcd_info.ed_pause1)){
		put_wvalue(HcInterruptEnable, OHCI_INTR_SF);
	}
}

void usbhost_interrupt(void)
{
	ulong status;
	ulong b;

	status = get_wvalue(SttTrnsCnt);
	if(status & B_DMAIRQ) {
		DBG("USB DMA interrupt\r\n");
		/* DMA not used */
		put_wvalue(SttTrnsCnt, B_DMAIRQ);
	} else if(status & B_OHCIIRQ) {
		if ((hcd_info.hcca->donehead != 0) && ((hcd_info.hcca->donehead & 0x01) == 0) ) {
			status = OHCI_INTR_WDH;
		} else {
			b = get_wvalue(HcInterruptEnable);
			if((status = (get_wvalue(HcInterruptStatus) & b)) == 0) return;
		}

#ifdef DEBUG_USB
		sprintf(dbg_buffer, "HCD: Interrupt %lX frame: %lX\r\n", status, (ushort)hcd_info.hcca->framenumber);
		DBG_USB(dbg_buffer);
#endif

		if(status & OHCI_INTR_UE) {
			hcd_info.disabled++;
			put_wvalue(HcInterruptDisable, 0xFFFFFFFF);
			DBG(" HCD: Unrecoverable Error, controller disabled\r\n");
		}

		if(status & OHCI_INTR_WDH) {
			put_wvalue(HcInterruptDisable, OHCI_INTR_WDH);
			put_wvalue(HcInterruptStatus, OHCI_INTR_WDH);
			DBG_USB(" OHCI_INTR_WDH irq\r\n");
			hcd_writeback_done();
			put_wvalue(HcInterruptEnable, OHCI_INTR_WDH);
		}

		if(status & OHCI_INTR_RHSC) {
			put_wvalue(HcInterruptDisable, OHCI_INTR_RHSC);
			put_wvalue(HcInterruptStatus, OHCI_INTR_RHSC);
			DBG_USB(" OHCI_INTR_RHSC irq\r\n");
			hcd_rh_irq();
			put_wvalue(HcInterruptEnable, OHCI_INTR_RHSC);
		}

		if(status & OHCI_INTR_SO) {
			put_wvalue(HcInterruptStatus, OHCI_INTR_SO);
			DBG_USB(" USB Schedule overrun\r\n");
		}

		if(status & OHCI_INTR_SF) {
			put_wvalue(HcInterruptDisable, OHCI_INTR_SF);
			put_wvalue(HcInterruptStatus, OHCI_INTR_SF);
			DBG_USB(" OHCI_INTR_SF\r\n");
			hcd_ed_delete_list();
		}
		put_wvalue(SttTrnsCnt, B_OHCIIRQ);
	}
}
