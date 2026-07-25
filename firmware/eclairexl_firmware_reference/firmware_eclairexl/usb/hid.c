//#include <stdio.h>

#include "usb.h"
#include "timer.h"
#include "hidparser.h"
#include "events.h"
#include "debug.h"
#include "usbhostslave.h"
#include "log.h"

/*
#include "printf.h"
extern unsigned char volatile * baseaddr;
extern int debug_pos;
*/

unsigned char kbd_led_state;  // default: all leds off
unsigned char joysticks;      // number of detected usb joysticks

//#define hid_debugf printf

uint16_t bs16(uint8_t * in)
{
	uint16_t low = *in;
	uint16_t high = *(in+1);
	uint16_t res = (high<<8) | low;
	return res;
}

uint8_t hid_get_joysticks(void) {
  return joysticks;
}

//get HID report descriptor 
static uint8_t hid_get_report_descr(usb_device_t *dev, uint8_t i, uint16_t size)  {
  //  hid_debugf("%s(%x, if=%d, size=%d)", __FUNCTION__, dev->bAddress, iface, size);

  uint8_t buf[size];
  usb_hid_info_t *info = &(dev->hid_info);
  uint8_t rcode = usb_ctrl_req( dev, HID_REQ_HIDREPORT, USB_REQUEST_GET_DESCRIPTOR, 0x00, 
			      HID_DESCRIPTOR_REPORT, info->iface[i].iface_idx, size, buf);
 
  if(!rcode) {
    hid_debugf("HID report desc:");
    //MWW hexdump(buf, size, 0);

    // we got a report descriptor. Try to parse it
    if(parse_report_descriptor(buf, size, &(info->iface[i].conf))) {
      if(info->iface[i].conf.type == CONFIG_TYPE_JOYSTICK) {
	//unsigned char * temp = baseaddr;
	//baseaddr = (unsigned char volatile *)(40000 + atari_regbase);
	//debug_pos=160;
	//printf("Detected USB joystick #%d", joysticks);
	hid_debugf("stick #%d", joysticks);

	info->iface[i].device_type = HID_DEVICE_JOYSTICK;
	info->iface[i].jindex = joysticks++;
	//debug_pos=240;
	//printf("assigned index #%d", info->iface[i].jindex);
	//baseaddr = temp;
      }
    }
  }
    
  return rcode;
}

static uint8_t hid_set_idle(usb_device_t *dev, uint8_t iface, uint8_t reportID, uint8_t duration ) {
  //  hid_debugf("%s(%x, if=%d id=%d, dur=%d)", __FUNCTION__, dev->bAddress, iface, reportID, duration);

  return( usb_ctrl_req( dev, HID_REQ_HIDOUT, HID_REQUEST_SET_IDLE, reportID, 
		       duration, iface, 0x0000, NULL));
}

static uint8_t hid_set_protocol(usb_device_t *dev, uint8_t iface, uint8_t protocol) {
  //  hid_debugf("%s(%x, if=%d proto=%d)", __FUNCTION__, dev->bAddress, iface, protocol);

  return( usb_ctrl_req( dev, HID_REQ_HIDOUT, HID_REQUEST_SET_PROTOCOL, protocol, 
		       0x00, iface, 0x0000, NULL));
}

static uint8_t hid_set_report(usb_device_t *dev, uint8_t iface, uint8_t report_type, uint8_t report_id, 
			      uint16_t nbytes, uint8_t* dataptr ) {
  //  hid_debugf("%s(%x, if=%d data=%x)", __FUNCTION__, dev->bAddress, iface, dataptr[0]);

  return( usb_ctrl_req(dev, HID_REQ_HIDOUT, HID_REQUEST_SET_REPORT, report_id, 
		       report_type, iface, nbytes, dataptr));
}

