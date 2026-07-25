//#include <stdio.h>

#include "timer.h"
#include "usb.h"
#include "usbhostslave.h"
#include "debug.h"
#include "memory.h"
#include "printf.h"
#include "log.h"

extern unsigned char joysticks;      // number of detected usb joysticks
usb_device_t devices[USB_NUMDEVICES];

/*#define tokSETUP  0x10  // HS=0, ISO=0, OUTNIN=0, SETUP=1
#define tokIN     0x00  // HS=0, ISO=0, OUTNIN=0, SETUP=0
#define tokOUT    0x20  // HS=0, ISO=0, OUTNIN=1, SETUP=0
#define tokINHS   0x80  // HS=1, ISO=0, OUTNIN=0, SETUP=0
#define tokOUTHS  0xA0  // HS=1, ISO=0, OUTNIN=1, SETUP=0 
#define tokISOIN  0x40  // HS=0, ISO=1, OUTNIN=0, SETUP=0
#define tokISOOUT 0x60  // HS=0, ISO=1, OUTNIN=1, SETUP=0
*/

typedef enum {tokSETUP,tokIN,tokOUT,tokINHS,tokOUTHS,tokISOIN,tokISOOUT} TOKEN;

static uint8_t outType;
static uint8_t controlAdj;
static uint8_t lineControlAdj;

#ifdef LINUX_BUILD
#define USBHOSTSLAVE_READ(ADDR) usbhostslave_read(ADDR)
#define USBHOSTSLAVE_WRITE(ADDR,VALUE) usbhostslave_write(ADDR,VALUE)
#else
#define USBHOSTSLAVE_READ(ADDR) usbhostslave[ADDR]
#define USBHOSTSLAVE_WRITE(ADDR,VALUE) usbhostslave[ADDR] = VALUE
#endif

void usb_reset_state() {
  iprintf("%s\n",__FUNCTION__);
}

usb_device_t *usb_get_devices() {
  return devices;
}

void usb_init(struct usb_host * host, int portnumber) {
  iprintf("%s\n",__FUNCTION__);

  joysticks = 0;

  // MWW max3421e_init();   // init underlaying hardware layer

  if (portnumber == 0)
  {
      host->addr = zpu_regbase + 0x800;
  }
  if (portnumber == 1)
  {
      host->addr = zpu_regbase + 0xc00;
  }
  usbhostslave = host->addr;
  host->poll = 0;
  host->delay = 0;

  USBHOSTSLAVE_WRITE(OHS900_HOSTSLAVECTLREG, OHS900_HSCTLREG_RESET_CORE);
 // timer_delay_msec(1);
  wait_us(1000);
  USBHOSTSLAVE_WRITE(OHS900_TXLINECTLREG, 0);
  USBHOSTSLAVE_WRITE(OHS900_HOSTSLAVECTLREG, OHS900_HS_CTL_INIT);
  USBHOSTSLAVE_WRITE(OHS900_SOFENREG, 0);
  USBHOSTSLAVE_WRITE(OHS900_IRQ_ENABLE, 0);

  host->usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE; 

  outType = 0;
  controlAdj = 0;
  lineControlAdj = 0;

  uint8_t i;
  for(i=0;i<USB_NUMDEVICES;i++)
    devices[i].bAddress = 0;

  usb_reset_state();
}

uint8_t usb_set_address(usb_device_t *dev, ep_t *ep, 
			uint16_t *nak_limit) {
  //  printf("  %s(addr=%x, ep=%d)\n", __FUNCTION__, addr, ep);
  *nak_limit = (1UL << ( ( ep->bmNakPower > USB_NAK_MAX_POWER ) ? 
			 USB_NAK_MAX_POWER : ep->bmNakPower) ) - 1;
  
  /*
    printf("\nAddress: %x\n", addr);
    printf(" EP: %d\n", ep);
    printf(" NAK Power: %d\n",(*ppep)->bmNakPower);
    printf(" NAK Limit: %d\n", nak_limit);
  */
  
  USBHOSTSLAVE_WRITE(OHS900_TXADDRREG, dev->bAddress);

  /* MWW (sets address and messes with mode - I plan to change mode on connect only...)
  max3421e_write_u08( MAX3421E_PERADDR, dev->bAddress); // set peripheral address
  uint8_t mode = max3421e_read_u08( MAX3421E_MODE );
  
  // Set bmLOWSPEED and bmHUBPRE in case of low-speed device, 
  // reset them otherwise
  max3421e_write_u08( MAX3421E_MODE, 
		      (dev->lowspeed) ? mode |   MAX3421E_LOWSPEED | bmHubPre : 
		                      mode & ~(MAX3421E_HUBPRE | MAX3421E_LOWSPEED)); 
  */

  controlAdj = 0;
  lineControlAdj = 0;
  if (dev->parent) // via hub
  {
    controlAdj = dev->lowspeed ? OHS900_HCTLMASK_PREAMBLE_EN : 0;
    lineControlAdj = dev->lowspeed ? 0 : OHS900_TXLCTL_MASK_FS_RATE; 
    lineControlAdj |= OHS900_TXLCTL_MASK_FS_POL; // hub always full speed polarity
  }
  else // direct
  {
    lineControlAdj = dev->lowspeed ? OHS900_TXLCTL_MASK_LSPD : OHS900_TXLCTL_MASK_FSPD;
  }
  
  return 0;
}

