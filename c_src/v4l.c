// Author: Max Lapshin <max@maxidoors.ru>
//
// License: BSD
//
#include <stdint.h>
#include <erl_nif.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include "videodev2.h"


#define ATOM(name) atm_##name
#define DECL_ATOM(name) ERL_NIF_TERM atm_##name = 0
#define LOAD_ATOM(name) atm_##name = enif_make_atom(env,#name)


#define v4l2_open open
#define v4l2_ioctl ioctl


#define ATOM_DECLARE(XX)  \
 XX(ok) \
 XX(true) \
 XX(error) \
 XX(eagain) \
 XX(badarg) \
 XX(undefined) \
 XX(input) \
 XX(output) \
 XX(open) \
 XX(ioctl) \
 XX(driver) \
 XX(card) \
 XX(bus_info) \
 XX(version) \
 XX(capabilities) \
 XX(device_caps) \
 XX(video_capture) \
 XX(video_output) \
 XX(video_overlay) \
 XX(vbi_capture) \
 XX(vbi_output) \
 XX(sliced_vbi_capture) \
 XX(sliced_vbi_output) \
 XX(rds_capture) \
 XX(video_output_overlay) \
 XX(hw_freq_seek) \
 XX(rds_output) \
 XX(video_capture_mplane) \
 XX(video_output_mplane) \
 XX(video_m2m_plane) \
 XX(video_m2m) \
 XX(tuner) \
 XX(audio) \
 XX(radio) \
 XX(modulator) \
 XX(sdr_capture) \
 XX(ext_pix_format) \
 XX(sdr_output) \
 XX(meta_capture) \
 XX(readwrite) \
 XX(asyncio) \
 XX(streaming) \
 XX(touch) \
 XX(no_power) \
 XX(no_signal) \
 XX(no_color) \
 XX(hflip) \
 XX(vflip) \
 XX(no_sync) \
 XX(no_equ) \
 XX(no_carrier) \
 XX(std) \
 XX(native_size) \
 XX(index) \
 XX(name) \
 XX(type) \
 XX(audioset) \
 XX(status) \
 XX(camera) \
 XX(analog_tv) \
 XX(digital_tv) \
 XX(sdr) \
 XX(rf) \
 XX(dv_timings) \
 XX(width) \
 XX(height) \
 XX(pixelformat) \
 XX(field) \
 XX(bytesperline) \
 XX(mmap) \
 XX(userptr) \
 XX(dmabuf) \
 XX(pts) \
 XX(body) \
 XX(fps_den) \
 XX(fps_num) \
 XX(capability) \
 XX(capturemode) \
 XX(extendedmode) \
 XX(readbuffers) \
 XX(cea861) \
 XX(dmt) \
 XX(cvt) \
 XX(gtf) \
 XX(sdi) \
 XX(min_width) \
 XX(max_width) \
 XX(min_height) \
 XX(max_height) \
 XX(min_pixelclock) \
 XX(max_pixelclock) \
 XX(interlaced) \
 XX(progressive) \
 XX(reduced_blanking) \
 XX(custom) \
 XX(standards) \
 XX(bounds) \
 XX(defrect) \
 XX(pixelaspect) \
 XX(arg0) \
 XX(arg1) \
 XX(arg2) \
 XX(arg3)

#define XX(name) DECL_ATOM(name);
ATOM_DECLARE(XX)
#undef XX


#define ERROR(a) enif_make_tuple2(env, ATOM(error), ATOM(a));

ErlNifResourceType *v4l_resource;

// libv4l2 synx mutex
pthread_mutex_t initializer_mutex = PTHREAD_MUTEX_INITIALIZER;


#define MAX_BUFFERS 256

typedef struct v4l_ctx {
  int fd;
  int working;
  ErlNifPid owner;
  int buffers;
  void *buffer_starts[MAX_BUFFERS];
  ssize_t buffer_sizes[MAX_BUFFERS];
} v4l_ctx;


static int
reload(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info)
{
  return 0;
}

static int
upgrade(ErlNifEnv* env, void** priv, void** old_priv,  ERL_NIF_TERM load_info)
{
  return 0;
}

static void
unload(ErlNifEnv* env, void* priv)
{
  // TODO deinit
  return;
}

static ERL_NIF_TERM
v4l_nif_loaded(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  return ATOM(true);
}


