
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

How to test (erlang required):

```
$ ./capture.erl -vi /dev/video1
Opening card XXXXXX Channel 2 on /dev/video1, input SDI 2. 720x576 'UYVY' on fps 25/1
UTC ms            	PTS        	Index	Seq	Field	Body size
1591135068057906	26494227222	1	0	4	829440
1591135068098003	26494267289	2	1	4	829440
1591135068137885	26494307290	3	2	4	829440
1591135068177829	26494347294	4	3	4	829440
1591135068217831	26494387288	5	4	4	829440
```