/* dispatch usb packet. Assumes peripheral address is set and relevant */
/* buffer is loaded/empty */
/* If NAK, tries to re-send up to nak_limit times  */
/* If nak_limit == 0, do not count NAKs, exit after timeout */
/* If bus timeout, re-sends up to USB_RETRY_LIMIT times */
/* return codes 0x00-0x0f are HRSLT (0x00 being success), 0xff means timeout */
uint8_t usb_dispatchPktWithData( TOKEN token, uint8_t ep, uint16_t nak_limit, uint8_t * data, uint16_t bytes_tosend, uint8_t * sndToggle) {
 //   printf("  %s(token=%x, ep=%d, nak_limit=%d tosend:%d)\n", 
 // 	  __FUNCTION__, token, ep, nak_limit, bytes_tosend);
  iprintf("SEND%02d ", bytes_tosend);
  unsigned long timeout = timer_get_msec() + USB_XFER_TIMEOUT;
  uint8_t tmpdata;   
  uint8_t rcode = 0x00;
  uint8_t retry_count = 0;
  uint16_t nak_count = 0;
	
  while( !timer_elapsed(timeout) )  {
    //MWW max3421e_write_u08( MAX3421E_HXFR, ( token|ep )); //launch the transfer
    USBHOSTSLAVE_WRITE(OHS900_TXENDPREG, ep);
    uint8_t control = OHS900_HCTLMASK_TRANS_REQ|controlAdj;
    uint8_t line_control = 0;
    uint8_t load_fifo = 0;
    uint8_t wait_for_sof = OHS900_HCTLMASK_SOF_SYNC;
    uint8_t expect_ack = 0;
    switch (token)
    {
    case tokSETUP:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, OHS900_SETUP);
	iprintf("S ");
	load_fifo = 1;
        expect_ack = 1;
        break;
    case tokIN:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, OHS900_IN);
	iprintf("I ");
        break;
    case tokOUT:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, *sndToggle ? OHS900_OUT_DATA1 : OHS900_OUT_DATA0);
	iprintf(*sndToggle ? "Data1 ":"Data0 ");
	load_fifo = 1;
        expect_ack = 1;
        break;
    case tokINHS:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, OHS900_IN);
	iprintf("HI ");
	//wait_for_sof = 0;
        break;
    case tokOUTHS:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, OHS900_OUT_DATA1);
	iprintf("HO ");
	load_fifo = 1;
	//wait_for_sof = 0;
        expect_ack = 1;
        break;
    case tokISOIN:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, OHS900_IN);
        control |= OHS900_HCTLMASK_ISO_EN;
        break;
    case tokISOOUT:
        USBHOSTSLAVE_WRITE(OHS900_TXTRANSTYPEREG, *sndToggle ? OHS900_OUT_DATA1 : OHS900_OUT_DATA0);
      	*sndToggle = !*sndToggle; // No acks, toggle each time
	load_fifo = 1;
        control |= OHS900_HCTLMASK_ISO_EN;
        break;
    }

    line_control |= lineControlAdj;

    USBHOSTSLAVE_WRITE(OHS900_TXFIFOCONTROLREG, OHS900_FIFO_FORCE_EMPTY);
    USBHOSTSLAVE_WRITE(OHS900_RXFIFOCONTROLREG, OHS900_FIFO_FORCE_EMPTY);

    if (load_fifo && data)
    {
  
      //filling output FIFO
      //MWW max3421e_write( MAX3421E_SNDFIFO, bytes_tosend, data );
      uint16_t toSend = bytes_tosend;
      uint8_t * dataToSend = data;
      iprintf("FIFO:");
      while (toSend--)
      {
	iprintf("%02x", *dataToSend);
        USBHOSTSLAVE_WRITE(OHS900_HOST_TXFIFO_DATA, *dataToSend++);
      }
	iprintf(" ");
      
      //set number of bytes
      //MWW max3421e_write_u08( MAX3421E_SNDBC, bytes_tosend );
    }

	{
	    rcode = USBHOSTSLAVE_READ(OHS900_HRXSTATREG);
	   // printf("Pre transfer rcode:%02x line:%02x ctrl:%02x", rcode, line_control, control);
	}

    USBHOSTSLAVE_WRITE(OHS900_TXLINECTLREG, line_control);
    //USBHOSTSLAVE_WRITE(OHS900_HOST_TX_CTLREG, control|wait_for_sof);
    USBHOSTSLAVE_WRITE(OHS900_HOST_TX_CTLREG, control);

    rcode = USB_ERROR_TRANSFER_TIMEOUT;   
    
    // wait for transfer completion
	//printf("Wait:%x %x ", timer_get_msec(), timeout);
    while( !timer_elapsed(timeout) )	{
      //tmpdata = max3421e_read_u08( MAX3421E_HIRQ );
      // MWW
      tmpdata = USBHOSTSLAVE_READ(OHS900_IRQ_STATUS);

      if( tmpdata & OHS900_INTMASK_TRANS_DONE ) {
	//clear the interrupt
	//max3421e_write_u08( MAX3421E_HIRQ, MAX3421E_HXFRDNIRQ );
	// MWW
	USBHOSTSLAVE_WRITE(OHS900_IRQ_STATUS, OHS900_INTMASK_TRANS_DONE);
	rcode = 0x00;

	iprintf("OK ");
	break;
      }
    }

    if( rcode != 0x00 )                 //exit if timeout
	{
		iprintf("TMOUT ");
      return( rcode );
	}

    //analyze transfer result
    //rcode = ( max3421e_read_u08( MAX3421E_HRSL ) & 0x0f );
    //rcode = 0x00;
    rcode = USBHOSTSLAVE_READ(OHS900_HRXSTATREG);
        
    iprintf("R%02x ", rcode);
    rcode &= ~OHS900_STATMASK_DATA_SEQ;

    if (!expect_ack && rcode == 0x00)
    {
        iprintf("EMPTY! ");
        //rcode = USB_ERROR_TRANSFER_TIMEOUT;   
        return rcode;
    }

    if (rcode&OHS900_STATMASK_ACK_RXED)
    {
        //set toggle value
        // MWW: max3421e_write_u08(MAX3421E_HCTL, 
        //    (pep->bmSndToggle) ? MAX3421E_SNDTOG1 : MAX3421E_SNDTOG0 );
        *sndToggle = !*sndToggle; // Toggled on ack
        iprintf("ACK ");
        break;
    }
    else if (rcode&OHS900_STATMASK_NAK_RXED)
    {
      nak_count++;
      if( nak_limit && ( nak_count == nak_limit ))
	return( rcode );
    }
    else if (rcode&OHS900_STATMASK_RX_TMOUT)
    {
      retry_count++;
      iprintf("Retry ");
      if( retry_count == USB_RETRY_LIMIT )
	return( rcode );
    }
    else if (rcode&(OHS900_STATMASK_CRC_ERROR|OHS900_STATMASK_BS_ERROR|OHS900_STATMASK_STALL_RXED))
    {
      return( rcode );
    }
  }

  return( rcode&~(OHS900_STATMASK_ACK_RXED));
}