static void
v4l_down_callback(ErlNifEnv* env, void* obj, ErlNifPid* pid, ErlNifMonitor* mon)
{
  v4l_ctx *ctx = (v4l_ctx *)obj;
  if(ctx->fd> 0) {
    enif_select(env, (ErlNifEvent)ctx->fd, ERL_NIF_SELECT_STOP, ctx, NULL, ATOM(undefined));
  }
  ctx->working = 0;
}

static void
v4l_stop_callback(ErlNifEnv *env, void *obj, ErlNifEvent event, int is_direct_call)
{
  v4l_ctx *ctx = (v4l_ctx *)obj;
  if(ctx->fd == event) {
    close(ctx->fd);
    ctx->fd = -1;
  }
}

static void
v4l_destructor(ErlNifEnv *env, void *obj)
{
  v4l_ctx *ctx = (v4l_ctx *)obj;
  int i;
  for(i = 0; i < ctx->buffers; i++)
    munmap(ctx->buffer_starts[i], ctx->buffer_sizes[i]);
}

static ErlNifResourceTypeInit v4l_callbacks = {
  v4l_destructor,
  v4l_stop_callback,
  v4l_down_callback
};


static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info)
{
#define XX(name) LOAD_ATOM(name);
ATOM_DECLARE(XX)
#undef XX
	

  ErlNifResourceFlags flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  if(!v4l_resource) {
    v4l_resource = enif_open_resource_type_x(env, "v4l_resource", &v4l_callbacks, flags, NULL);
  }

  return 0;
}





static ERL_NIF_TERM
v4l_nif_open0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  char path[1024] = {0};
  
  if(argc < 1)
    return ERROR(badarg);

  if (!enif_get_string(env, argv[0], path, sizeof(path), ERL_NIF_LATIN1))
    return ERROR(arg0);
  

  pthread_mutex_lock(&initializer_mutex);
  int fd;
  if ((fd = v4l2_open(path, O_NONBLOCK | O_RDWR)) == -1) {
    pthread_mutex_unlock(&initializer_mutex);
    return ERROR(open);
  }
  pthread_mutex_unlock(&initializer_mutex);
  v4l_ctx *ctx = (v4l_ctx *)enif_alloc_resource(v4l_resource, sizeof(*ctx));
  memset(ctx, 0, sizeof(*ctx));
  enif_self(env, &ctx->owner);
  enif_monitor_process(env, ctx, &ctx->owner, NULL);
  ctx->working = 1;
  ctx->fd = fd;

  ERL_NIF_TERM ctx_term = enif_make_resource(env, ctx);
  enif_release_resource(ctx);

  return enif_make_tuple2(env, ATOM(ok), ctx_term);  
}


