
ifneq (,$(COMPILE))
  C_SRC_OUTPUT = priv/v4l
	include erlang.mk
endif

build:
	docker build -t v4l .


compile:
	make COMPILE=true -f Makefile all
