// system.cc 
//	Nachos initialization and cleanup routines.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"

// This defines *all* of the global data structures used by Nachos.
// These are all initialized and de-allocated by this file.

Thread *currentThread;			// the thread we are running now
Thread *threadToBeDestroyed;  		// the thread that just finished
Scheduler *scheduler;			// the ready list
Interrupt *interrupt;			// interrupt status
Statistics *stats;			// performance metrics
Timer *timer;				// the hardware timer device,
					// for invoking context switches

unsigned numPagesAllocated;              // number of physical frames allocated

Thread *threadArray[MAX_THREAD_COUNT];  // Array of thread pointers
unsigned thread_index;			// Index into this array (also used to assign unique pid)
bool initializedConsoleSemaphores;
bool exitThreadArray[MAX_THREAD_COUNT];  //Marks exited threads

TimeSortedWaitQueue *sleepQueueHead;	// Needed to implement SC_Sleep

int schedulingAlgo;			// Scheduling algorithm to simulate
char **batchProcesses;			// Names of batch processes
int *priority;				// Process priority

int cpu_burst_start_time;        // Records the start of current CPU burst
int completionTimeArray[MAX_THREAD_COUNT];        // Records the completion time of all simulated threads
bool excludeMainThread;		// Used by completion time statistics calculation

List *pagesfree;
int next_phys_index;
int free_page_count;
int * physpid;
int * physvpn;
int *fifo_array;
int fifo_count;
int fifo_head;
int fifo_tail;
node * LRU;

dbl *fifo;
// Our definations







#ifdef FILESYS_NEEDED
FileSystem  *fileSystem;
#endif

#ifdef FILESYS
SynchDisk   *synchDisk;
#endif

#ifdef USER_PROGRAM	// requires either FILESYS or FILESYS_STUB
Machine *machine;	// user program memory and registers
#endif

#ifdef NETWORK
PostOffice *postOffice;
#endif


// External definition, to allow us to take a pointer to this function
extern void Cleanup();


//----------------------------------------------------------------------
// TimerInterruptHandler
// 	Interrupt handler for the timer device.  The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	"dummy" is because every interrupt handler takes one argument,
//		whether it needs it or not.
//----------------------------------------------------------------------
static void
TimerInterruptHandler(int dummy)
{
    TimeSortedWaitQueue *ptr;
    if (interrupt->getStatus() != IdleMode) {
        // Check the head of the sleep queue
        while ((sleepQueueHead != NULL) && (sleepQueueHead->GetWhen() <= (unsigned)stats->totalTicks)) {
           sleepQueueHead->GetThread()->Schedule();
           ptr = sleepQueueHead;
           sleepQueueHead = sleepQueueHead->GetNext();
           delete ptr;
        }
        //printf("[%d] Timer interrupt.\n", stats->totalTicks);
        if ((schedulingAlgo == ROUND_ROBIN) || (schedulingAlgo == UNIX_SCHED)) {
           if ((stats->totalTicks - cpu_burst_start_time) >= SCHED_QUANTUM) {
              ASSERT(cpu_burst_start_time == currentThread->GetCPUBurstStartTime());
	      interrupt->YieldOnReturn();
           }
        }
    }
}

