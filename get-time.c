/*
 * get wr time from shared memory. based on specmem.c
 * aw2013aug13
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h> // gettimeofday()

#include <inttypes.h> // for printing out 64-bit numbers
#include <time.h>       /* time_t, struct tm, time, gmtime */

#include "speclib.h"

uint32_t read_mem(void *base, uint32_t offset) {
    uint32_t *ptr;
    ptr = base + 0x20300 + offset;	
    return  *ptr;
}

int main(int argc, char **argv)
{
    int bar = BASE_BAR0;
    int bus = -1, dev_fn = -1;
    void *card;
    void *map_base;
    card = spec_open(bus, dev_fn);
    if(!card)
    {
        fprintf(stderr, "Can't detect a SPEC card under the given "
            "adress. Make sure a SPEC card is present in your PC, "
            "the driver is loaded and you run the program as root.\n");
        exit(1);
    }
    map_base = spec_get_base(card, bar);
    if(!map_base || map_base == (void *) -1) {
        fprintf(stderr, "%s: mmap(/dev/mem): %s\n", argv[0], strerror(errno));
        exit(1);
    }
    uint32_t nsec, s_lsb, s_msb;
    nsec = read_mem(map_base, 4 ); // read nanoseconds, in number of 62.5MHz ref cycles
    double cycle = 1/125e6;
    double nsecs_d = 1e9*nsec*cycle;
    uint32_t nsecs = (uint32_t)nsecs_d;
    s_lsb = read_mem(map_base, 8 ); // read utc lsb
    s_msb = read_mem(map_base, 12 ); // read utc msb
    uint64_t utc_seconds;
    utc_seconds = s_msb;
    utc_seconds =  (( ((uint64_t)s_msb) << 32) | (uint64_t)s_lsb);
    
    struct timeval tv;
    gettimeofday(&tv,NULL);
    
    //printf ("%d \n",rawtime);
    printf( "WR-time    : %" PRId64 ".%09d \n", utc_seconds, nsecs);
    printf ("system time: %d.%06d \n", (int)tv.tv_sec, (int)tv.tv_usec);
    spec_close(card);
    exit (0);
}