uint8_t usb_dispatchPkt( uint8_t token, uint8_t ep, uint16_t nak_limit)
{
  uint8_t dummy = 0;
  return usb_dispatchPktWithData(token,ep,nak_limit,0,0,&dummy);
}

uint8_t usb_InTransfer(ep_t *pep, uint16_t nak_limit, 
		       uint16_t *nbytesptr, uint8_t* data) {
  uint8_t rcode = 0;
  uint8_t pktsize;
  uint8_t stat;
  
  uint16_t	nbytes		= *nbytesptr;
  uint8_t	maxpktsize	= pep->maxPktSize; 

  *nbytesptr = 0;
  // set toggle value
  // MWW max3421e_write_u08( MAX3421E_HCTL, 
  //	      (pep->bmRcvToggle) ? MAX3421E_RCVTOG1 : MAX3421E_RCVTOG0 );
  // MWW: TODO on receipt looks like hardware takes cart of DATA0/DATA1? Perhaps I'm meant to check DATA_SEQUENCE_BIT...
  
  // use a 'return' to exit this loop
  while( 1 ) { 
    //IN packet to EP-'endpoint'. Function takes care of NAKS.
    rcode = usb_dispatchPkt( tokIN, pep->epAddr, nak_limit );
      
    //should be 0, indicating ACK. Else return error code.
    if( rcode )
      return( rcode );
    
    /* check for RCVDAVIRQ and generate error if not present */ 
    /* the only case when absense of RCVDAVIRQ makes sense is when */
    /* toggle error occured. Need to add handling for that */
  // MWW if(( max3421e_read_u08( MAX3421E_HIRQ ) & MAX3421E_RCVDAVIRQ ) == 0 ) 
  //    return ( 0xf0 );                            //receive error
    stat = USBHOSTSLAVE_READ(OHS900_HRXSTATREG);
    iprintf("rcv %02d stat %02d ", pep->bmRcvToggle, stat);
    if (pep->bmRcvToggle != (!!(stat&OHS900_STATMASK_DATA_SEQ))) // Check data0/data1
    {
        iprintf("DATA WRONG! rcv %02d stat %02d ", pep->bmRcvToggle, stat);
        return (0xf0);
    }
    
    // MWW pktsize = max3421e_read_u08( MAX3421E_RCVBC ); // number of received bytes
    pktsize = USBHOSTSLAVE_READ(OHS900_RXFIFOCNTLSBREG);
    iprintf("rd %02d ", pktsize);
        
    int16_t mem_left = (int16_t)nbytes - *((int16_t*)nbytesptr);

    if (mem_left < 0)
      mem_left = 0;

    //MWW data = max3421e_read(MAX3421E_RCVFIFO, 
    //		 ((pktsize > mem_left) ? mem_left : pktsize), data );
    int16_t toRead = (pktsize > mem_left) ? mem_left : pktsize;
    iprintf("rd2 %02d ",toRead);
    LOG("DATA:%x:%d\n",data,toRead);
    while (toRead--)
    {
      uint8_t val = USBHOSTSLAVE_READ(OHS900_HOST_RXFIFO_DATA);
      iprintf ("%02x", val);
      *data++ = val;
    }
    iprintf(" ");

    // Clear the IRQ & free the buffer
    //MWW - already done? max3421e_write_u08( MAX3421E_HIRQ, MAX3421E_RCVDAVIRQ );
    *nbytesptr += pktsize;							
    // add this packet's byte count to total transfer length
    /* The transfer is complete under two conditions:           */
    /* 1. The device sent a short packet (L.T. maxPacketSize)   */
    /* 2. 'nbytes' have been transferred.                       */

    pep->bmRcvToggle = (!(stat&OHS900_STATMASK_DATA_SEQ)); // expect other way next time

    // have we transferred 'nbytes' bytes?
    if (( pktsize < maxpktsize ) || (*nbytesptr >= nbytes )) {     
      // Save toggle value
      //MWW pep->bmRcvToggle = (( max3421e_read_u08( MAX3421E_HRSL ) & 
	//		    MAX3421E_RCVTOGRD )) ? 1 : 0;
      
      return 0;
    }
  }
}

