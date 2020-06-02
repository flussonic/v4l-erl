
ifneq (,$(COMPILE))
	include erlang.mk
endif

build:
	docker build -t v4l .


compile:
	make COMPILE=true -f Makefile all