static ERL_NIF_TERM
fill_capabilities(ErlNifEnv* env, uint32_t caps)
{
  ERL_NIF_TERM term = enif_make_list(env, 0);
   if(caps & V4L2_CAP_VIDEO_CAPTURE)
     term = enif_make_list_cell(env, ATOM(video_capture), term);
   if(caps & V4L2_CAP_VIDEO_OUTPUT)
     term = enif_make_list_cell(env, ATOM(video_output), term);
   if(caps & V4L2_CAP_VIDEO_OVERLAY)
     term = enif_make_list_cell(env, ATOM(video_overlay), term);
   if(caps & V4L2_CAP_VBI_CAPTURE)
     term = enif_make_list_cell(env, ATOM(vbi_capture), term);
   if(caps & V4L2_CAP_VBI_OUTPUT)
     term = enif_make_list_cell(env, ATOM(vbi_output), term);
   if(caps & V4L2_CAP_SLICED_VBI_CAPTURE)
     term = enif_make_list_cell(env, ATOM(sliced_vbi_capture), term);
   if(caps & V4L2_CAP_SLICED_VBI_OUTPUT)
     term = enif_make_list_cell(env, ATOM(sliced_vbi_output), term);
   if(caps & V4L2_CAP_RDS_CAPTURE)
     term = enif_make_list_cell(env, ATOM(rds_capture), term);
   if(caps & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
     term = enif_make_list_cell(env, ATOM(video_output_overlay), term);
   if(caps & V4L2_CAP_HW_FREQ_SEEK)
     term = enif_make_list_cell(env, ATOM(hw_freq_seek), term);
   if(caps & V4L2_CAP_RDS_OUTPUT)
     term = enif_make_list_cell(env, ATOM(rds_output), term);

   if(caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
     term = enif_make_list_cell(env, ATOM(video_capture_mplane), term);
   if(caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
     term = enif_make_list_cell(env, ATOM(video_output_mplane), term);
   if(caps & V4L2_CAP_VIDEO_M2M_MPLANE)
     term = enif_make_list_cell(env, ATOM(video_m2m_plane), term);
   if(caps & V4L2_CAP_VIDEO_M2M)
     term = enif_make_list_cell(env, ATOM(video_m2m), term);

   if(caps & V4L2_CAP_TUNER)
     term = enif_make_list_cell(env, ATOM(tuner), term);
   if(caps & V4L2_CAP_AUDIO)
     term = enif_make_list_cell(env, ATOM(audio), term);
   if(caps & V4L2_CAP_RADIO)
     term = enif_make_list_cell(env, ATOM(radio), term);
   if(caps & V4L2_CAP_MODULATOR)
     term = enif_make_list_cell(env, ATOM(modulator), term);
   if(caps & V4L2_CAP_SDR_CAPTURE)
     term = enif_make_list_cell(env, ATOM(sdr_capture), term);
   if(caps & V4L2_CAP_EXT_PIX_FORMAT)
     term = enif_make_list_cell(env, ATOM(ext_pix_format), term);
   if(caps & V4L2_CAP_SDR_OUTPUT)
     term = enif_make_list_cell(env, ATOM(sdr_output), term);
   if(caps & V4L2_CAP_META_CAPTURE)
     term = enif_make_list_cell(env, ATOM(meta_capture), term);
   if(caps & V4L2_CAP_READWRITE)
     term = enif_make_list_cell(env, ATOM(readwrite), term);
   if(caps & V4L2_CAP_ASYNCIO)
     term = enif_make_list_cell(env, ATOM(asyncio), term);
   if(caps & V4L2_CAP_STREAMING)
     term = enif_make_list_cell(env, ATOM(streaming), term);
   if(caps & V4L2_CAP_TOUCH)
     term = enif_make_list_cell(env, ATOM(touch), term);
   if(caps & V4L2_CAP_DEVICE_CAPS)
     term = enif_make_list_cell(env, ATOM(device_caps), term);

  return term;
}


static ERL_NIF_TERM
fill_v4l2_standard(ErlNifEnv* env, v4l2_std_id id)
{
  // FIXME
  //ERL_NIF_TERM term = enif_make_list(env, 0);
  return enif_make_uint64(env, (uint64_t)id); 
}

static ERL_NIF_TERM
fill_input_status(ErlNifEnv* env, uint32_t st)
{ 
  ERL_NIF_TERM term = enif_make_list(env, 0);
  if(st & V4L2_IN_ST_NO_POWER)
    term = enif_make_list_cell(env, ATOM(no_power), term);
  if(st & V4L2_IN_ST_NO_SIGNAL)
    term = enif_make_list_cell(env, ATOM(no_signal), term);
  if(st & V4L2_IN_ST_NO_COLOR)
    term = enif_make_list_cell(env, ATOM(no_color), term);
  if(st & V4L2_IN_ST_HFLIP)
    term = enif_make_list_cell(env, ATOM(hflip), term);
  if(st & V4L2_IN_ST_VFLIP)
    term = enif_make_list_cell(env, ATOM(vflip), term);
  if(st & V4L2_IN_ST_NO_SYNC)
    term = enif_make_list_cell(env, ATOM(no_sync), term);
  if(st & V4L2_IN_ST_NO_EQU)
    term = enif_make_list_cell(env, ATOM(no_equ), term);
  if(st & V4L2_IN_ST_NO_CARRIER)
    term = enif_make_list_cell(env, ATOM(no_carrier), term);
  return term;
}


static ERL_NIF_TERM
fill_input_capabilities(ErlNifEnv* env, uint32_t caps)
{
  ERL_NIF_TERM term = enif_make_list(env, 0);
  if(caps & V4L2_IN_CAP_DV_TIMINGS || caps & V4L2_IN_CAP_CUSTOM_TIMINGS)
    term = enif_make_list_cell(env, ATOM(dv_timings), term);
  if(caps & V4L2_IN_CAP_STD)
    term = enif_make_list_cell(env, ATOM(std), term);
  if(caps & V4L2_IN_CAP_NATIVE_SIZE)
    term = enif_make_list_cell(env, ATOM(native_size), term);
  return term;
}

static ERL_NIF_TERM
v4l_nif_querycap0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  struct v4l2_capability caps;
  int ret;
  v4l_ctx *ctx;

  if(argc < 1)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_QUERYCAP, &caps)) != 0)
    return ERROR(ioctl);

  ERL_NIF_TERM keys[] = {
    ATOM(driver),
    ATOM(card),
    ATOM(bus_info),
    ATOM(version),
    ATOM(capabilities),
    ATOM(device_caps)
  };

  ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
    enif_make_string(env, (char *)caps.driver, ERL_NIF_LATIN1),
    enif_make_string(env, (char *)caps.card, ERL_NIF_LATIN1),
    enif_make_string(env, (char *)caps.bus_info, ERL_NIF_LATIN1),
    enif_make_ulong(env, (unsigned long)caps.version),
    fill_capabilities(env, caps.capabilities),
    fill_capabilities(env, caps.device_caps)
  };

  ERL_NIF_TERM reply;
  if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
    return ERROR(error);
  return reply;
}