/* IN transfer to arbitrary endpoint. Assumes PERADDR is set. Handles multiple packets */
/* if necessary. Transfers 'nbytes' bytes. Keep sending INs and writes data to memory area */
/* pointed by 'data' */
/* rcode 0 if no errors. rcode 01-0f is relayed from dispatchPkt(). Rcode f0 means RCVDAVIRQ error, */
/* fe USB xfer timeout */
uint8_t usb_in_transfer( usb_device_t *dev, ep_t *ep, uint16_t *nbytesptr, uint8_t* data) {
  uint16_t nak_limit = 0;

  uint8_t rcode = usb_set_address(dev, ep, &nak_limit);
  if (rcode) return rcode;

  return usb_InTransfer(ep, nak_limit, nbytesptr, data);
}

uint8_t usb_OutTransfer(ep_t *pep, uint16_t nak_limit, 
			uint16_t nbytes, uint8_t *data) {
  //  printf("%s(%d)\n", __FUNCTION__, nbytes);

  uint8_t rcode = 0;
  uint16_t bytes_tosend;
  uint16_t bytes_left = nbytes;
  
  uint8_t maxpktsize = pep->maxPktSize; 
 
  if (maxpktsize < 1 || maxpktsize > 64)
    return USB_ERROR_INVALID_MAX_PKT_SIZE;
 
  //unsigned long timeout = timer_get_msec() + USB_XFER_TIMEOUT;
  
  while( bytes_left ) {
    bytes_tosend = ( bytes_left >= maxpktsize ) ? maxpktsize : bytes_left;

    uint8_t sndToggle = pep->bmSndToggle;
    rcode = usb_dispatchPktWithData( tokOUT, pep->epAddr, nak_limit, data, bytes_tosend, &sndToggle );
    pep->bmSndToggle = sndToggle!=0;
    if (rcode)
    {
       return(rcode);
    }

/*    MWW (this was custom due to bug in chip I think) // dispatch packet
    max3421e_write_u08( MAX3421E_HXFR, ( tokOUT | pep->epAddr ));

    //wait for the completion IRQ
    while(!(max3421e_read_u08( MAX3421E_HIRQ ) & MAX3421E_HXFRDNIRQ ));
    max3421e_write_u08( MAX3421E_HIRQ, MAX3421E_HXFRDNIRQ );    //clear IRQ
    rcode = max3421e_read_u08( MAX3421E_HRSL ) & 0x0f;
    
    while( rcode && ( timeout > timer_get_msec())) {
      switch( rcode ) {
      case hrNAK:
	nak_count ++;
	if( nak_limit && ( nak_count == nak_limit )) 
	  return( rcode );
	break;
      case hrTIMEOUT:
	retry_count ++;
	if( retry_count == USB_RETRY_LIMIT ) 
	  return( rcode );
	break;
      default:
	return( rcode );
      }
      
      // process NAK according to Host out NAK bug 
      max3421e_write_u08( MAX3421E_SNDBC, 0 );
      max3421e_write_u08( MAX3421E_SNDFIFO, *data );
      max3421e_write_u08( MAX3421E_SNDBC, bytes_tosend );

      // dispatch packet
      max3421e_write_u08( MAX3421E_HXFR, ( tokOUT | pep->epAddr ));

      // wait for the completion IRQ
      while(!(max3421e_read_u08( MAX3421E_HIRQ ) & MAX3421E_HXFRDNIRQ ));
      max3421e_write_u08( MAX3421E_HIRQ, MAX3421E_HXFRDNIRQ );      // clear IRQ
      rcode = ( max3421e_read_u08( MAX3421E_HRSL ) & 0x0f );
    }//while( rcode && .... */
    bytes_left -= bytes_tosend;
    data += bytes_tosend;
  }//while( bytes_left...

  //update toggle
  // MWW: pep->bmSndToggle = ( max3421e_read_u08( MAX3421E_HRSL ) & MAX3421E_SNDTOGRD ) ? 1 : 0;

  return (rcode);    //should be 0 in all cases
}

