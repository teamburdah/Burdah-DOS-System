/* $Id$
 *  COLOR.C - COLOR internal command.
 *
 *  Comments:
 *
 *	Sets the console background/foreground text color, in the
 *	style of MS-DOS 6 / Windows CMD.EXE's COLOR command.
 *
 *	Usage: COLOR [bf]
 *	  b = background color (hex digit 0-7)
 *	  f = foreground color (hex digit 0-F)
 *	With no parameter, resets to the default 07 (light gray on
 *	black).
 *
 *	Implementation note: DOS/BIOS text mode has no notion of a
 *	session-wide "current color" the way the Win32 console does.
 *	Instead we repaint the entire visible screen with the chosen
 *	attribute (like CLS does), which is enough for the attribute
 *	to stick for anything written afterwards, since BIOS
 *	teletype output (INT 10h, AH=0Eh) only touches the character
 *	cell, not its attribute byte.
 *
 * 2026 - added for Burdah-DOS [Vibe-Coded by Claude Chat]
 */

#include "../config.h"

#include <dos.h>

#include "../include/command.h"
#include "../include/misc.h"
#include "../err_fcts.h"

static int hexval(char c)
{
  if(c >= '0' && c <= '9') return c - '0';
  if(c >= 'a' && c <= 'f') return c - 'a' + 10;
  if(c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int cmd_color(char *param)
{
  unsigned attr = 0x07;		/* default: light gray on black */
  int mode;
  IREGS r;

  if(param && *param)
  {
    int bg, fg;

    bg = hexval(param[0]);
    fg = (bg >= 0 && param[1]) ? hexval(param[1]) : -1;

    if(bg < 0 || bg > 7 || fg < 0 || param[2])
    {
      error_syntax(param);
      return 1;
    }

    if(bg == fg)
    {
      /* foreground == background would make text invisible */
      error_syntax(param);
      return 1;
    }

    attr = (unsigned)((bg << 4) | fg);
  }

  /* Determine current video mode; only meaningful for text modes */
  r.r_ax = 0x0f00;
  intrpt(0x10, &r);
  mode = r.r_ax & 0x7f;

  switch(mode)
  {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x07:
      break;			/* recognised text modes: proceed */
    default:
      error_syntax(param);	/* graphics mode: nothing sane to do */
      return 1;
  }

  r.r_ax = 0x0600;			/* scroll up / clear entire window */
  r.r_bx = attr << 8;			/* attribute to fill blanked area */
  r.r_cx = 0x0000;			/* upper left corner */
  r.r_dx = ((SCREEN_ROWS - 1) << 8) | (SCREEN_COLS - 1); /* lower right */
  intrpt(0x10, &r);

  goxy(1, 1);				/* home the cursor */

  return 0;
}