static ERL_NIF_TERM
v4l_nif_cropcap0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  struct v4l2_cropcap caps;
  int ret;
  v4l_ctx *ctx;

  if(argc < 2)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  if(argv[1] == ATOM(video_capture)) {
    caps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else {
    return ERROR(arg1);
  }


  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_CROPCAP, &caps)) != 0)
    return ERROR(ioctl);

  ERL_NIF_TERM keys[] = {
    ATOM(bounds),
    ATOM(defrect),
    ATOM(pixelaspect)
  };

  ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
    enif_make_tuple4(env,
      enif_make_uint(env, caps.bounds.left),
      enif_make_uint(env, caps.bounds.top),
      enif_make_uint(env, caps.bounds.width),
      enif_make_uint(env, caps.bounds.height)),
    enif_make_tuple4(env,
      enif_make_uint(env, caps.defrect.left),
      enif_make_uint(env, caps.defrect.top),
      enif_make_uint(env, caps.defrect.width),
      enif_make_uint(env, caps.defrect.height)),
    enif_make_tuple2(env,
      enif_make_uint(env, caps.pixelaspect.numerator),
      enif_make_uint(env, caps.pixelaspect.denominator))
  };

  ERL_NIF_TERM reply;
  if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
    return ERROR(error);
  return reply;
}


static ERL_NIF_TERM
fill_timings_standards(ErlNifEnv* env, uint32_t st)
{
  ERL_NIF_TERM term = enif_make_list(env, 0);
  if(st & V4L2_DV_BT_STD_CEA861)
    term = enif_make_list_cell(env, ATOM(cea861), term);
  if(st & V4L2_DV_BT_STD_DMT)
    term = enif_make_list_cell(env, ATOM(dmt), term);
  if(st & V4L2_DV_BT_STD_CVT)
    term = enif_make_list_cell(env, ATOM(cvt), term);
  if(st & V4L2_DV_BT_STD_GTF)
    term = enif_make_list_cell(env, ATOM(gtf), term);
  if(st & V4L2_DV_BT_STD_SDI)
    term = enif_make_list_cell(env, ATOM(sdi), term);
  return term;
}

static ERL_NIF_TERM
fill_timings_capabilities(ErlNifEnv* env, uint32_t st)
{ 
  ERL_NIF_TERM term = enif_make_list(env, 0);
  if(st & V4L2_DV_BT_CAP_INTERLACED)
    term = enif_make_list_cell(env, ATOM(interlaced), term);
  if(st & V4L2_DV_BT_CAP_PROGRESSIVE)
    term = enif_make_list_cell(env, ATOM(progressive), term);
  if(st & V4L2_DV_BT_CAP_REDUCED_BLANKING)
    term = enif_make_list_cell(env, ATOM(reduced_blanking), term);
  if(st & V4L2_DV_BT_CAP_CUSTOM)
    term = enif_make_list_cell(env, ATOM(custom), term);
  return term;
}


