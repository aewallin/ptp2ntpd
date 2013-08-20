spec_ptp2ntpd
========

Experimental White Rabbit SPEC PTP to NTP shared memory reflock driver.

Reads WR-time from SPEC shared memory and transfers it to NTP shared memory for use with the NTP shm reflock driver.

To build: place get-time.c and spec_ptp2ntpd.c in spec-sw/tools/ and add them to the Makefile.

usage
========
Add line similar to these to /etc/ntp.conf

    # experimental reflock
    server 127.127.28.0 minpoll 4 maxpoll 4

Resart ntp

    sudo service ntp restart

Start the daemon

    sudo ./spec_ptp2ntpd
   
See if it's working

    watch ntpq -np

References
==========

* http://en.wikipedia.org/wiki/White_Rabbit
* http://www.eecis.udel.edu/~mills/ntp/html/drivers/driver28.html
* http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html