/* todo: handle parsing in chunks */
static uint8_t usb_hid_parse_conf(usb_device_t *dev, uint8_t conf, uint16_t len) {
  usb_hid_info_t *info = &(dev->hid_info);
  uint8_t rcode;
  bool isGoodInterface = false;

  union buf_u {
    usb_configuration_descriptor_t conf_desc;
    usb_interface_descriptor_t iface_desc;
    usb_endpoint_descriptor_t ep_desc;
    usb_hid_descriptor_t hid_desc;
    uint8_t raw[len];
  } buf, *p;

  // usb_interface_descriptor

  if(rcode = usb_get_conf_descr(dev, len, conf, &buf.conf_desc)) 
    return rcode;

  /* scan through all descriptors */
  p = &buf;
  //LOG("LenX:%d\n", p->conf_desc.bLength);
  while(len > 0) {
    hid_debugf("L%02d %02d %02d ",len, p->conf_desc.bLength, p->conf_desc.bDescriptorType);

    switch(p->conf_desc.bDescriptorType) {
    case USB_DESCRIPTOR_CONFIGURATION:
      hid_debugf("conf descriptor size %d", p->conf_desc.bLength);
      // we already had this, so we simply ignore it
      break;

    case USB_DESCRIPTOR_INTERFACE:
      isGoodInterface = false;
      hid_debugf("iface descriptor size %d class %d", p->iface_desc.bLength, p->iface_desc.bInterfaceClass);

      /* check the interface descriptors for supported class */

      // only HID interfaces are supported
      if(p->iface_desc.bInterfaceClass == USB_CLASS_HID) {
	hid_debugf("iface is HID");
	//	puts("iface is HID");

	if(info->bNumIfaces < MAX_IFACES) {
	  // ok, let's use this interface
	  isGoodInterface = true;

	  info->iface[info->bNumIfaces].iface_idx = p->iface_desc.bInterfaceNumber;
	  info->iface[info->bNumIfaces].has_boot_mode = false;
	  info->iface[info->bNumIfaces].is_5200daptor = false;
	  info->iface[info->bNumIfaces].is_MCC = 0;
	  info->iface[info->bNumIfaces].key_state = 0;
	  info->iface[info->bNumIfaces].device_type = HID_DEVICE_UNKNOWN;
	  info->iface[info->bNumIfaces].conf.type = CONFIG_TYPE_NONE;

	  if(p->iface_desc.bInterfaceSubClass == HID_BOOT_INTF_SUBCLASS) {
	    hid_debugf("Iface %d is Boot sub class", info->bNumIfaces);
	    info->iface[info->bNumIfaces].has_boot_mode = true;
	  }
	  
	  hid_debugf("HID protocol is ");
	  switch(p->iface_desc.bInterfaceProtocol) {
	  case HID_PROTOCOL_NONE:
	    hid_debugf("NONE");
	    break;
	    
	  case HID_PROTOCOL_KEYBOARD:
	    hid_debugf("KEYBOARD");
	    info->iface[info->bNumIfaces].device_type = HID_DEVICE_KEYBOARD;
	    break;
	    
	  case HID_PROTOCOL_MOUSE:
	    hid_debugf("MOUSE");
	    info->iface[info->bNumIfaces].device_type = HID_DEVICE_MOUSE;
	    break;
	    
	  default:
	    hid_debugf("%d", p->iface_desc.bInterfaceProtocol);
	    break;
	  }
	}
      }
      break;

    case USB_DESCRIPTOR_ENDPOINT:
      //      hid_debugf("endpoint descriptor size %d", p->ep_desc.bLength);

      if(isGoodInterface) {

	// only interrupt in endpoints are supported
	if ((p->ep_desc.bmAttributes & 0x03) == 3 && (p->ep_desc.bEndpointAddress & 0x80) == 0x80) {
	  hid_debugf("ep %d, ival = %dms", 
		  p->ep_desc.bEndpointAddress & 0x0F, p->ep_desc.bInterval);

	  // Fill in the endpoint info structure
	  uint8_t epidx = info->bNumIfaces;
	  info->iface[epidx].interval = p->ep_desc.bInterval;
	  info->iface[epidx].ep.epAddr	 = (p->ep_desc.bEndpointAddress & 0x0F);
	  info->iface[epidx].ep.maxPktSize = p->ep_desc.wMaxPacketSize[0];
	  info->iface[epidx].ep.epAttribs	 = 0;
	  info->iface[epidx].ep.bmNakPower = USB_NAK_NOWAIT;
	  info->bNumIfaces++;
	}
      }
      break;

    case HID_DESCRIPTOR_HID:
      hid_debugf("hid desc size %d type %x", p->ep_desc.bLength, p->hid_desc.bDescrType);

      if(isGoodInterface) {
	// we need a report descriptor
	if(p->hid_desc.bDescrType == HID_DESCRIPTOR_REPORT) {
	  uint16_t len = p->hid_desc.wDescriptorLength[0] + 
	    256 * p->hid_desc.wDescriptorLength[1];
	  hid_debugf(" -> report desc size = %d", len);
	  
	  info->iface[info->bNumIfaces].report_desc_size = len;
	}
      }
      break;

    default:
      hid_debugf("unsupd desc type %d size %d", p->raw[1], p->raw[0]);
    }

    // advance to next descriptor
   //LOG("Len:%d\n", p->conf_desc.bLength);
   //timer_delay_msec(1000);
    len -= p->conf_desc.bLength;
    p = (union buf_u*)(p->raw + p->conf_desc.bLength);
  }
  
  if(len != 0) {
    hid_debugf("Config underrun: %d", len);
    return USB_ERROR_CONFIGURAION_SIZE_MISMATCH;
  }

  return 0;
}