//----------------------------------------------------------------------
// Initialize
// 	Initialize Nachos global data structures.  Interpret command
//	line arguments in order to determine flags for the initialization.  
// 
//	"argc" is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3 
//	"argv" is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
//----------------------------------------------------------------------
void
Initialize(int argc, char **argv)
{
    int argCount, i;
    char* debugArgs = "";
    bool randomYield = FALSE;

    initializedConsoleSemaphores = false;
    numPagesAllocated = 0;


    pagesfree = new List;
    next_phys_index = 0;
    free_page_count = 0;
    physpid = new int[NumPhysPages];
    physvpn = new int[NumPhysPages];
    fifo_array = new int[NumPhysPages];
    fifo_head = 0;
    fifo_count = 0;
    fifo_tail = 0;
    LRU = new node[NumPhysPages];
    fifo = new dbl();

    schedulingAlgo = NON_PREEMPTIVE_BASE;	// Default

    batchProcesses = new char*[MAX_BATCH_SIZE];
    ASSERT(batchProcesses != NULL);
    for (i=0; i<MAX_BATCH_SIZE; i++) {
       batchProcesses[i] = new char[256];
       ASSERT(batchProcesses[i] != NULL);
    }

    priority = new int[MAX_BATCH_SIZE];
    ASSERT(priority != NULL);
    
    excludeMainThread = FALSE;

    for (i=0; i<MAX_THREAD_COUNT; i++) { threadArray[i] = NULL; exitThreadArray[i] = false; completionTimeArray[i] = -1; }
    thread_index = 0;

    sleepQueueHead = NULL;

#ifdef USER_PROGRAM
    bool debugUserProg = FALSE;	// single step user program
#endif
#ifdef FILESYS_NEEDED
    bool format = FALSE;	// format disk
#endif
#ifdef NETWORK
    double rely = 1;		// network reliability
    int netname = 0;		// UNIX socket name
#endif
    
    for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
	argCount = 1;
	if (!strcmp(*argv, "-d")) {
	    if (argc == 1)
		debugArgs = "+";	// turn on all debug flags
	    else {
	    	debugArgs = *(argv + 1);
	    	argCount = 2;
	    }
	} else if (!strcmp(*argv, "-rs")) {
	    ASSERT(argc > 1);
	    RandomInit(atoi(*(argv + 1)));	// initialize pseudo-random
						// number generator
	    randomYield = TRUE;
	    argCount = 2;
	}
#ifdef USER_PROGRAM
	if (!strcmp(*argv, "-s"))
	    debugUserProg = TRUE;
#endif
#ifdef FILESYS_NEEDED
	if (!strcmp(*argv, "-f"))
	    format = TRUE;
#endif
#ifdef NETWORK
	if (!strcmp(*argv, "-l")) {
	    ASSERT(argc > 1);
	    rely = atof(*(argv + 1));
	    argCount = 2;
	} else if (!strcmp(*argv, "-m")) {
	    ASSERT(argc > 1);
	    netname = atoi(*(argv + 1));
	    argCount = 2;
	}
#endif
    }

    DebugInit(debugArgs);			// initialize DEBUG messages
    stats = new Statistics();			// collect statistics
    interrupt = new Interrupt;			// start up interrupt handling
    scheduler = new Scheduler();		// initialize the ready queue
    //if (randomYield)				// start the timer (if needed)
       timer = new Timer(TimerInterruptHandler, 0, randomYield);

    threadToBeDestroyed = NULL;

    // We didn't explicitly allocate the current thread we are running in.
    // But if it ever tries to give up the CPU, we better have a Thread
    // object to save its state.
    currentThread = NULL;
    currentThread = new Thread("main", MIN_NICE_PRIORITY);		
    currentThread->setStatus(RUNNING);
    stats->start_time = stats->totalTicks;
    cpu_burst_start_time = stats->totalTicks;

    interrupt->Enable();
    CallOnUserAbort(Cleanup);			// if user hits ctl-C
    
#ifdef USER_PROGRAM
    machine = new Machine(debugUserProg);	// this must come first
#endif

#ifdef FILESYS
    synchDisk = new SynchDisk("DISK");
#endif

#ifdef FILESYS_NEEDED
    fileSystem = new FileSystem(format);
#endif

#ifdef NETWORK
    postOffice = new PostOffice(netname, rely, 10);
#endif
}

//----------------------------------------------------------------------
// Cleanup
// 	Nachos is halting.  De-allocate global data structures.
//----------------------------------------------------------------------
void
Cleanup()
{
    printf("\nCleaning up...\n");
#ifdef NETWORK
    delete postOffice;
#endif
    
#ifdef USER_PROGRAM
    delete machine;
#endif

#ifdef FILESYS_NEEDED
    delete fileSystem;
#endif

#ifdef FILESYS
    delete synchDisk;
#endif
    
    delete timer;
    delete scheduler;
    delete interrupt;
    
    Exit(0);
}


dbl::dbl(){
    head = NULL;
    tail = NULL;
}

dbl::~dbl(){

}

void 
dbl::insertathead(node * nd){
    nd->prev = NULL;
    nd->next = head;
    if(head!=NULL) nd->next->prev=nd;
    else tail =nd;
    head = nd;
}

void
dbl::deletenode(node * nd){
    if(nd!=head){
        nd->prev->next = nd->next;
        if(nd==tail) tail = nd->prev;
        else nd->next->prev = nd->prev;
        delete nd;
    }
    else{
        head=nd->next;
        if(tail==nd) tail = nd->prev;
        else nd->next->prev = NULL;
        delete nd;
    }
}

void
dbl::bringtotop(node* nd){
    if(nd!=head){
        nd->prev->next = nd->next;
        if(nd->next==NULL){
            tail=nd->prev;
        }
        else nd->next->prev=nd->prev;
        insertathead(nd);
    }
}

node* 
dbl::makenode(int ppn){
    node  *nd = new node;
    nd->ppn=ppn;
//    DEBUG('y', "hello\n");
    return nd;
}