/* OUT transfer to arbitrary endpoint. Handles multiple packets if necessary. Transfers 'nbytes' bytes. */
/* Handles NAK bug per Maxim Application Note 4000 for single buffer transfer   */
/* rcode 0 if no errors. rcode 01-0f is relayed from HRSL                       */
uint8_t usb_out_transfer(usb_device_t *dev, ep_t *ep, uint16_t nbytes, uint8_t* data ) {
  uint16_t nak_limit = 0;

  uint8_t rcode = usb_set_address(dev, ep, &nak_limit);
  if (rcode) return rcode;

  return usb_OutTransfer(ep, nak_limit, nbytes, data);
}

/* Control transfer. Sets address, endpoint, fills control packet */
/* with necessary data, dispatches control packet, and initiates */
/* bulk IN transfer, depending on request. Actual requests are defined */
/* as inlines                   */
/* return codes:                */
/* 00       =   success         */
/* 01-0f    =   non-zero HRSLT  */

uint8_t usb_ctrl_req(usb_device_t *dev, uint8_t bmReqType, 
		    uint8_t bRequest, uint8_t wValLo, uint8_t wValHi, 
		    uint16_t wInd, uint16_t nbytes, uint8_t* dataptr) {
//    printf("%s(addr=%x, len=%d, ptr=%p)\n", __FUNCTION__,
//  	  dev->bAddress, nbytes, dataptr);
iprintf("C%02X %02X %02x ",dev->bAddress,bRequest, nbytes);
  bool direction = false;     //request direction, IN or OUT
  uint8_t rcode;   
  setup_pkt_t setup_pkt;
  uint16_t	nak_limit;
  
  rcode = usb_set_address(dev, &(dev->ep0), &nak_limit);
  if (rcode)
  {
    iprintf("set_address failed ");
    return rcode;
  }
  
  direction = (( bmReqType & 0x80 ) > 0);

  /* fill in setup packet */
  setup_pkt.ReqType_u.bmRequestType	= bmReqType;
  setup_pkt.bRequest			= bRequest;
  setup_pkt.wValueL		= wValLo;
  setup_pkt.wValueH		= wValHi;
  setup_pkt.wIndexL			= wInd&0xFF;
  setup_pkt.wIndexH			= wInd>>8;
  setup_pkt.wLengthL			= nbytes&0xFF;
  setup_pkt.wLengthH			= nbytes>>8;
  
  // transfer to setup packet FIFO
  /* MWW max3421e_write(MAX3421E_SUDFIFO, sizeof(setup_pkt_t), (uint8_t*)&setup_pkt );
  
  rcode = usb_dispatchPkt( tokSETUP, 0, nak_limit );     //dispatch packet
  */
  uint8_t dummy = 0;
  rcode = usb_dispatchPktWithData( tokSETUP, 0, nak_limit, (uint8_t*)&setup_pkt,  sizeof(setup_pkt_t), &dummy);
  if( rcode )		//return HRSLT if not zero
  {
    iprintf("setup failed:%02x ",rcode);
    return( rcode );
  }
  
  // data stage, if present
  if( dataptr != NULL )	{
    if( direction ) { //IN transfer
      dev->ep0.bmRcvToggle = 1;
      rcode = usb_InTransfer( &(dev->ep0), nak_limit, &nbytes, dataptr );
    } else { //OUT transfer
      dev->ep0.bmSndToggle = 1;
      rcode = usb_OutTransfer( &(dev->ep0), nak_limit, nbytes, dataptr );
    }    

    //return error
    if( rcode )	
    {
      iprintf("setupd failed:%02x ",rcode);
      return( rcode );
    }
  }

  // Status stage
  // GET if direction
  rcode = usb_dispatchPkt( (direction) ? tokOUTHS : tokINHS, 0, nak_limit );
  if (rcode)
  {
      iprintf("status failed:%02x ",rcode);
  }
  return rcode;
}