static uint8_t usb_hid_init(usb_device_t *dev) {
  hid_debugf("%s(%x)", __FUNCTION__, dev->bAddress);

  uint8_t rcode;
  uint8_t i;
  uint16_t vid, pid;

  kbd_led_state = 0;  // default: all leds off

  usb_hid_info_t *info = &(dev->hid_info);
  
  union {
    usb_device_descriptor_t dev_desc;
    usb_configuration_descriptor_t conf_desc;
  } buf;

  // reset status
  info->bPollEnable = false;
  info->bNumIfaces = 0;

  for(i=0;i<MAX_IFACES;i++) {
    info->iface[i].qNextPollTime = 0;
    info->iface[i].ep.epAddr	 = i;
    info->iface[i].ep.maxPktSize = 8;
    info->iface[i].ep.epAttribs	 = 0;
    info->iface[i].ep.bmNakPower = USB_NAK_MAX_POWER;
  }

  // try to re-read full device descriptor from newly assigned address
  if(rcode = usb_get_dev_descr( dev, sizeof(usb_device_descriptor_t), &buf.dev_desc ))
    return rcode;

  // save vid/pid for automatic hack later
  vid = bs16(&buf.dev_desc.idVendorL);
  pid = bs16(&buf.dev_desc.idProductL);

  uint8_t num_of_conf = buf.dev_desc.bNumConfigurations;
  hid_debugf("number of configurations: %d", num_of_conf);

  for(i=0; i<num_of_conf; i++) {
    if(rcode = usb_get_conf_descr(dev, sizeof(usb_configuration_descriptor_t), i, &buf.conf_desc)) 
      return rcode;
    
    uint16_t wTotalLength = bs16(&buf.conf_desc.wTotalLengthL);
    hid_debugf("conf desc %d size %d", i, wTotalLength);
    LOG("conf descriptor %d has total size %d", i, wTotalLength);

    // parse directly if it already fitted completely into the buffer
    usb_hid_parse_conf(dev, i, wTotalLength);
  }

  hid_debugf("here:%d vid:%x pid:%x",info->bNumIfaces,vid,pid);

  // check if we found valid hid interfaces
  if(!info->bNumIfaces) {
    hid_debugf("no hid ifaces found");

    if((vid == 0x45e) && (pid == 0x028e)) {
      hid_debugf("hackn Torid\n");
      info->bNumIfaces = 1;
      info->iface[0].iface_idx = 0;
      info->iface[0].has_boot_mode = false;
      info->iface[0].is_5200daptor = false;
      info->iface[0].is_MCC = 3;
      info->iface[0].key_state = 0;
      info->iface[0].device_type = HID_DEVICE_JOYSTICK;
      info->iface[0].conf.type = CONFIG_TYPE_JOYSTICK;
      info->iface[0].jindex = joysticks++;
      info->iface[0].interval = 1;
      info->iface[0].ep.epAddr	 = 1;
      info->iface[0].ep.maxPktSize = 14;
      info->iface[0].ep.epAttribs	 = 0;
      info->iface[0].ep.bmNakPower = USB_NAK_NOWAIT;

      /*info->iface[0].conf.joystick.button[0].byte_offset = 2;
      info->iface[0].conf.joystick.button[0].bitmask;
      info->iface[0].conf.joystick.axis[0].size;
      info->iface[0].conf.joystick.axis[0].byte_offset;
      info->iface[0].conf.joystick.axis[0].logical.min;
      info->iface[0].conf.joystick.axis[0].logical.max;
      */

      // joystick pos is expected to be an axis... it isn't...
      // might be easier to directly create jmap for torid...
    }
    else
    {
      return USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;
    }
  }

  // Set Configuration Value
  rcode = usb_set_conf(dev, buf.conf_desc.bConfigurationValue);

  // process all supported interfaces
  for(i=0; i<info->bNumIfaces; i++) {
    // no boot mode, try to parse HID report descriptor
    if(!info->iface[i].has_boot_mode && !info->iface[i].is_MCC) {
 
      hid_debugf("DESC %x ", info->iface[i].report_desc_size);
      rcode = hid_get_report_descr(dev, i, info->iface[i].report_desc_size);
      if(rcode) return rcode;

      hid_debugf("TYPE%02x ", info->iface[i].device_type);
      /*
      if(info->iface[i].device_type == CONFIG_TYPE_JOYSTICK) {
	char k;
        
	iprintf("Report: type = %d, id = %d, size = %d\n", 
		info->iface[i].conf.type,
		info->iface[i].conf.report_id,
		info->iface[i].conf.report_size);
	
	for(k=0;k<2;k++)
	  iprintf("Axis%d: %d@%d %d->%d\n", k, 
		  info->iface[i].conf.joystick.axis[k].size,
		  info->iface[i].conf.joystick.axis[k].byte_offset,
		  info->iface[i].conf.joystick.axis[k].logical.min,
		  info->iface[i].conf.joystick.axis[k].logical.max);
	
	for(k=0;k<24;k++)
	  iprintf("Button%d: @%d/%d\n", k,
		  info->iface[i].conf.joystick.button[k].byte_offset,
		  info->iface[i].conf.joystick.button[k].bitmask);
      }*/
      
      
      // use fixed setup for known interfaces 
      if (i==0)
      {
      if((vid == 0x0079) && (pid == 0x0011)) {
	iprintf("hacking cheap NES pad\n");
        
        // fixed setup for nes gamepad
/*	int i;
	  static const struct {
	    uint8_t byte_offset; 
	    uint8_t bitmask;  
	  } d[] = {{5,32},{5,64},{6,16},{6,32}};
	for (i=0;i!=4;++i)
	{
	        info->iface[0].conf.joystick.button[i].byte_offset = d[i].byte_offset;
 	      	info->iface[0].conf.joystick.button[i].bitmask = d[i].bitmask;
	}*/
	info->iface[0].conf.joystick.button[0].byte_offset = 5;
        info->iface[0].conf.joystick.button[0].bitmask = 32;
        info->iface[0].conf.joystick.button[1].byte_offset = 5;
        info->iface[0].conf.joystick.button[1].bitmask = 64;
        info->iface[0].conf.joystick.button[2].byte_offset = 6;
        info->iface[0].conf.joystick.button[2].bitmask = 16;
        info->iface[0].conf.joystick.button[3].byte_offset = 6;
        info->iface[0].conf.joystick.button[3].bitmask = 32;
      }

      if((vid == 0x04d8) && (pid == 0xf6ec)) {
	iprintf("hacking 5200daptor\n");
        
	info->iface[0].conf.joystick.button[2].byte_offset = 4;
	info->iface[0].conf.joystick.button[2].bitmask = 0x40;    // "Reset"
	info->iface[0].conf.joystick.button[3].byte_offset = 4;
	info->iface[0].conf.joystick.button[3].bitmask = 0x10;    // "Start"

	info->iface[0].is_5200daptor = true;
      }

      if((vid == 0x0079) && (pid == 0x0006)) {
	iprintf("MCC classic\n");

	info->iface[0].is_MCC = 1;
      }
      if((vid == 0x1a34) && (pid == 0x0809)) {
	iprintf("MCC Xeox\n");

	info->iface[0].is_MCC = 2;
      }
      }
    }

    rcode = hid_set_idle(dev, info->iface[i].iface_idx, 0, 0);
    // MWW if (rcode && rcode != hrSTALL)
    //printf("RCODE:%02x ",rcode);
    if (rcode && rcode != OHS900_STATMASK_STALL_RXED)
      return rcode;

    //printf("BM ");
    // enable boot mode
    if(info->iface[i].has_boot_mode)
    {
      //printf("BM ");
      hid_set_protocol(dev, info->iface[i].iface_idx, HID_BOOT_PROTOCOL);
    }
  }
  
  iprintf("HID configured\n");

  // update leds
  for(i=0;i<MAX_IFACES;i++)
    if(dev->hid_info.iface[i].device_type == HID_DEVICE_KEYBOARD)
    {
//      printf("LEDS ");
      uint8_t res = hid_set_report(dev, dev->hid_info.iface[i].iface_idx, 2, 0, 1, &kbd_led_state);
//      printf("LEDS res:%02x ", res);
    }

  info->bPollEnable = true;
  return 0;
}

