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

# include <sys/ipc.h>
# include <sys/shm.h>


// Linux Daemon Writing HOWTO
// http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html

// NTP reflock
// http://www.eecis.udel.edu/~mills/ntp/html/drivers/driver28.html

// ntpd.conf
// server 127.127.t.u
// t = 28 (shared memory reflock)
// u = unique id

// fudge time1 t1 time2 t2 stratum str refid string flag1 0/1 flag2 0/1 flag3 0/1 flag4 0/1

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

int main(void) {
        
        /* Our process ID and Session ID */
        pid_t pid, sid;
        
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
                exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);
                
        /* Open any logs here */        
                
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        }
        

        
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        }
        
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        /* Daemon-specific initialization goes here */
     
		 setlogmask (LOG_UPTO (LOG_NOTICE));
		 openlog ("spec_ptp2ntpd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
		 //time_t tt;
		 //struct tm *utc;
		 //char* time_string;
		 //tt = time(NULL);
		 //utc = gmtime(&tt);
		 //time_string = asctime(utc);
		 //time_string[strlen(time_string)-1] = 0;
		 //time_string = strptime( tmstr, "%C%y-%m-%dT%H:%M:%S", &tm );
		 syslog (LOG_NOTICE, "Program started by User %d", getuid());
		 
		 // attach to shared memory
		 T = getShmTime(0);
		 if (T==0) { /* error */
			syslog(LOG_ERR,"getShmTime() error");
			syslog(LOG_ERR,"EXITING.");
			exit(EXIT_FAILURE);
		}
        
        // initialize
        T->mode=1;
        T->leap=0;
        T->precision=-2;
        T->nsamples=10;
		shmdt(T); //detach
        
        struct timeval tv;
        /* The Big Loop */
        while (1) {
			T = getShmTime(0); // attach
			gettimeofday(&tv,NULL);
			// clock time
			T->clockTimeStampSec =  tv.tv_sec;
			T->clockTimeStampUSec = tv.tv_usec+4000; // offset 4 msec, just for fun
			T->clockTimeStampNSec = tv.tv_usec*1000;
			
			// time stamp
			T->receiveTimeStampSec  = tv.tv_sec;
			T->receiveTimeStampUSec = tv.tv_usec;
			T->receiveTimeStampNSec = tv.tv_usec*1000;
			
			T->valid = 1;
			T->count += 1;
			shmdt(T); // detach

			syslog (LOG_NOTICE, "system time is %d : %d ", (int)tv.tv_sec,(int)tv.tv_usec);
           
            sleep(10); 
        }
        
   closelog ();
   exit(EXIT_SUCCESS);
}