// list of supported device classes
static const usb_device_class_config_t *class_list[]= {
  &usb_hub_class,
  &usb_hid_class,
  NULL
};

uint8_t usb_configure(uint8_t parent, uint8_t port, bool lowspeed) {
  uint8_t rcode = 0;
  iprintf("%s(par=%x prt=%d speed=%d)\n", __FUNCTION__, parent, port, lowspeed);

  // find an empty device entry
  uint8_t i;
  for(i=0; i<USB_NUMDEVICES && devices[i].bAddress; i++);

  if(i < USB_NUMDEVICES) {
    iprintf("using entry %d\n", i);

    usb_device_t *d = devices+i;

    // setup generic info
    d->bAddress = 0;
    d->parent = parent;
    d->lowspeed = lowspeed;
    d->port = port;
    d->class = NULL;
    d->host_addr = usbhostslave;

    // setup endpoint 0
    d->ep0.epAddr	= 0;
    d->ep0.maxPktSize	= 8;
    d->ep0.epAttribs	= 0;
    d->ep0.bmNakPower	= USB_NAK_MAX_POWER;

    // --- enumerate device ---

    // Assign new address to the device
    // (address is simply the number of the free slot + 1)
    iprintf("Set addr %x\n", i+1);
    rcode = usb_set_addr(d, i+1);
    if(rcode) {
      iprintf("failed to assign address:%x\n", rcode);
      return rcode;
    }

    // try to connect device to one of the supported classes
    uint8_t c;
    for(c=0;class_list[c];c++) {
      iprintf("trying to init class %d\n", c);
      rcode = class_list[c]->init(d);

      if (!rcode) {
	d->class = class_list[c];

	iprintf(" -> accepted :-)\n");
	// ok, device accepted by class

	return 0;
      }
  
      iprintf(" -> not accepted :-(\n");
    }
  } else
  {
    iprintf("no more free entries\n");
  }

  iprintf("unsupported device\n");
  return 0;
}

