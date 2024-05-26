#ifndef __ULTRA64_ultramodern_H__
#define __ULTRA64_ultramodern_H__

#include <stdint.h>

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#define ALIGNED(x) __attribute__((aligned(x)))
#else
#define UNUSED
#define ALIGNED(x)
#endif

typedef int64_t s64;
typedef uint64_t u64;
typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef int8_t s8;
typedef uint8_t u8;

// TODO allow a compile-time flag to be set to switch between recomp mode and
// fully native mode.
#if 0 // For native compilation
#  define PTR(x) x*
#  define RDRAM_ARG
#  define RDRAM_ARG1
#  define PASS_RDRAM
#  define PASS_RDRAM1
#  define TO_PTR(type, var) var
#  define GET_MEMBER(type, addr, member) (&addr->member)
#  ifdef __cplusplus
#    define NULLPTR nullptr
#  endif
#else
#  define PTR(x) int32_t
#  define RDRAM_ARG uint8_t *rdram,
#  define RDRAM_ARG1 uint8_t *rdram
#  define PASS_RDRAM rdram,
#  define PASS_RDRAM1 rdram
#  define TO_PTR(type, var) ((type*)(&rdram[(uint64_t)var - 0xFFFFFFFF80000000]))
#  define GET_MEMBER(type, addr, member) (addr + (intptr_t)&(((type*)nullptr)->member))
#  ifdef __cplusplus
#    define NULLPTR (PTR(void))0
#  endif
#endif

#ifndef NULL
#define NULL (PTR(void) 0)
#endif

#define OS_MESG_NOBLOCK     0
#define OS_MESG_BLOCK       1

typedef s32 OSPri;
typedef s32 OSId;

typedef u64	OSTime;

#define OS_EVENT_SW1              0     /* CPU SW1 interrupt */
#define OS_EVENT_SW2              1     /* CPU SW2 interrupt */
#define OS_EVENT_CART             2     /* Cartridge interrupt: used by rmon */
#define OS_EVENT_COUNTER          3     /* Counter int: used by VI/Timer Mgr */
#define OS_EVENT_SP               4     /* SP task done interrupt */
#define OS_EVENT_SI               5     /* SI (controller) interrupt */
#define OS_EVENT_AI               6     /* AI interrupt */
#define OS_EVENT_VI               7     /* VI interrupt: used by VI/Timer Mgr */
#define OS_EVENT_PI               8     /* PI interrupt: used by PI Manager */
#define OS_EVENT_DP               9     /* DP full sync interrupt */
#define OS_EVENT_CPU_BREAK        10    /* CPU breakpoint: used by rmon */
#define OS_EVENT_SP_BREAK         11    /* SP breakpoint:  used by rmon */
#define OS_EVENT_FAULT            12    /* CPU fault event: used by rmon */
#define OS_EVENT_THREADSTATUS     13    /* CPU thread status: used by rmon */
#define OS_EVENT_PRENMI           14    /* Pre NMI interrupt */

#define	M_GFXTASK	1
#define	M_AUDTASK	2
#define	M_VIDTASK	3
#define M_NJPEGTASK 4

/////////////
// Structs //
/////////////

// Threads

typedef struct UltraThreadContext UltraThreadContext;

typedef enum {
    STOPPED,
    QUEUED,
    RUNNING,
    BLOCKED
} OSThreadState;

typedef struct OSThread_t {
    PTR(struct OSThread_t) next; // Next thread in the given queue
    OSPri priority;
    PTR(PTR(struct OSThread_t)) queue; // Queue this thread is in, if any
    uint32_t pad2;
    uint16_t flags; // These two are swapped to reflect rdram byteswapping
    uint16_t state;
    OSId id;
    int32_t pad3;
    UltraThreadContext* context; // An actual pointer regardless of platform
    int32_t sp;
} OSThread;

typedef u32 OSEvent;
typedef PTR(void) OSMesg;

typedef struct OSMesgQueue {
    PTR(OSThread) blocked_on_recv; /* Linked list of threads blocked on receiving from this queue */
    PTR(OSThread) blocked_on_send; /* Linked list of threads blocked on sending to this queue */ 
    s32 validCount;                /* Number of messages in the queue */
    s32 first;                     /* Index of the first message in the ring buffer */
    s32 msgCount;                  /* Size of message buffer */
    PTR(OSMesg) msg;               /* Pointer to circular buffer to store messages */
} OSMesgQueue;

