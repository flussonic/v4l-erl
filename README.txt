
V4L2 for Erlang
===============


```
make build
```

will compile all this in docker container

You can also use your rebar3 or other build tool to build this.


This code does not use libv4l2, because this library does strange things and breaks some devices.
Only pure ioctl calls.


To compile under linux:


```
make compile
```
