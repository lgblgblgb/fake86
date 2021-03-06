extern float timercomp;
extern int16_t speakergensample(void);
extern int  initscreen (const char *ver);
extern uint16_t segregs[4];
extern uint32_t framedelay;
extern uint32_t loadrom (uint32_t addr32, const char *filename, uint8_t failure_fatal);
extern uint32_t makeupticks;
extern uint32_t nh;
extern uint32_t nw;
extern uint32_t speed;
extern uint64_t curtick;
extern uint64_t gensamplerate;
extern uint64_t hostfreq;
extern uint64_t lasttick;
extern uint64_t sampleticks;
extern uint64_t tickgap;
extern uint64_t timinginterval;
extern uint64_t totalexec;
extern uint64_t totalframes;
extern uint8_t bootdrive;
extern uint8_t cf;
extern uint8_t cgaonly;
extern uint8_t dohardreset;
extern uint8_t ethif;
extern uint8_t hdcount;
extern uint8_t keyboardwaitack;
extern uint8_t net_enabled;
extern uint8_t nextintr(void);
extern uint8_t noscale;
extern uint8_t nosmooth;
extern uint8_t RAM[0x100000];
extern uint8_t readonly[0x100000];
extern uint8_t renderbenchmark;
extern uint8_t running;
extern uint8_t scrmodechange;
extern uint8_t slowsystem;
extern uint8_t speakerenabled;
extern uint8_t useconsole;
extern uint8_t usessource;
extern uint8_t verbose;
extern void bufsermousedata (uint8_t value);
extern void dispatch(void);
extern void doirq (uint8_t irqnum);
extern void doscrmodechange(void);
extern void handleinput(void);
extern void initBlaster (uint16_t baseport, uint8_t irq);
extern void initpcap(void);
extern void initsermouse (uint16_t baseport, uint8_t irq);
extern void initsoundsource(void);
extern void inittiming(void);
extern void nethandler(void);
extern void parsecl (int argc, char *argv[]);
extern void *port_read_callback[0x10000];
extern void *port_read_callback16[0x10000];
extern void *port_write_callback[0x10000];
extern void *port_write_callback16[0x10000];
extern void sendpkt (uint8_t *src, uint16_t len);
extern void sermouseevent (uint8_t buttons, int8_t xrel, int8_t yrel);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector(uint16_t startport, uint16_t endport, void *callback);
extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_write_redirector(uint16_t startport, uint16_t endport, void *callback);
extern void setwindowtitle (const char *extra);
extern void tickBlaster(void);
extern void tickssource(void);
extern void timing(void);
extern void write86 (uint32_t addr32, uint8_t value);
extern void reset86(void);
extern void exec86 (uint32_t execloops);
extern uint8_t read86 (uint32_t addr32);
extern uint8_t portin(uint16_t portnum);
extern uint8_t portram[0x10000];
extern void portout16(uint16_t portnum, uint16_t value);
extern void portout(uint16_t portnum, uint8_t value);
extern uint8_t insertdisk (uint8_t drivenum, char *filename);
extern void diskhandler(void);
extern void ejectdisk (uint8_t drivenum);
extern void readdisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head, uint16_t sectcount);
extern int ne2000_can_receive(void);
extern void isa_ne2000_init (uint16_t baseport, uint8_t irq);
extern void ne2000_receive (const uint8_t *buf, int size);
