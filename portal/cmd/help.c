/* $Id$
 *  HELP.C - HELP internal command.
 *
 *  Comments:
 *
 *	Full-screen, keyboard-scrollable reference table of every internal
 *	command.com command, in the style of a classic DOS TUI. Text mode
 *	has no scrollback, and Burdah has too many internal commands to
 *	fit on one 80x25 screen, so this paints a fixed-size viewport and
 *	lets the user scroll through it with the arrow keys instead of
 *	dumping everything at once.
 *
 *	Layout (1-based rows, 80 columns):
 *	  row 1        title bar
 *	  row 2-3      instructions
 *	  row 4        spacer
 *	  row 5        table top border
 *	  row 6        column headers (No / Command / Purpose)
 *	  row 7        header separator
 *	  row 8-24     scrollable data viewport (17 rows)
 *	  row 25       closing border -- only drawn once the LAST command
 *	               has scrolled into view at row 24, so the table's
 *	               bottom edge naturally appears right after it.
 *
 *	No scrollbar is drawn on purpose (keeps redraws cheap on real
 *	8086/286 hardware) -- the closing border at the end of the list is
 *	the only "you've reached the bottom" indicator, as requested.
 *
 * 2026 - added for Burdah-DOS [Vibe-Coded by Claude Chat]
 */

#include "../config.h"

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/command.h"
#include "../include/misc.h"
#include "../include/keys.h"

#define SCR_COLS        80

#define ROW_TITLE       1
#define ROW_INFO1       2
#define ROW_INFO2       3
#define ROW_SPACER      4
#define ROW_TOPBORDER   5
#define ROW_HEADER      6
#define ROW_HDRBORDER   7
#define ROW_DATA_TOP    8
#define ROW_DATA_BOT    24
#define ROW_BOTBORDER   25
#define VISIBLE_ROWS    (ROW_DATA_BOT - ROW_DATA_TOP + 1)     /* 17 */

#define COL_NO_W        4
#define COL_CMD_W       15
#define COL_PUR_W       (SCR_COLS - 1 - COL_NO_W - 1 - COL_CMD_W - 1 - 1) /* 57 */

#define ATTR_TITLE      0x30   /* black on cyan */
#define ATTR_BODY       0x1F   /* bright white on blue */

/* CP437 single-line box drawing characters */
#define BX_TL   0xDA
#define BX_TR   0xBF
#define BX_BL   0xC0
#define BX_BR   0xD9
#define BX_H    0xC4
#define BX_V    0xB3
#define BX_TT   0xC2    /* top tee    (top border, column split)    */
#define BX_BT   0xC1    /* bottom tee (bottom border, column split) */
#define BX_LT   0xC3    /* left tee   (header separator, left end)  */
#define BX_RT   0xB4    /* right tee  (header separator, right end) */
#define BX_CR   0xC5    /* cross      (header separator, middle)    */

/* ---- low-level screen helpers (BIOS INT 10h only, 8086-safe) ---- */

static void set_cursor(int row, int col)
{
    IREGS r;
    r.r_ax = 0x0200;
    r.r_bx = 0;
    r.r_dx = ((row - 1) << 8) | (col - 1);
    intrpt(0x10, &r);
}

/* Fill an entire row with spaces at the given attribute; leaves the
   cursor at column 1 of that row, ready for teletype output. Teletype
   (AH=0Eh) never touches the attribute byte of a cell, only the
   character, so painting the attribute first and writing text with
   teletype afterwards keeps colors correct without per-char attribute
   calls. */
static void fill_row(int row, unsigned char attr)
{
    IREGS r;
    set_cursor(row, 1);
    r.r_ax = 0x0920;    /* AH=09 write char/attr, AL=' ' */
    r.r_bx = attr;
    r.r_cx = SCR_COLS;
    intrpt(0x10, &r);
}

/* Writes a string starting at (row,col) by explicitly positioning the
   cursor before every character and poking it with INT 10h AH=09h
   (write char+attr, does NOT move the cursor). This is deliberately
   NOT done with teletype (AH=0Eh): writing the very last cell of the
   screen (row 25, col 80) with teletype makes the BIOS advance the
   cursor past the end of the screen, which silently scrolls the
   *entire* screen up by one line -- exactly the cell our closing
   border's bottom-right corner sits on. Per-cell AH=09 writes never
   move the cursor, so they're safe at any position, including the
   last one. */
static void put_text(int row, int col, unsigned char attr, const char *s)
{
    IREGS r;
    int c = col;

    while (*s)
    {
        r.r_ax = 0x0200;
        r.r_bx = 0;
        r.r_dx = ((row - 1) << 8) | (c - 1);
        intrpt(0x10, &r);

        r.r_ax = 0x0900 | (unsigned char)*s++;
        r.r_bx = attr;
        r.r_cx = 1;
        intrpt(0x10, &r);

        c++;
    }
}

/* ---- table drawing ---- */

static void draw_border(int row, unsigned char left, unsigned char mid, unsigned char right)
{
    char buf[SCR_COLS + 1];
    int i, p = 0;

    buf[p++] = left;
    for (i = 0; i < COL_NO_W;  i++) buf[p++] = BX_H;
    buf[p++] = mid;
    for (i = 0; i < COL_CMD_W; i++) buf[p++] = BX_H;
    buf[p++] = mid;
    for (i = 0; i < COL_PUR_W; i++) buf[p++] = BX_H;
    buf[p++] = right;
    buf[p] = '\0';

    fill_row(row, ATTR_BODY);
    put_text(row, 1, ATTR_BODY, buf);
}