static ERL_NIF_TERM
v4l_nif_dv_timings_cap0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{ 
  struct v4l2_dv_timings_cap caps;
  int ret;
  v4l_ctx *ctx;
  
  if(argc < 1)
    return ERROR(badarg);
  
  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);
  
  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_DV_TIMINGS_CAP, &caps)) != 0)
    return ERROR(ioctl);
  
  
  ERL_NIF_TERM keys[] = {
    ATOM(min_width),
    ATOM(max_width),
    ATOM(min_height),
    ATOM(max_height),
    ATOM(min_pixelclock),
    ATOM(max_pixelclock),
    ATOM(standards),
    ATOM(capabilities)
  };

  ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
    enif_make_uint(env, (unsigned int)caps.bt.min_width),
    enif_make_uint(env, (unsigned int)caps.bt.max_width),
    enif_make_uint(env, (unsigned int)caps.bt.min_height),
    enif_make_uint(env, (unsigned int)caps.bt.max_height),
    enif_make_uint(env, (unsigned int)caps.bt.min_pixelclock),
    enif_make_uint(env, (unsigned int)caps.bt.max_pixelclock),
    fill_timings_standards(env, caps.bt.standards),
    fill_timings_capabilities(env, caps.bt.capabilities),
  };

  ERL_NIF_TERM reply;
  if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
    return ERROR(error);
  return reply;

}



static ERL_NIF_TERM
v4l_nif_g_input0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{ 
  int ret;
  v4l_ctx *ctx;
  
  if(argc < 1)
    return ERROR(badarg);
  
  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);
  
  int input_count = 0;
  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_G_INPUT, &input_count)) != 0)
    return ERROR(ioctl);
  
  return enif_make_int(env, input_count);
}




static ERL_NIF_TERM
v4l_nif_enuminput0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  int ret;
  int device_index;
  v4l_ctx *ctx;

  if(argc < 2)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  if (!enif_get_int(env, argv[1], &device_index))
    return ERROR(arg1);

  struct v4l2_input input;
  memset(&input, 0, sizeof(input));
  
  input.index = device_index;

  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_ENUMINPUT, &input)) != 0)
    return ERROR(ioctl);

  ERL_NIF_TERM keys[] = {
    ATOM(index),
    ATOM(name),
    ATOM(type),
    ATOM(audioset),
    ATOM(tuner),
    ATOM(std),
    ATOM(status),
    ATOM(capabilities)
  };

  ERL_NIF_TERM type_term =
    input.type == V4L2_INPUT_TYPE_TUNER ? ATOM(tuner) :
    input.type == V4L2_INPUT_TYPE_CAMERA ? ATOM(camera) :
    input.type == V4L2_INPUT_TYPE_TOUCH ? ATOM(touch) :
    ATOM(undefined);

  ERL_NIF_TERM tuner_type_term =
    input.tuner == V4L2_TUNER_RADIO ? ATOM(radio) :
    input.tuner == V4L2_TUNER_ANALOG_TV ? ATOM(analog_tv) :
    input.tuner == V4L2_TUNER_DIGITAL_TV ? ATOM(digital_tv) :
    input.tuner == V4L2_TUNER_SDR ? ATOM(sdr) :
    input.tuner == V4L2_TUNER_RF ? ATOM(rf) :
    ATOM(undefined);


  ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
    enif_make_ulong(env, (unsigned long)input.index),
    enif_make_string(env, (char *)input.name, ERL_NIF_LATIN1),
    type_term,
    enif_make_ulong(env, (unsigned long)input.audioset),
    tuner_type_term,
    fill_v4l2_standard(env, input.std),
    fill_input_status(env, input.status),
    fill_input_capabilities(env, input.capabilities)
  };

  ERL_NIF_TERM reply;
  if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
    return ERROR(error);
  return reply;

}



static ERL_NIF_TERM
v4l_nif_get_parm0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  int ret;
  v4l_ctx *ctx;

  if(argc < 2)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  struct v4l2_streamparm parm;
  memset(&parm, 0, sizeof(parm));
  if(argv[1] == ATOM(video_capture)) {
   parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else {
    return ERROR(arg1);
  }

  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_G_PARM, &parm)) != 0)
    return ERROR(ioctl);

  if(argv[1] == ATOM(video_capture)) {
    ERL_NIF_TERM keys[] = {
      ATOM(fps_den),
      ATOM(fps_num),
      ATOM(capability),
      ATOM(capturemode),
      ATOM(extendedmode),
      ATOM(readbuffers)
    };
    ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
      enif_make_uint(env, (unsigned int)parm.parm.capture.timeperframe.numerator),
      enif_make_uint(env, (unsigned int)parm.parm.capture.timeperframe.denominator),
      enif_make_uint(env, (unsigned int)parm.parm.capture.capability),
      enif_make_uint(env, (unsigned int)parm.parm.capture.capturemode),
      enif_make_uint(env, (unsigned int)parm.parm.capture.extendedmode),
      enif_make_uint(env, (unsigned int)parm.parm.capture.readbuffers)
    };

    ERL_NIF_TERM reply;
    if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
      return ERROR(error);
    return reply;
  }

  return ATOM(undefined);
}