// RSP

typedef struct {
    u32	type;
    u32	flags;

    PTR(u64) ucode_boot;
    u32	ucode_boot_size;

    PTR(u64) ucode;
    u32	ucode_size;

    PTR(u64) ucode_data;
    u32	ucode_data_size;

    PTR(u64) dram_stack;
    u32	dram_stack_size;

    PTR(u64) output_buff;
    PTR(u64) output_buff_size;

    PTR(u64) data_ptr;
    u32	data_size;

    PTR(u64) yield_data_ptr;
    u32	yield_data_size;
} OSTask_s;

typedef union {
    OSTask_s t;
    int64_t force_structure_alignment;
} OSTask;

// PI

struct OSIoMesgHdr {
    // These 3 reversed due to endianness
    u8 status;                 /* Return status */
    u8 pri;                    /* Message priority (High or Normal) */
    u16 type;                  /* Message type */
    PTR(OSMesgQueue) retQueue; /* Return message queue to notify I/O completion */
};

struct OSIoMesg {
    OSIoMesgHdr	hdr;    /* Message header */
    PTR(void) dramAddr;	/* RDRAM buffer address (DMA) */
    u32 devAddr;	    /* Device buffer address (DMA) */
    u32 size;		    /* DMA transfer size in bytes */
    u32 piHandle;	    /* PI device handle */
};

struct OSPiHandle {
    PTR(OSPiHandle_s)    unused;        /* point to next handle on the table */
    // These four members reversed due to endianness
    u8                   relDuration;   /* domain release duration */
    u8                   pageSize;      /* domain page size */
    u8                   latency;       /* domain latency */
    u8                   type;          /* DEVICE_TYPE_BULK for disk */
    // These three members reversed due to endianness
    u16                  padding;       /* struct alignment padding */
    u8                   domain;        /* which domain */
    u8                   pulse;         /* domain pulse width */
    u32                  baseAddress;   /* Domain address */
    u32                  speed;         /* for roms only */
    /* The following are "private" elements" */
    u32                  transferInfo[18];  /* for disk only */
};

typedef struct {
    u32	ctrl;
    u32	width;
    u32	burst;
    u32	vSync;
    u32	hSync;
    u32	leap;
    u32	hStart;
    u32	xScale;
    u32	vCurrent;
} OSViCommonRegs;

typedef struct {
    u32	origin;
    u32	yScale;
    u32	vStart;
    u32	vBurst;
    u32	vIntr;
} OSViFieldRegs;

typedef struct {
    u8 padding[3];
    u8 type;
    OSViCommonRegs comRegs;
    OSViFieldRegs  fldRegs[2];
} OSViMode;

/// os_pfs.h


/*
 * Structure for file system
 */
typedef struct {
    int status;
    PTR(OSMesgQueue) queue;
    int channel;
    u8 id[32];
    u8 label[32];
    int version;
    int dir_size;
    int inode_table; /* block location */
    int minode_table; /* mirrioring inode_table */
    int dir_table; /* block location */
    int inode_start_page; /* page # */

    // These two members reversed due to endianness
    u16 padding;
    u8 activebank;
    u8 banks;
} OSPfs;

typedef struct {
    u32 file_size; /* bytes */
    u32 game_code;
#if 0
    // I'm not sure how this should look like due to endianness
    u16 company_code;
    char ext_name[4];
    char game_name[16];
#endif
} OSPfsState;


/* File System error number */

#define PFS_ERR_NOPACK          1   /* no memory card is plugged or */
#define PFS_ERR_NEW_PACK        2   /* ram pack has been changed to a different one */
#define PFS_ERR_INCONSISTENT    3   /* need to run Pfschecker*/
#define PFS_ERR_CONTRFAIL       CONT_OVERRUN_ERROR              
#define PFS_ERR_INVALID         5   /* invalid parameter or file not exist*/
#define PFS_ERR_BAD_DATA        6   /* the data read from pack are bad*/
#define PFS_DATA_FULL           7   /* no free pages on ram pack*/
#define PFS_DIR_FULL            8   /* no free directories on ram pack*/
#define PFS_ERR_EXIST           9   /* file exists*/
#define PFS_ERR_ID_FATAL        10  /* dead ram pack */
#define PFS_ERR_DEVICE          11  /* wrong device type*/
#define PFS_ERR_NO_GBCART       12  /* no gb cartridge (64GB-PAK) */
#define PFS_ERR_NEW_GBCART      13  /* gb cartridge may be changed */