void usb_poll(struct usb_host * host) {
  bool lowspeed = false;

  usbhostslave = host->addr;

  // max poll 1ms
  if(timer_elapsed(host->poll)) {
    host->poll = timer_get_msec()+1;

    // poll underlaying hardware layer
    //MWW tmpdata = max3421e_poll();
    uint8_t conState = USBHOSTSLAVE_READ(OHS900_RXCONNSTATEREG);
    switch(conState)
    {
    case OHS900_DISCONNECT_STATE:
      if(( host->usb_task_state & USB_STATE_MASK ) != USB_STATE_DETACHED ) 
        host->usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;
      break;
    case OHS900_LS_CONN_STATE:
      lowspeed = true;
      // intentional fall-through ...
    case OHS900_FS_CONN_STATE:
      if(( host->usb_task_state & USB_STATE_MASK ) == USB_STATE_DETACHED ) {
        host->delay = timer_get_msec() + USB_SETTLE_DELAY;
        host->usb_task_state = USB_ATTACHED_SUBSTATE_SETTLE;
      }
      break;
    }
  
    /* modify USB task state if Vbus changed */
  /*  switch( tmpdata )  {
  
      // illegal state
    case MAX3421E_STATE_SE1:   
      host->usb_task_state = USB_DETACHED_SUBSTATE_ILLEGAL;
      lowspeed = false;
      break;
  
      // disconnected
    case MAX3421E_STATE_SE0:
      if(( host->usb_task_state & USB_STATE_MASK ) != USB_STATE_DETACHED ) 
        host->usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;
      lowspeed = false;
      break;
  
      // attached
    case MAX3421E_STATE_LSHOST:
      lowspeed = true;
      // intentional fall-through ...
  
    case MAX3421E_STATE_FSHOST:
      if(( host->usb_task_state & USB_STATE_MASK ) == USB_STATE_DETACHED ) {
        delay = timer_get_msec() + USB_SETTLE_DELAY;
        host->usb_task_state = USB_ATTACHED_SUBSTATE_SETTLE;
      }
      break;
    }*/

    // poll all configured devices
    uint8_t i;
    LOG("Poll\n");
    //printf("Poll\n");
    for (i=0; i<USB_NUMDEVICES; i++)
      if(devices[i].bAddress && devices[i].class && devices[i].class->poll && devices[i].host_addr == usbhostslave)
	devices[i].class->poll(devices+i);
	// MWW (rcode unused error) rcode = devices[i].class->poll(dev+i);
    
    switch( host->usb_task_state ) {
    case USB_DETACHED_SUBSTATE_INITIALIZE:
      usb_reset_state();
      
      // just remove everything ...
      for (i=0; i<USB_NUMDEVICES; i++) {
	if(devices[i].bAddress && devices[i].class && devices[i].host_addr == usbhostslave) {
	  devices[i].class->release(devices+i);
	  // MWW (rcode unused error) rcode = devices[i].class->release(devices+i);
	  devices[i].bAddress = 0;
	}
      }
    
      host->usb_task_state = USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE;
      break;
      
    case USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE:
    case USB_DETACHED_SUBSTATE_ILLEGAL:
      break;
      
    case USB_ATTACHED_SUBSTATE_SETTLE:              //settle time for just attached device            
      if( timer_elapsed(host->delay) ) 
	host->usb_task_state = USB_ATTACHED_SUBSTATE_RESET_DEVICE;
      break;
      
    case USB_ATTACHED_SUBSTATE_RESET_DEVICE:
      // MWW max3421e_write_u08( MAX3421E_HCTL, MAX3421E_BUSRST );	             // issue bus reset
      USBHOSTSLAVE_WRITE(OHS900_SOFENREG, 0);
      USBHOSTSLAVE_WRITE(OHS900_TXLINECTLREG, OHS900_TXLCTL_MASK_SE0);
      host->usb_task_state = USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE;
      host->delay = timer_get_msec() + 50; // send reset for 50msec
      break;
      
    case USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE:
      if( timer_elapsed(host->delay) )
      {
        USBHOSTSLAVE_WRITE(OHS900_SOFENREG, OHS900_MASK_SOF_ENA);
        //USBHOSTSLAVE_WRITE(OHS900_TXLINECTLREG, OHS900_TXLCTL_MASK_NORMAL);
        USBHOSTSLAVE_WRITE(OHS900_TXLINECTLREG, lowspeed ? OHS900_TXLCTL_MASK_LSPD : OHS900_TXLCTL_MASK_FSPD);

        USBHOSTSLAVE_WRITE(OHS900_IRQ_STATUS, OHS900_INTMASK_SOFINTR);
  
        /*if(( !max3421e_read_u08( MAX3421E_HCTL ) & MAX3421E_BUSRST ) ) {
  	tmpdata = max3421e_read_u08( MAX3421E_MODE ) | MAX3421E_SOFKAENAB;   // start SOF generation
  	max3421e_write_u08( MAX3421E_MODE, tmpdata );
        }*/
        host->delay = timer_get_msec() + 20;                           //20ms wait after reset per USB spec
        host->usb_task_state = USB_ATTACHED_SUBSTATE_WAIT_SOF;
      }
      break;
      
    case USB_ATTACHED_SUBSTATE_WAIT_SOF:  //todo: change check order
      // MWW if( max3421e_read_u08( MAX3421E_HIRQ ) & MAX3421E_FRAMEIRQ ) { //when first SOF received we can continue
      if (USBHOSTSLAVE_READ(OHS900_IRQ_STATUS)&OHS900_INTMASK_SOFINTR)
      {
        USBHOSTSLAVE_WRITE(OHS900_IRQ_STATUS, OHS900_INTMASK_SOFINTR);
	if( timer_elapsed(host->delay) ) //20ms passed
	  host->usb_task_state = USB_STATE_CONFIGURING;
      }
      break;
      
    case USB_STATE_CONFIGURING:
      // configure root device
      usb_configure(0, 0, lowspeed);
      host->usb_task_state = USB_STATE_RUNNING;
    break;
    
    case USB_STATE_RUNNING:
      break;
    }
  }

  if (USBHOSTSLAVE_READ(OHS900_IRQ_STATUS)&OHS900_INTMASK_SOFINTR)
  {
    USBHOSTSLAVE_WRITE(OHS900_IRQ_STATUS, OHS900_INTMASK_SOFINTR);
  }

}

