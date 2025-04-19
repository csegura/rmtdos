#include "../lib16/video.h"
#include "server/globals.h"


#define video_write_str(x, y, attr, str) \
    video_write_str(video_segment, x, y, attr, str)

#define video_copy_from_frame_buffer(dest, offset, words) \
    video_copy_from_frame_buffer(video_segment, dest, offset, words)

#define video_clear_rows(y1, y2) \
    video_clear_rows(video_segment, y1, y2)

#define video_printf(x, y, attr, fmt, ...)  \
     video_printf(video_segment, x, y, attr, fmt, __VA_ARGS__)

#define VIDEO_WORD (video_segment == 0xB800 ? 2 : 1)