/// os_pfs.h


///////////////
// Functions //
///////////////

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void osInitialize(void);

typedef void (thread_func_t)(PTR(void));

void osCreateThread(RDRAM_ARG PTR(OSThread) t, OSId id, PTR(thread_func_t) entry, PTR(void) arg, PTR(void) sp, OSPri p);
void osStartThread(RDRAM_ARG PTR(OSThread) t);
void osStopThread(RDRAM_ARG PTR(OSThread) t);
void osDestroyThread(RDRAM_ARG PTR(OSThread) t);
void osYieldThread(RDRAM_ARG1);
void osSetThreadPri(RDRAM_ARG PTR(OSThread) t, OSPri pri);
OSPri osGetThreadPri(RDRAM_ARG PTR(OSThread) thread);
OSId osGetThreadId(RDRAM_ARG PTR(OSThread) t);

void osCreateMesgQueue(RDRAM_ARG PTR(OSMesgQueue), PTR(OSMesg), s32);
s32 osSendMesg(RDRAM_ARG PTR(OSMesgQueue), OSMesg, s32);
s32 osJamMesg(RDRAM_ARG PTR(OSMesgQueue), OSMesg, s32);
s32 osRecvMesg(RDRAM_ARG PTR(OSMesgQueue), PTR(OSMesg), s32);
void osSetEventMesg(RDRAM_ARG OSEvent, PTR(OSMesgQueue), OSMesg);
void osViSetEvent(RDRAM_ARG PTR(OSMesgQueue), OSMesg, u32);
void osViSwapBuffer(RDRAM_ARG PTR(void) frameBufPtr);
void osViSetMode(RDRAM_ARG PTR(OSViMode));
void osViSetSpecialFeatures(uint32_t func);
void osViBlack(uint8_t active);
void osViSetXScale(float scale);
void osViSetYScale(float scale);
PTR(void) osViGetNextFramebuffer();
PTR(void) osViGetCurrentFramebuffer();
u32 osGetCount();
OSTime osGetTime();
int osSetTimer(RDRAM_ARG PTR(OSTimer) timer, OSTime countdown, OSTime interval, PTR(OSMesgQueue) mq, OSMesg msg);
int osStopTimer(RDRAM_ARG PTR(OSTimer) timer);
u32 osVirtualToPhysical(PTR(void) addr);

s32 osPfsInitPak(RDRAM_ARG PTR(OSMesgQueue), PTR(OSPfs), int);
s32 osPfsRepairId(RDRAM_ARG PTR(OSPfs));
s32 osPfsInit(RDRAM_ARG PTR(OSMesgQueue), PTR(OSPfs), int);
s32 osPfsReFormat(RDRAM_ARG PTR(OSPfs), PTR(OSMesgQueue), int);
s32 osPfsChecker(RDRAM_ARG PTR(OSPfs));
s32 osPfsAllocateFile(RDRAM_ARG PTR(OSPfs), u16, u32, PTR(u8), PTR(u8), int, PTR(s32));
s32 osPfsFindFile(RDRAM_ARG PTR(OSPfs), u16, u32, PTR(u8), PTR(u8), PTR(s32));
s32 osPfsDeleteFile(RDRAM_ARG PTR(OSPfs), u16, u32, PTR(u8), PTR(u8));
s32 osPfsReadWriteFile(RDRAM_ARG PTR(OSPfs), s32, u8, int, int, PTR(u8));
s32 osPfsFileState(RDRAM_ARG PTR(OSPfs), s32, PTR(OSPfsState));
s32 osPfsGetLabel(RDRAM_ARG PTR(OSPfs), PTR(u8), PTR(int));
s32 osPfsSetLabel(RDRAM_ARG PTR(OSPfs), PTR(u8));
s32 osPfsIsPlug(RDRAM_ARG PTR(OSMesgQueue), PTR(u8));
s32 osPfsFreeBlocks(RDRAM_ARG PTR(OSPfs), PTR(s32));
s32 osPfsNumFiles(RDRAM_ARG PTR(OSPfs), PTR(s32), PTR(s32));

#ifdef __cplusplus
} // extern "C"
#endif

#endif
