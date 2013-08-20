spec_ptp2ntpd
========

Experimental White Rabbit SPEC PTP to NTP shared memory refclock driver.

Reads WR-time from SPEC shared memory and transfers it to NTP shared memory for use with the NTP shm refclock driver.

To build: place get-time.c and spec_ptp2ntpd.c in spec-sw/tools/ and add them to the Makefile.

usage
========
Add a line similar to this to /etc/ntp.conf

    # experimental refclock
    server 127.127.28.0 minpoll 4 maxpoll 4

Resart ntp

    sudo service ntp restart

Start the daemon

    sudo ./spec_ptp2ntpd
   
See what is going on

    watch ntpq -np

if it's working, you should see something along the lines of:

        remote           refid      st t when poll reach   delay   offset  jitter
    ==============================================================================
    *127.127.28.0    .SHM.            0 l   10   16  377    0.000    0.001   0.001
    +194.100.49.132  194.100.49.134   2 u   27   64  241    0.279    0.003   0.066
     194.100.49.134  194.100.49.134   2 u  966 1024    1    0.282    0.010   0.000

here the first line is the refclock. 

References
==========

* http://en.wikipedia.org/wiki/White_Rabbit
* http://www.eecis.udel.edu/~mills/ntp/html/drivers/driver28.html
* http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html
