FROM ubuntu:20.04

RUN DEBIAN_FRONTEND=noninteractive apt update && apt install -y build-essential erlang-dev vim-tiny

WORKDIR /v4l
ADD erlang.mk .
ADD c_src c_src
ADD src src
ADD Makefile .
RUN make compile


