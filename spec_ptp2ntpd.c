#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <stdint.h> // uint_32
#include <sys/time.h> // gettimeofday()
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include <inttypes.h> // for printing out 64-bit numbers
#include <time.h>       /* time_t, struct tm, time, gmtime */

#include "speclib.h"

// Linux Daemon Writing HOWTO
// http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html

// NTP reflock
// http://www.eecis.udel.edu/~mills/ntp/html/drivers/driver28.html

// Configuring NTP with // /etc/ntpd.conf
// server 127.127.t.u minpoll 4 maxpoll 4 
// t = 28 (shared memory reflock)
// u = 0 (unique id)
// fudge time1 t1 time2 t2 stratum str refid string flag1 0/1 flag2 0/1 flag3 0/1 flag4 0/1

/* this is the struct NTP expects to see in shared memory */
struct shmTime {
        int    mode; /* 0 - if valid is set:
                      *       use values,
                      *       clear valid
                      * 1 - if valid is set:
                      *       if count before and after read of data is equal:
                      *         use values
                      *       clear valid
                      */
        volatile int    count; // mode0: NTP bumps count each time it reads
        time_t          clockTimeStampSec;
        int             clockTimeStampUSec;
        time_t          receiveTimeStampSec;
        int             receiveTimeStampUSec;
        int             leap;
        int             precision;
        int             nsamples;
        volatile int    valid;  // set valid once timestamps updated
        unsigned        clockTimeStampNSec;     /* Unsigned ns timestamps */
        unsigned        receiveTimeStampNSec;   /* Unsigned ns timestamps */
        int             dummy[8];
};

struct shmTime* T;

/* from NTP shm reflock code */
struct shmTime *getShmTime (int unit) {
    int shmid=0;
    /* 0x4e545030 is NTP0.
    * Big units will give non-ascii but that's OK
    * as long as everybody does it the same way. 
    */
    shmid=shmget (0x4e545030+unit, sizeof (struct shmTime), IPC_CREAT|(unit<2?0600:0666));
    if (shmid==-1) { /*error */
        syslog(LOG_ERR,"SHM shmget (unit %d): %s",unit,strerror(errno));
        return 0;
    }
    else { /* no error  */
        struct shmTime *p=(struct shmTime *)shmat (shmid, 0, 0);
        if ((int)(long)p==-1) { /* error */
            syslog(LOG_ERR,"SHM shmat (unit %d): %s",unit,strerror(errno));
            return 0;
        }
        return p;
    }
}

/* read from SPEC shared memory */
uint32_t read_mem(void *base, uint32_t offset) {
    uint32_t *ptr;
    ptr = base + 0x20300 + offset;	
    return  *ptr;
}

/* based on minimal daemon code from the daemon-HOWTO */
int main(void) {       
        /* Our process ID and Session ID */
        pid_t pid, sid;       
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
                exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then we can exit the parent process. */
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }
        /* Change the file mode mask */
        umask(0);
                
                
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                //syslog(LOG_ERR,"setsid() error. EXIT.");
                exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if ((chdir("/")) < 0) {
                /* Log the failure */
                //syslog(LOG_ERR,"chdir() error. EXIT.");
                exit(EXIT_FAILURE);
        }
        
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        /* Daemon-specific initialization goes here */
                /* use syslog */
        setlogmask (LOG_UPTO (LOG_NOTICE));
        openlog ("spec_ptp2ntpd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        syslog (LOG_NOTICE, "Program started by User %d", getuid());

        // Attach to SPEC shared memory           
        int bar = BASE_BAR0;
        int bus = -1, dev_fn = -1;
        void *card;
        void *map_base;
        card = spec_open(bus, dev_fn);
        if(!card)
        {
            syslog(LOG_ERR, "Can't detect a SPEC card under the given "
                "adress. Make sure a SPEC card is present in your PC, "
                "the driver is loaded and you run the program as root. EXIT.\n");
            exit(1);
        }
        syslog(LOG_NOTICE,"Found SPEC at %x \n",(uint)card);

        map_base = spec_get_base(card, bar);
        if(!map_base || map_base == (void *) -1) {
            syslog(LOG_ERR,"mmap(/dev/mem): %s. EXIT.\n",  strerror(errno));
            syslog(LOG_ERR, "map_base = %x \n",(uint)map_base);
            exit(1);
        }
        syslog(LOG_NOTICE,"map_base = %u \n",(uint)map_base);
        syslog (LOG_NOTICE, "Attached to SPEC SHM at %x", (uint)map_base);
            
        // attach to NTP shared memory
        T = getShmTime(0);
        if (T==0) { /* error */
            syslog(LOG_ERR,"getShmTime() error");
            syslog(LOG_ERR,"EXITING.");
            exit(EXIT_FAILURE);
        } else {
            syslog (LOG_NOTICE, "Attached to NTP SHM at %x", (uint)T);
        }
        
        // initialize
        T->mode=1; // does it matter? set here or by NTP?
        T->leap=0; // ?
        T->precision=-10; //?
        T->nsamples=10; // ?
        shmdt(T); //detach
        
        struct timeval tv;
        uint32_t nsec_cycles, s_lsb,  usecs;
        double cycle = 1/125e6;
        uint32_t nsecs;
                        
        /* The Big Loop */
        while (1) {
            T = getShmTime(0); // attach to shared memory

            gettimeofday(&tv,NULL); // system time-stamp
            
            // WR time
            nsec_cycles = read_mem(map_base, 4 ); // read nanoseconds, in number of 62.5MHz ref cycles
            nsecs = (uint32_t) (cycle*nsec_cycles*1e9);
            usecs = (uint32_t) (cycle*nsec_cycles*1e6);
            s_lsb = read_mem(map_base, 8 ); // read utc lsb
            //s_msb = read_mem(map_base, 12 ); // read utc msb
            
            // clock time
            T->clockTimeStampSec =  s_lsb;
            T->clockTimeStampUSec = usecs; // offset 4 msec, just for fun
            T->clockTimeStampNSec = nsecs;
            
            // time stamp
            T->receiveTimeStampSec  = tv.tv_sec;
            T->receiveTimeStampUSec = tv.tv_usec;
            T->receiveTimeStampNSec = tv.tv_usec*1000;
            
            T->valid = 1;
            T->count += 1;
            
            shmdt(T); // detach, required here?

            syslog (LOG_NOTICE, "WR     time is %d.%09d ", (int)s_lsb,(int)nsecs);
            syslog (LOG_NOTICE, "system time is %d.%06d ", (int)tv.tv_sec,(int)tv.tv_usec);
            sleep(8); // minpoll is 4, so NTP reads every 16s
        }
    spec_close(card);
    closelog();
    exit(EXIT_SUCCESS);
}
