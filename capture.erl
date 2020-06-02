#!/usr/bin/env escript
%%
%%! -pa ebin

-mode(compile).

main([]) ->
  io:fwrite(standard_error, "~s -vi /dev/video1 -ai hw:1,0\n", [escript:script_name()]),
  erlang:halt(2);


main(Args) ->
  code:add_pathz("ebin"),
  Opts = parse_args(Args),
  VideoDevice = maps:get(video_device, Opts, <<"/dev/video0">>),
  case v4l:open(VideoDevice) of
    {ok, VCapture} ->
      init_and_loop(VideoDevice, VCapture);
    {error, E} ->
      io:fwrite(standard_error, "Failed to open ~s: ~p\n", [VideoDevice, E]),
      erlang:halt(3)
  end.

init_and_loop(VideoDevice, VCapture) ->
  #{card := Card} = v4l:capabilities(VCapture),
  #{name := InputName} = v4l:input(VCapture, 0),
  #{width := Width, height := Height, bytesperline := _LineSize, pixelformat := PixFmt} = v4l:get_format(VCapture, video_capture), 
  #{fps_den := FpsDen, fps_num := FpsNum} = v4l:get_param(VCapture, video_capture),

  io:format("Opening card ~s on ~s, input ~s. ~Bx~B ~p on fps ~B/~B\n", [Card, VideoDevice, InputName,
      Width, Height, PixFmt, FpsNum, FpsDen]),
  io:format("UTC ms            \tPTS        \tIndex\tSeq\tField\tBody size\n"),

  {ok,VBufCount} = v4l:request_buffers(VCapture, video_capture, mmap, 256),
  [ok = v4l:queue_buffer(VCapture, video_capture, mmap, I) || I <- lists:seq(0,VBufCount-1)],
  v4l:start(VCapture, video_capture),

  loop(VCapture).



loop(VCapture) ->
  {ok, VRef} = v4l:select(VCapture),
  Msg = receive M -> M end,
  case Msg of
    {select, _, VRef, ready_input} ->
      case v4l:dequeue_buffer(VCapture, video_capture, mmap) of
        eagain -> % EAGAIN
          io:fwrite(standard_error, "buffer busy with eagain\n", []),
          erlang:halt(5);
        #{pts := PTS, index := I, body := Raw, sequence := Seq, field := Field} ->
          io:format("~B\t~B\t~B\t~B\t~B\t~B\n", [
            erlang:system_time(micro_seconds),
            PTS,
            I,
            Seq,
            Field,
            size(Raw)
          ]),
          ok = v4l:queue_buffer(VCapture, video_capture, mmap, I),
          loop(VCapture)
      end
  end.

       




parse_args(Args) ->
  parse_args2(Args, #{}).

parse_args2(["-vi", Device|Args], Opts) ->
  parse_args2(Args, Opts#{video_device => list_to_binary(Device)});

parse_args2(["-ai", Device|Args], Opts) ->
  parse_args2(Args, Opts#{audio_device => list_to_binary(Device)});

parse_args2([], Opts) ->
  Opts.