static ERL_NIF_TERM
v4l_nif_get_format0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  int ret;
  v4l_ctx *ctx;

  if(argc < 2)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  if(argv[1] == ATOM(video_capture)) {
   format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else {
    return ERROR(arg1);
  }

  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_G_FMT, &format)) != 0)
    return ERROR(ioctl);

  if(argv[1] == ATOM(video_capture)) {
    ERL_NIF_TERM keys[] = {
      ATOM(width),
      ATOM(height),
      ATOM(pixelformat),
      ATOM(field),
      ATOM(bytesperline)
    };
    uint8_t fourcc[] = {
      format.fmt.pix.pixelformat >> 0,
      format.fmt.pix.pixelformat >> 8,
      format.fmt.pix.pixelformat >> 16,
      format.fmt.pix.pixelformat >> 24,
      0
    };
    ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
      enif_make_uint(env, (unsigned int)format.fmt.pix.width),
      enif_make_uint(env, (unsigned int)format.fmt.pix.height),
      enif_make_atom(env, (char *)fourcc),
      enif_make_uint(env, (unsigned int)format.fmt.pix.field),
      enif_make_uint(env, (unsigned int)format.fmt.pix.bytesperline)
    };

    ERL_NIF_TERM reply;
    if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
      return ERROR(error);
    return reply;
  }

  return ATOM(undefined);
}


static ERL_NIF_TERM
v4l_nif_request_buffers0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  int ret;
  v4l_ctx *ctx;

  if(argc < 4)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  if(argv[1] == ATOM(video_capture)) {
   req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else {
    return ERROR(arg1);
  }
  if(argv[2] == ATOM(mmap)) {
    req.memory = V4L2_MEMORY_MMAP;
  } else if(argv[2] == ATOM(userptr)) {
    req.memory = V4L2_MEMORY_USERPTR;
  } else if(argv[2] == ATOM(dmabuf)) {
    req.memory = V4L2_MEMORY_DMABUF;
  } else {
    return ERROR(arg2);
  }

  unsigned int req_count;
  if(!enif_get_uint(env, argv[3], &req_count))
    return ERROR(arg3);
  req.count = (uint32_t)req_count;

  if ((ret = v4l2_ioctl(ctx->fd, VIDIOC_REQBUFS, &req)) != 0)
    return ERROR(ioctl);

  if (req.memory == V4L2_MEMORY_MMAP) {
    ctx->buffers = req.count;
    int j;
    for(int i = 0; i < req.count && i < MAX_BUFFERS; i++) {
      struct v4l2_buffer buf = {
            .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .index  = i,
            .memory = V4L2_MEMORY_MMAP
      };      
      if (v4l2_ioctl(ctx->fd, VIDIOC_QUERYBUF, &buf) < 0) {
        for(j = 0; j < i ; j++)
          munmap(ctx->buffer_starts[j], ctx->buffer_sizes[j]);
        return ERROR(ioctl);
      }
      ctx->buffer_sizes[i] = buf.length;
      ctx->buffer_starts[i] = mmap(NULL, buf.length,
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                ctx->fd, buf.m.offset);
      if (ctx->buffer_starts[i] == MAP_FAILED) {
        for(j = 0; j < i ; j++)
          munmap(ctx->buffer_starts[j], ctx->buffer_sizes[j]);
        return ERROR(mmap);
      }
    }
  }

  return enif_make_tuple2(env, ATOM(ok), enif_make_uint(env, req.count));
}



