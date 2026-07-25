#define hid_debugf(IN, ...) {};
#define hidp_debugf(IN, ...) {};
#define hub_debugf(IN, ...) {};
#define iprintf(IN, ...) {};

extern struct SimpleFile * usb_file;

void usb_log_init(struct SimpleFile * file);

void usb_log(char * format, ...);

//#define hid_debugf usb_log
//#define hidp_debugf usb_log
//#define iprintf usb_log