static uint8_t usb_hid_release(usb_device_t *dev) {
  usb_hid_info_t *info = &(dev->hid_info);

  iprintf("%s\n",__FUNCTION__);

  uint8_t i;
  // check if a joystick is released
  for(i=0;i<info->bNumIfaces;i++) {
    if(info->iface[i].device_type == HID_DEVICE_JOYSTICK) {
      uint8_t c_jindex = info->iface[i].jindex;
      hid_debugf("rel joystick #%d, renum", c_jindex);

      event_digital_joystick(c_jindex, 0);
      event_analog_joystick(c_jindex, 0,0);

      // walk through all devices and search for sticks with a higher id

      // search for all joystick interfaces on all hid devices
      usb_device_t *dev = usb_get_devices();
      uint8_t j;
      for(j=0;j<USB_NUMDEVICES;j++) {
	if(dev[j].bAddress && (dev[j].class == &usb_hid_class)) {
	  // search for joystick interfaces
	  uint8_t k;
	  for(k=0;k<MAX_IFACES;k++) {
	    if(dev[j].hid_info.iface[k].device_type == HID_DEVICE_JOYSTICK) {
	      if(dev[j].hid_info.iface[k].jindex > c_jindex) {
		hid_debugf("dec jindex of dev #%d from %d to %d", j, 
			dev[j].hid_info.iface[k].jindex, dev[j].hid_info.iface[k].jindex-1);

		dev[j].hid_info.iface[k].jindex--;
	      }
	    }
	  }
	}
      }
      // one less joystick in the system ...
      joysticks--;
    }
  }

  return 0;
}