static ERL_NIF_TERM
v4l_nif_queue_buffer0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  v4l_ctx *ctx;

  if(argc < 4)
    return ERROR(badarg);

  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  struct v4l2_buffer buf = {0};
  if(argv[1] == ATOM(video_capture)) {
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else {
    return ERROR(arg1);
  }
  if(argv[2] == ATOM(mmap)) {
    buf.memory = V4L2_MEMORY_MMAP;
  } else {
    return ERROR(arg2);
  }

  if(!enif_get_uint(env, argv[3], &buf.index)) 
    return ERROR(arg3);

  if (v4l2_ioctl(ctx->fd, VIDIOC_QBUF, &buf) < 0) 
    return ERROR(ioctl)
  
  return ATOM(ok);
}



static ERL_NIF_TERM
v4l_nif_streamon0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{ 
  v4l_ctx *ctx;
  
  if(argc < 2)
    return ERROR(badarg);
  
  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  enum v4l2_buf_type type;
  if(argv[1] == ATOM(video_capture)) {
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else { 
    return ERROR(arg1);
  }

  if (v4l2_ioctl(ctx->fd, VIDIOC_STREAMON, &type) < 0)
    return ERROR(ioctl);

  return ATOM(ok);
}


static ERL_NIF_TERM
v4l_nif_select0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{ 
  v4l_ctx *ctx;
  
  if(argc < 1)
    return ERROR(badarg);
  
  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);

  ERL_NIF_TERM ref;
  ref = enif_make_ref(env);

  if (enif_select(env, (ErlNifEvent)ctx->fd, ERL_NIF_SELECT_READ, ctx, NULL, ref) < 0) {
    return ERROR(ioctl);
  }

  return enif_make_tuple2(env, ATOM(ok), ref);
}




static ERL_NIF_TERM
v4l_nif_dequeue_buffer0(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{ 
  v4l_ctx *ctx;
  
  if(argc < 3)
    return ERROR(badarg);
  
  if (!enif_get_resource(env, argv[0], v4l_resource, (void **) &ctx))
    return ERROR(arg0);
  
  struct v4l2_buffer buf = {0};
  if(argv[1] == ATOM(video_capture)) {
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  } else { 
    return ERROR(arg1);
  }
  if(argv[2] == ATOM(mmap)) {
    buf.memory = V4L2_MEMORY_MMAP;
  } else { 
    return ERROR(arg2);
  }
  
  if (v4l2_ioctl(ctx->fd, VIDIOC_DQBUF, &buf) < 0)
    return ERROR(ioctl);

  ERL_NIF_TERM keys[] = {
    ATOM(index),
    ATOM(pts),
    ATOM(body)
  };


  ErlNifBinary body;
  enif_alloc_binary(ctx->buffer_sizes[buf.index], &body);
  memcpy(body.data, ctx->buffer_starts[buf.index], ctx->buffer_sizes[buf.index]);

  ERL_NIF_TERM values[sizeof(keys)/sizeof(*keys)] = {
    enif_make_uint(env, buf.index),
    enif_make_uint64(env, (uint64_t)buf.timestamp.tv_sec * 1000000 + (uint64_t)buf.timestamp.tv_usec),
    enif_make_binary(env, &body)
  };

  ERL_NIF_TERM reply;
  if(!enif_make_map_from_arrays(env, keys, values, sizeof(keys)/sizeof(*keys), &reply))
    return ERROR(error);
  return reply;
}







static ErlNifFunc v4l_funcs[] = {
  {"open0"                   , 1, v4l_nif_open0},
  {"querycap0"               , 1, v4l_nif_querycap0},
  {"dv_timings_cap"          , 1, v4l_nif_dv_timings_cap0},
  {"cropcap0"                , 2, v4l_nif_cropcap0},
  {"g_input0"                , 1, v4l_nif_g_input0},
  {"enuminput0"              , 2, v4l_nif_enuminput0},
  {"get_format0"             , 2, v4l_nif_get_format0},
  {"get_parm0"               , 2, v4l_nif_get_parm0},
  {"request_buffers0"        , 4, v4l_nif_request_buffers0},
  {"qbuf0"                   , 4, v4l_nif_queue_buffer0},
  {"dqbuf0"                  , 3, v4l_nif_dequeue_buffer0},
  {"streamon0"               , 2, v4l_nif_streamon0},
  {"select0"                 , 1, v4l_nif_select0},
  {"nif_loaded"              , 0, v4l_nif_loaded}
};

ERL_NIF_INIT(v4l, v4l_funcs, load, reload, upgrade, unload)