static void pad_field(char *dst, const char *src, int width)
{
    int l = src ? strlen(src) : 0;
    if (l > width) l = width;
    memset(dst, ' ', width);
    if (l) memcpy(dst, src, l);
}

static void draw_header(void)
{
    char buf[SCR_COLS + 1];
    char no[COL_NO_W + 1], cmd[COL_CMD_W + 1], pur[COL_PUR_W + 1];
    int p = 0;

    pad_field(no,  "No",      COL_NO_W);
    pad_field(cmd, "Command", COL_CMD_W);
    pad_field(pur, "Purpose", COL_PUR_W);

    buf[p++] = BX_V;
    memcpy(buf + p, no, COL_NO_W);   p += COL_NO_W;
    buf[p++] = BX_V;
    memcpy(buf + p, cmd, COL_CMD_W); p += COL_CMD_W;
    buf[p++] = BX_V;
    memcpy(buf + p, pur, COL_PUR_W); p += COL_PUR_W;
    buf[p++] = BX_V;
    buf[p] = '\0';

    fill_row(ROW_HEADER, ATTR_BODY);
    put_text(ROW_HEADER, 1, ATTR_BODY, buf);
}

/* Grabs just the first line of a command's help text (the one-sentence
   summary every TEXT_CMDHELP_* string starts with) for the Purpose
   column -- reuses the same help text HELP-style commands already
   carry, instead of maintaining a second, separate description table. */
static void get_purpose(unsigned help_id, char *out, int outsz)
{
    char *full, *nl;
    int len;

    out[0] = '\0';
    full = getString(help_id);
    if (!full)
        return;

    nl = strchr(full, '\n');
    len = nl ? (int)(nl - full) : (int)strlen(full);
    if (len >= outsz)
        len = outsz - 1;
    memcpy(out, full, len);
    out[len] = '\0';

    free(full);
}

static void draw_data_row(int row, int idx, int total)
{
    char buf[SCR_COLS + 1];
    char no[COL_NO_W + 1], cmd[COL_CMD_W + 1], pur[COL_PUR_W + 1];
    char numtxt[COL_NO_W + 1];
    char purtxt[128];
    int p = 0;

    if (idx < total)
    {
        sprintf(numtxt, "%d", idx + 1);
        pad_field(no, numtxt, COL_NO_W);
        pad_field(cmd, internalCommands[idx].name, COL_CMD_W);
        get_purpose(internalCommands[idx].help_id, purtxt, sizeof(purtxt));
        pad_field(pur, purtxt, COL_PUR_W);
    }
    else
    {
        pad_field(no,  "", COL_NO_W);
        pad_field(cmd, "", COL_CMD_W);
        pad_field(pur, "", COL_PUR_W);
    }

    buf[p++] = BX_V;
    memcpy(buf + p, no, COL_NO_W);   p += COL_NO_W;
    buf[p++] = BX_V;
    memcpy(buf + p, cmd, COL_CMD_W); p += COL_CMD_W;
    buf[p++] = BX_V;
    memcpy(buf + p, pur, COL_PUR_W); p += COL_PUR_W;
    buf[p++] = BX_V;
    buf[p] = '\0';

    fill_row(row, ATTR_BODY);
    put_text(row, 1, ATTR_BODY, buf);
}

static int count_commands(void)
{
    int n = 0;
    while (internalCommands[n].name)
        n++;
    return n;
}

int cmd_help(char *param)
{
    int total = count_commands();
    int max_top = total - VISIBLE_ROWS;
    int top = 0;
    int i, key;

    (void)param;

    if (max_top < 0)
        max_top = 0;

    fill_row(ROW_TITLE, ATTR_TITLE);
    put_text(ROW_TITLE, 2, ATTR_TITLE, "Burdah DOS System Ver 0.5 (Portal ver 1.05)");

    fill_row(ROW_INFO1, ATTR_BODY);
    put_text(ROW_INFO1, 2, ATTR_BODY,
        "This is a table for internal command.com (Burdah Portal) commands. Press");
    fill_row(ROW_INFO2, ATTR_BODY);
    put_text(ROW_INFO2, 2, ATTR_BODY,
        "arrow keys UP/DOWN for scroll all commands. Or press ESC to exit");

    fill_row(ROW_SPACER, ATTR_BODY);

    draw_border(ROW_TOPBORDER, BX_TL, BX_TT, BX_TR);
    draw_header();
    draw_border(ROW_HDRBORDER, BX_LT, BX_CR, BX_RT);

    for (;;)
    {
        for (i = 0; i < VISIBLE_ROWS; i++)
            draw_data_row(ROW_DATA_TOP + i, top + i, total);

        if (top >= max_top)
            draw_border(ROW_BOTBORDER, BX_BL, BX_BT, BX_BR);
        else
            fill_row(ROW_BOTBORDER, ATTR_BODY);

        key = cgetchar();

        if (key == KEY_ESC || key == 'x' || key == 'X')
            break;
        else if (key == KEY_UP)
        {
            if (top > 0) top--;
        }
        else if (key == KEY_DOWN)
        {
            if (top < max_top) top++;
        }
        /* other keys ignored */
    }

    /* leave the screen clean for the shell prompt */
    {
        IREGS r;
        r.r_ax = 0x0600;
        r.r_bx = 0x0700;               /* light gray on black */
        r.r_cx = 0x0000;
        r.r_dx = ((SCREEN_ROWS - 1) << 8) | (SCREEN_COLS - 1);
        intrpt(0x10, &r);
    }
    set_cursor(1, 1);

    return 0;
}