#ifdef FIRMWARE_5200
// special 5200daptor button processing
static void handle_5200daptor(usb_hid_iface_info_t *iface, uint8_t *buf) {

  // list of buttons that are reported as keys
  static const struct {
    uint8_t byte_offset;   // offset of the byte within the report which the button bit is in
    uint8_t mask;          // bitmask of the button bit
    uint8_t key_code[2];   // usb keycodes to be sent for joystick 0 and joystick 1
  } button_map[] = {
    { 4, 0x10, 0x3a, 0x3d }, /* START -> f1/f4 */
    { 4, 0x20, 0x3b, 0x3e }, /* PAUSE -> f2/f5 */
    { 4, 0x40, 0x3c, 0x3f }, /* RESET -> f3/f6 */
    { 5, 0x01, 0x1e, 0x21 }, /*     1 ->  1/4  */
    { 5, 0x02, 0x1f, 0x22 }, /*     2 ->  2/5  */
    { 5, 0x04, 0x20, 0x23 }, /*     3 ->  3/6  */
    { 5, 0x08, 0x14, 0x15 }, /*     4 ->  q/r  */
    { 5, 0x10, 0x1a, 0x17 }, /*     5 ->  w/t  */
    { 5, 0x20, 0x08, 0x1c }, /*     6 ->  e/y  */
    { 5, 0x40, 0x04, 0x09 }, /*     7 ->  a/f  */
    { 5, 0x80, 0x16, 0x0a }, /*     8 ->  s/g  */
    { 6, 0x01, 0x07, 0x0b }, /*     9 ->  d/h  */
    { 6, 0x02, 0x1d, 0x19 }, /*     * ->  z/v  */
    { 6, 0x04, 0x1b, 0x05 }, /*     0 ->  x/b  */
    { 6, 0x08, 0x06, 0x11 }, /*     # ->  c/n  */
    { 0, 0x00, 0x00, 0x00 }  /* ----  end ---- */
  };

  // keyboard events are only generated for the first
  // two joysticks in the system
  if(iface->jindex > 1) return;

  // build map of pressed keys
  uint8_t i;
  uint16_t keys = 0;
  for(i=0;button_map[i].mask;i++) 
    if(buf[button_map[i].byte_offset] & button_map[i].mask)
      keys |= (1<<i);

  // check if keys have changed
  if(iface->key_state != keys) {
    uint8_t buf[6] = { 0,0,0,0,0,0 };
    uint8_t p = 0;

    // report up to 6 pressed keys
    for(i=0;(i<16)&&(p<6);i++) 
      if(keys & (1<<i))
	buf[p++] = button_map[i].key_code[iface->jindex];

    //    iprintf("5200: %d %d %d %d %d %d\n", buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);

    // generate key events
    event_keyboard(0x00, buf);

    // save current state of keys
    iface->key_state = keys;
  }
}
#endif

