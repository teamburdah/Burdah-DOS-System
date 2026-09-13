/*
 * Burdah DOS Boot Splash Header
 * VGA Mode 13h (320x200 256-color) BMP Splash Screen
 */

#ifndef __SPLASH_H__
#define __SPLASH_H__

#include "portab.h"

int splash_init(void);
void splash_check_abort(void);
void splash_close(void);

#endif /* __SPLASH_H__ */
