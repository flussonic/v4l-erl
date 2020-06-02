%
% License: BSD
%
-module(v4l).
-author('Max Lapshin <max@maxidoors.ru>').
-copyright('Max Lapshin <max@maxidoors.ru>').

-on_load(load_nif/0).
-export([nif_loaded/0]).
-export([open/1, capabilities/1, input_count/1, input/2]).
-export([get_format/2, get_param/2]).
-export([request_buffers/4, queue_buffer/4, dequeue_buffer/3]).
-export([start/2, select/1]).
-export([cropcap/2, dv_timings_cap/1]).


load_nif() ->
  PrivDir = case code:lib_dir(v4l, priv) of
    P when is_list(P) -> P;
    _ -> "./priv"
  end,
  LibPath = filename:join([
    PrivDir,
    erlang:system_info(system_architecture),
    "v4l.so"
  ]),
  erlang:load_nif(LibPath, 0).


nif_loaded() ->
  false.


open(Path) ->
  open0(binary_to_list(iolist_to_binary(Path))).

open0(_) ->
  erlang:nif_error(not_loaded).

capabilities(Capture) ->
  querycap0(Capture).

querycap0(_Capture) ->
  erlang:nif_error(not_loaded).

dv_timings_cap(Capture) ->
  dv_timings_cap0(Capture).

dv_timings_cap0(_) -> erlang:nif_error(not_loaded).

cropcap(Capture, Type) ->
  cropcap0(Capture, Type).

cropcap0(_,_) -> erlang:nif_error(not_loaded).


input_count(Capture) ->
  g_input0(Capture).

g_input0(_) -> erlang:nif_error(not_loaded).

input(Capture, Number) when is_integer(Number) ->
  enuminput0(Capture, Number).

enuminput0(_,_) -> erlang:nif_error(not_loaded).


get_format(Capture, Type) when Type == video_capture ->
  get_format0(Capture, Type).

get_format0(_,_) -> erlang:nif_error(not_loaded).

get_param(Capture, Type) when Type == video_capture ->
  get_parm0(Capture, Type).

get_parm0(_,_) -> erlang:nif_error(not_loaded).

request_buffers(Capture, Type, Memory, Count) ->
  request_buffers0(Capture, Type, Memory, Count).

request_buffers0(_,_,_,_) -> erlang:nif_error(not_loaded).


queue_buffer(Capture, Type, Memory, I) when is_integer(I) ->
  qbuf0(Capture, Type, Memory, I).

qbuf0(_,_,_,_) -> erlang:nif_error(not_loaded).


dequeue_buffer(Capture, Type, Memory) ->
  dqbuf0(Capture, Type, Memory).

dqbuf0(_,_,_) -> erlang:nif_error(not_loaded).


start(Capture, Type) ->
  streamon0(Capture, Type).

streamon0(_,_) -> erlang:nif_error(not_loaded).

select(Capture) ->
  select0(Capture).

select0(_) -> erlang:nif_error(not_loaded).