// special MCC button processing
static void handle_MCC(usb_hid_iface_info_t *iface, uint32_t * jmap_ptr, uint8_t type) {

/*
[start or left&right shoulder2] - start
[select or left&right shoulder1] - select
[select] - option
[3] - reset
[2] - cold start
[1] - quick select
[4] - settings

MY MAPPING
[select] - select
[start] - start
[L1|R1|3] - fire!
[L2|R2|4] - option
[L2&R2] - reboot
[L1&R2] - reset
[1] - settings
[2] - disk menu
*/
/*
    { 4, 0x41, }, * START -> f8 [1]*
    { 5, 0x41, }, * START -> f8 [2]*
    { 6, 0x42, }, * PAUSE -> f9 [3]*
    { 7, 0x43, }, * RESET -> f10 [4]*
    { 8, 0x1e, }, *     1 ->  1/4  SH L1*
    { 9, 0x1f, }, *     2 ->  2/5  SH R1*
    { 10, 0x20 }, *     3 ->  3/6 SH L2 *
    { 11, 0x14 }, *     4 ->  q/r  SH R2*
    { 12, 0x1a }, *     5 ->  w/t  SELECT*
    { 13, 0x08 }, *     6 ->  e/y  START*
    { 14, 0x04 }, *     7 ->  a/f  STICK CLICK LEFT*
    { 15, 0x16 }, *     8 ->  s/g  STICK CLICK RIGHT*
*/

uint32_t jmap = *jmap_ptr;
*jmap_ptr&=0xf;

  if (type == 2)
  {
    uint32_t mask = 0;
    if (jmap&0x40) mask ^= 0xc0;
    if (jmap&0x80) mask ^= 0x90;
    if (jmap&0x10) mask ^= 0x50;
    jmap ^= mask;
//x(left)=40 (old 80)
//y(up)=80 (old 10)
//a(down)=10 (old 40)
  }

  static const struct {
    uint8_t bit1;           // bit of the button bit (1<<bit)
    uint8_t bit2;           // bit of the button bit (1<<bit)
    uint8_t key_code;      // usb keycodes to be sent for all joysticks
    uint8_t button_bit;  
  } button_map[] = {
#ifndef FIRMWARE_5200
    { 13, 13, 0x3f, 0 }, /* start -> f6 */
    { 12, 12, 0x40, 0 }, /* select -> f7 */
    { 10, 10, 0x41, 0 }, /* l2 -> f8 */
    { 11, 11, 0x41, 0 }, /* r2 -> f8 */
    { 7, 7, 0x41, 0 }, /* 4 -> f8 */
    { 10, 11, 0x43, 0 }, /* l2&r2 -> f10 */
    { 8, 11, 0x42, 0 }, /* l1&r2 -> f9 */
    { 6, 6, 0x00, 4 }, /* 3 -> fire */
#endif
#ifdef FIRMWARE_5200
    { 13, 13, 0x3a, 0 }, /* start -> f1 (start) */
    { 12, 12, 0x3b, 0 }, /* select -> f2 (pause) */
    { 10, 10, 0x00, 5 }, /* l2 -> fire2 */
    { 11, 11, 0x00, 5 }, /* r2 -> fire2 */
    { 6, 6, 0x00, 5 }, /* 3 -> fire2 */
    { 7, 7, 0x00, 4 }, /* 4 -> fire */
#endif
    { 4, 4, 0x45, 0 }, /* 1 -> f12 */
    { 5, 5, 0x44, 0 }, /* 2 -> f11 */
    { 14, 15, 0x29, 0 }, /* both sticks  -> esc */
    { 8, 8, 0x00, 4 }, /* l1 -> fire */
    { 9, 9, 0x00, 4 }, /* r1 -> fire */
    { 10, 15, 0x43, 0 }, /* l2& rstick click -> f10 */
    { 9, 15, 0x42, 0 }, /* l1& rstick click -> f9 */
    { 0, 0, 0x00, 0 }  /* ----  end ---- */
  };

  uint8_t buf[6] = { 0,0,0,0,0,0 };
  uint8_t p = 0;

  // report up to 6 pressed keys
  // modify jmap buttons
  int i;
  for(i=0;button_map[i].bit1;i++) 
  {
    uint32_t mask = 0;
    mask |= 1<<button_map[i].bit1;
    mask |= 1<<button_map[i].bit2;

    if((jmap & mask) == mask)
    {
      if (p<6 && button_map[i].key_code)
      {
          buf[p++] = button_map[i].key_code;
      }

      if (button_map[i].button_bit)
      {
          *jmap_ptr |= 1<<button_map[i].button_bit;
      }
    }
  }
  // generate key events
  event_keyboard(0x00, buf);
}

struct buttonbits
{
	unsigned char byte;
	unsigned char mask;
	unsigned char bit;
};

