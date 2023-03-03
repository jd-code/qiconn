# qiconn #

Qiconn is a light c++ library for pool of "socket-enriched" derivative of iostreams.

A typical usage can be found in [bulkrays](https://github.com/jd-code/bulkrays#bulkrays)

# Dependencies #
* posix socket C calls
* STL

### Building ###
the following will bring a default build :
```
autoall && ./configure
```

### Typical building dependencies ###
in order to compile on a debian buster :
* build-essential
* autotools-dev
* libtool
* expect (provides unbuffer used in the "vimtest" target)

---

> jd