uint8_t usb_release_device(uint8_t parent, uint8_t port) {
  iprintf("%s(parent=%x, port=%d\n", __FUNCTION__, parent, port);


  int i;
  for(i=0; i<USB_NUMDEVICES; i++) {
    if(devices[i].bAddress && devices[i].parent == parent && devices[i].port == port && devices[i].host_addr == usbhostslave) {
      iprintf("  -> device with address %x\n", devices[i].bAddress);

      // check if this is a hub (parent of some other device)
      // and release its kids first
      uint8_t j;
      for(j=0; j<USB_NUMDEVICES; j++) {
	if(devices[j].parent == devices[i].bAddress && devices[j].host_addr == devices[i].host_addr)
	  usb_release_device(devices[j].parent, devices[j].port);
      }
      
      uint8_t rcode = 0;
      if(devices[i].class)
	rcode = devices[i].class->release(devices+i);

      devices[i].bAddress = 0;
      return rcode;	
    }
  }

  // this should never happen ...
  return 0;
}

uint8_t usb_get_dev_descr( usb_device_t *dev, uint16_t nbytes, usb_device_descriptor_t* p )  {
  return( usb_ctrl_req( dev, USB_REQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 
	       0x00, USB_DESCRIPTOR_DEVICE, 0x0000, nbytes, (uint8_t*)p));
}

//get configuration descriptor  
uint8_t usb_get_conf_descr( usb_device_t *dev, uint16_t nbytes, 
			    uint8_t conf, usb_configuration_descriptor_t* p )  {
  LOG("GET_CONF:%x\n",p);
  return( usb_ctrl_req( dev, USB_REQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 
	       conf, USB_DESCRIPTOR_CONFIGURATION, 0x0000, nbytes, (uint8_t*)p));
}

uint8_t usb_set_addr( usb_device_t *dev, uint8_t newaddr )  {
  iprintf("%s(%x)\n", __FUNCTION__, newaddr);
  
  uint8_t rcode = usb_ctrl_req( dev, USB_REQ_SET, USB_REQUEST_SET_ADDRESS, newaddr, 
				0x00, 0x0000, 0x0000, NULL);
  if(!rcode) dev->bAddress = newaddr;
  return rcode;
}

//set configuration
uint8_t usb_set_conf( usb_device_t *dev, uint8_t conf_value )  {
  return( usb_ctrl_req( dev, USB_REQ_SET, USB_REQUEST_SET_CONFIGURATION,
			conf_value, 0x00, 0x0000, 0x0000, NULL));
}