static uint8_t usb_hid_poll(usb_device_t *dev) {
  usb_hid_info_t *info = &(dev->hid_info);
  int8_t i;

  if (!info->bPollEnable)
    return 0;
  
  for(i=0;i<info->bNumIfaces;i++) {
    usb_hid_iface_info_t *iface = info->iface+i;
    
    if(iface->device_type != HID_DEVICE_UNKNOWN) {

      if (timer_elapsed(iface->qNextPollTime)) {
	//      hid_debugf("poll %d...", iface->ep.epAddr);
      
	uint16_t read = iface->ep.maxPktSize;
	uint8_t buf[iface->ep.maxPktSize];
        //hid_debugf("bytes:%d ep:%x",read,iface->ep);
	uint8_t rcode = 
	  usb_in_transfer(dev, &(iface->ep), &read, buf);
	
	if (rcode) {
	  // MWW if (rcode != hrNAK)
	  if (rcode != OHS900_STATMASK_NAK_RXED)
	    hid_debugf("%s() error: %d", __FUNCTION__, rcode);
	} else {
	  
	  // successfully received some bytes
	  if(iface->has_boot_mode) {
	    if(iface->device_type == HID_DEVICE_MOUSE) {
	      // boot mouse needs at least three bytes
	      if(read >= 3) {
		// forward all three bytes to the user_io layer
		event_mouse(buf[0], buf[1], buf[2]);
	      }
	    }
	    
	    if(iface->device_type == HID_DEVICE_KEYBOARD) {
	      // boot kbd needs at least eight bytes
	      if(read >= 8) {
		event_keyboard(buf[0], buf+2);
	      }
	    }
	  }
	  
	  if(iface->device_type == HID_DEVICE_JOYSTICK) {
	    hid_config_t *conf = &iface->conf;
            //hid_debugf("JOY");
	    if(iface->is_MCC==3 || read >= conf->report_size) {
	      uint32_t jmap = 0;
	      uint16_t a[2];
	      uint8_t idx, i;
	      
	      // hid_debugf("Joystick data:"); hexdump(buf, read, 0);

	      // two axes ...
              if(iface->is_MCC==3)
	      {
                /*14 bytes (0-13)
                Left trigger: Byte 4: 0 (not pressed) to ff (pressed) - analogue
                Right trigger: Byte 5: 0 (not pressed) to ff (pressed) - analogue
                Left button: Byte 3: bit 0 (1=pressed)
                Right button: Byte 3: bit 1 (2=pressed)
                A button: Byte 3: bit 4 (0x10=pressed)
                B button: Byte 3: bit 5 (0x20=pressed)
                X button: Byte 3: bit 6 (0x40=pressed)
                Y button: Byte 3: bit 7 (0x80=pressed)
                Joy up: Byte 2: bit 0 (1=pressed)
                Joy down: Byte 2: bit 1 (2=pressed)
                Joy left: Byte 2: bit 2 (4=pressed)
                Joy right: Byte 2: bit 3 (8=pressed)
                Start: Byte 2: bit 4 (0x10=pressed)
                Select: Byte 2: bit 5 (0x20=pressed)
                Left hat LR: Byte 7: 0x80 - 0x7f (analogue - 2s comp left =-ve) 
                Left hat UD: Byte 9: 0x80 - 0x7f (analogue - 2s comp down=-ve) 
                Right hat LR: Byte b: 0x80 - 0x7f (analogue - 2s comp left =-ve) 
                Right hat UD: Byte d: 0x80 - 0x7f (analogue - 2s comp down=-ve) 
                left hat button: byte 2: bit 6 (0x40=pressed)
                right hat button: byte 2: bit 7 (0x80=pressed)*/

		if (buf[0]==00 && buf[1]==0x14)
		{
			struct buttonbits buttonbitmap[] = 
			{
/*#define JOY_RIGHT       0x01
#define JOY_LEFT        0x02
#define JOY_DOWN        0x04
#define JOY_UP          0x08*/
				{2,1,3}, //up
				{2,2,2}, //down
				{2,4,1}, //left
				{2,8,0} , //right
				{3,0x80,4}, //1
				{3,0x20,5}, //2
				{3,0x10,6}, //3
				{3,0x40,7}, //4

				{3,1,8}, //l1
				{3,2,9}, //r1
				{4,0x80,10}, //l2
				{5,0x80,11}, //r2
				{2,0x20,12}, //select
				{2,0x10,13}, //select
				{2,0x40,14}, //select
				{2,0x80,15} //select
			};
                        for (i=0;i!=(sizeof(buttonbitmap)/sizeof(struct buttonbits));++i)
			{
				if (buf[buttonbitmap[i].byte]&buttonbitmap[i].mask)
					jmap |= 1<<buttonbitmap[i].bit;
			}

/*			if (buf[2]&1) jmap |= JOY_UP;
			if (buf[2]&2) jmap |= JOY_DOWN;
			if (buf[2]&4) jmap |= JOY_LEFT;
			if (buf[2]&8) jmap |= JOY_RIGHT;
			if (buf[3]&0x80) jmap |= 1<<4; //1
			if (buf[3]&0x20) jmap |= 1<<5;  //2
			if (buf[3]&0x10) jmap |= 1<<6;  //3
			if (buf[3]&0x40) jmap |= 1<<7;  //4
			if (buf[3]&1) jmap |= 1<<8; //l1
			if (buf[3]&2) jmap |= 1<<9;  //r1
			if (buf[4]&0x80) jmap |= 1<<10;  //l2
			if (buf[5]&0x80) jmap |= 1<<11;  //r2
			if (buf[2]&0x20) jmap |= 1<<12; //select
			if (buf[2]&0x10) jmap |= 1<<13;  //start
			if (buf[2]&0x40) jmap |= 1<<14;  //lstick click
			if (buf[2]&0x80) jmap |= 1<<15;  //rstick click*/

			int8_t x = (char)buf[7];
			int8_t y = (char)buf[9];

			a[0] = 128+x;
			a[1] = 127-y;
		}
		else
		{
			a[0] = 128;
			a[1] = 128;
		}

		//for (i=0;i!=14;++i)
		//{
		//	hid_debugf("%02x",buf[i]);
		//}
            	//hid_debugf("=jmap:%x",jmap);
	      }
              else
	      {
	        for(i=0;i<2;i++) {
	          a[i] = buf[conf->joystick.axis[i].byte_offset];
	          if(conf->joystick.axis[i].size == 16)
	            a[i] += (buf[conf->joystick.axis[i].byte_offset+1])<<8;

	          // scale to 0 -> 255 range. 99% of the joysticks already deliver that
#ifdef FIRMWARE_5200
	          if((conf->joystick.axis[i].logical.min != 0) ||
	             (conf->joystick.axis[i].logical.max != 255)) {
	            a[i] = ((a[i] - conf->joystick.axis[i].logical.min) * 255)/
	              (conf->joystick.axis[i].logical.max - 
	               conf->joystick.axis[i].logical.min);
	          }
#endif
	        }
  	      
  	        //	      iprintf("JOY X:%d Y:%d\n", a[0], a[1]);
  	      
  	        // ... and twenty-four buttons
  	        for(i=0;i<24;i++)
  	 	  if(buf[conf->joystick.button[i].byte_offset] & 
  	  	     conf->joystick.button[i].bitmask) jmap |= (JOY_BTN1<<i);
              }

  	      if(a[0] <  64) jmap |= JOY_LEFT;
  	      if(a[0] > 192) jmap |= JOY_RIGHT;
  	      if(a[1] <  64) jmap |= JOY_UP;
  	      if(a[1] > 192) jmap |= JOY_DOWN;
	      
	      //	      iprintf("JOY D:%d\n", jmap);

	      idx = iface->jindex;
	      
	      // check if joystick state has changed
	      if(jmap != iface->jmap) {

		//unsigned char * temp = baseaddr;
		//baseaddr = (unsigned char volatile *)(screen_address + atari_regbase);
		//debug_pos=80;
		//printf("jmap %d(%d) changed to %x\n", idx, iface->jindex,jmap);
		//baseaddr = temp;
		//	      iprintf("jmap %d changed to %x\n", idx, jmap);
		iface->jmap = jmap;

                // do special MCC treatment
                if(iface->is_MCC)
                    handle_MCC(iface, &jmap, iface->is_MCC);
		
		// and feed into joystick input system
		event_digital_joystick(idx, jmap);
	      }
	      
	      // also send analog values
	      event_analog_joystick(idx, a[0]-128, a[1]-128);

	      // do special 5200daptor treatment
#ifdef FIRMWARE_5200
	      if(iface->is_5200daptor)
		handle_5200daptor(iface, buf);
#endif
	    }
	  }
	}
	iface->qNextPollTime += iface->interval;   // poll at requested rate
      }
    }
  }

  return 0;
}

const usb_device_class_config_t usb_hid_class = {
  usb_hid_init, usb_hid_release, usb_hid_poll };  

