#ifndef DETECT_VIDEO_H
#define DETECT_VIDEO_H

#include "sysinfo.h"

/* Mengisi `out` dengan jenis adapter video (MDA/CGA/EGA/VGA) via
 * INT 10h. Resolusi monitor asli (VESA/EDID) masih TODO fase 2. */
void detect_video(video_info_t *out);

#endif /* DETECT_VIDEO_H */
