/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#define AUTHORS "Anish S Kumar"


/* macros */
#define INRECT(X,Y,RX,RY,RW,RH) ((X) >= (RX) && (X) < (RX) + (RW) && (Y) >= (RY) && (Y) < (RY) + (RH))
#define MIN(a, b)               ((a) < (b) ? (a) : (b))

/* enums */
enum { ColFG, ColBG, ColLast };

/* typedefs */
typedef struct {
	int x, y, w, h;
	unsigned long norm[ColLast];
	unsigned long sel[ColLast];
	Drawable drawable;
	GC gc;
	struct {
		XFontStruct *xfont;
		XFontSet set;
		int ascent;
		int descent;
		int max_rbearing;
		int min_lbearing;
		int height;
		int width;
	} font;
} DC; /* draw context */

/* forward declarations */
static void drawmenu(char* t);
static void drawtext(const char *text, unsigned long col[ColLast]);
static void eprint(const char *errstr, ...);
static unsigned long getcolor(const char *colstr);
static Bool grabkeyboard(void);
static void initfont(const char *fontstr);
static int kpress(XKeyEvent * e);
static void run(void);
static void setup(Bool topbar);
static int textnw(const char *text, unsigned int len);

#include "config.h"

/* variables */
static char text[4096];
static int ret = 0;
static int screen;
static unsigned int mw, mh;
static unsigned int numlockmask = 0;
static Bool running = True;
static Display *dpy;
static DC dc;
static Window root, win;


/*Display the text passed as argument*/
void drawmenu(char *t) {
	dc.x = 0;
	dc.y = 0;
	dc.w = mw;
	dc.h = mh;
	drawtext(t,dc.norm);
	XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, mw, mh, 0, 0);
	XFlush(dpy);
}

/*Draw the text on the bar*/
void drawtext(const char *text, unsigned long col[ColLast]) {
	char buf[256];
	int i, x, y, h, len, olen;
	XRectangle r = { dc.x, dc.y, dc.w, dc.h };

	XSetForeground(dpy, dc.gc, col[ColBG]);
	XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	if(!text)
		return;
	olen = strlen(text);
	h = dc.font.ascent + dc.font.descent;
	y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
	/*The alignment is set here*/
	if(align == 0)
		x = dc.y + (h/2);
	else if(align == 1)
		x = dc.w/2 - (dc.font.width*olen)/2;
	else if(align == 2)
		x = dc.w - (dc.font.width*olen) - (h/2);
	else
		x = dc.w/2 - (dc.font.width*olen)/2;
	/* shorten text if necessary */
	for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);
	if(!len)
		return;
	memcpy(buf, text, len);
	if(len < olen)
		for(i = len; i && i > len - 3; buf[--i] = '.');
	XSetForeground(dpy, dc.gc, col[ColFG]);
	if(dc.font.set)
		XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
	else
		XDrawString(dpy, dc.drawable, dc.gc, x, y, buf, len);
}

void eprint(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

unsigned long getcolor(const char *colstr)
{
	Colormap cmap = DefaultColormap(dpy, screen);
	XColor color;

	if(!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
		eprint("error, cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

Bool grabkeyboard(void)
{
	unsigned int len;

	for(len = 1000; len; len--) {
		if(XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
				== GrabSuccess)
			break;
		usleep(1000);
	}
	return len > 0;
}

void initfont(const char *fontstr)
{
	char *def, **missing;
	int i, n;

	if(!fontstr || fontstr[0] == '\0')
		eprint("error, cannot load font: '%s'\n", fontstr);
	missing = NULL;
	dc.font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def);
	if(missing)
		XFreeStringList(missing);
	if(dc.font.set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;
		dc.font.ascent = dc.font.descent = dc.font.max_rbearing = dc.font.min_lbearing = 0;
		font_extents = XExtentsOfFontSet(dc.font.set);
		n = XFontsOfFontSet(dc.font.set, &xfonts, &font_names);
		for(i = 0, dc.font.ascent = 0, dc.font.descent = 0; i < n; i++) {
			if(dc.font.ascent < (*xfonts)->ascent)
				dc.font.ascent = (*xfonts)->ascent;
			if(dc.font.descent < (*xfonts)->descent)
				dc.font.descent = (*xfonts)->descent;
			/*Initialized lbearing, rbearing variables to set the alignment properly*/
			if(dc.font.max_rbearing < (*xfonts)->max_bounds.rbearing)
				dc.font.max_rbearing = (*xfonts)->max_bounds.rbearing;
			if(dc.font.min_lbearing > (*xfonts)->min_bounds.lbearing)
				dc.font.min_lbearing = (*xfonts)->min_bounds.lbearing;
			xfonts++;
		}
	}
	else {
		if(!(dc.font.xfont = XLoadQueryFont(dpy, fontstr))
				&& !(dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
			eprint("error, cannot load font: '%s'\n", fontstr);
		dc.font.ascent = dc.font.xfont->ascent;
		dc.font.descent = dc.font.xfont->descent;
		dc.font.max_rbearing = dc.font.xfont->max_bounds.rbearing;
		dc.font.min_lbearing = dc.font.xfont->min_bounds.lbearing;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;
	/*lbearing and rbearing is used to calculate width*/
	dc.font.width = dc.font.max_rbearing - dc.font.min_lbearing;
}

/*Only keypress that is detected is Esc, to quit*/
int kpress(XKeyEvent * e) 
{
	char buf[32];
	int num;
	KeySym ksym;

	buf[0] = 0;
	num = XLookupString(e, buf, sizeof buf, &ksym, NULL);
	switch(ksym) {
		default:
			return 0;
			break;
		case XK_Escape:
			ret = 1;
			running = False;
			break;
	}
	return 0;
}

/*The main function*/
void run(void) 
{
	XEvent ev;
	char buf[4096];
	FILE *fp;

	/* main event loop */

	while(running && !XNextEvent(dpy, &ev))
		switch (ev.type) {
			default:	/* ignore all crap */
				fp = popen(cmd, "r");			
				if(fp == NULL)
					return;
				fgets(buf, sizeof buf, fp);
				if(buf[strlen(buf)-1] == '\n')
					buf[strlen(buf) - 1] = 0;
				drawmenu(buf);
				pclose(fp);
				break;
			case KeyPress:
				if(kpress(&ev.xkey) == 1)
					return;
				break;
		}

}

void setup(Bool topbar) {
	int i, j, x, y;
#if XINERAMA
	int n;
	XineramaScreenInfo *info = NULL;
#endif
	XModifierKeymap *modmap;
	XSetWindowAttributes wa;

	/* init modifier map */
	modmap = XGetModifierMapping(dpy);
	for(i = 0; i < 8; i++)
		for(j = 0; j < modmap->max_keypermod; j++) {
			if(modmap->modifiermap[i * modmap->max_keypermod + j]
					== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
		}
	XFreeModifiermap(modmap);

	/* style */
	dc.norm[ColBG] = getcolor(normbgcolor);
	dc.norm[ColFG] = getcolor(normfgcolor);
	dc.sel[ColBG] = getcolor(selbgcolor);
	dc.sel[ColFG] = getcolor(selfgcolor);
	initfont(font);

	/* menu window */
	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;

	/* menu window geometry */
	mh = dc.font.height + 2;
#if XINERAMA
	if(XineramaIsActive(dpy) && (info = XineramaQueryScreens(dpy, &n))) {
		i = 0;
		if(n > 1) {
			int di;
			unsigned int dui;
			Window dummy;
			if(XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &di, &di, &dui))
				for(i = 0; i < n; i++)
					if(INRECT(x, y, info[i].x_org, info[i].y_org, info[i].width, info[i].height))
						break;
		}
		x = info[i].x_org;
		y = topbar ? info[i].y_org : info[i].y_org + info[i].height - mh;
		mw = info[i].width;
		XFree(info);
	}
	else
#endif
	{
		x = 0;
		y = topbar ? 0 : DisplayHeight(dpy, screen) - mh;
		mw = DisplayWidth(dpy, screen);
	}

	win = XCreateWindow(dpy, root, x, y, mw, mh, 0,
			DefaultDepth(dpy, screen), CopyFromParent,
			DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

	/* pixmap */
	dc.drawable = XCreatePixmap(dpy, root, mw, mh, DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);
	if(!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.xfont->fid);

	text[0] = 0;
	XMapRaised(dpy, win);
}

int textnw(const char *text, unsigned int len) {
	XRectangle r;

	if(dc.font.set) {
		XmbTextExtents(dc.font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc.font.xfont, text, len);
}


int main(int argc, char *argv[]) {
	unsigned int i;
	Bool topbar = True;

	/* command line args */
	for(i = 1; i < argc; i++)
		if(!strcmp(argv[i], "-b"))
			topbar = False;
		else if(!strcmp(argv[i], "-fn")) {
			if(++i < argc) font = argv[i];
		}
		else if(!strcmp(argv[i], "-nb")) {
			if(++i < argc) normbgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-nf")) {
			if(++i < argc) normfgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-sb")) {
			if(++i < argc) selbgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-sf")) {
			if(++i < argc) selfgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-v"))
			eprint("sysmenu-"VERSION", 2010 dmenu engineers and "AUTHORS", see LICENSE for details\n");
		else
			eprint("usage: sysmenu [-i] [-b] [-fn <font>] [-nb <color>] [-nf <color>]\n"
					"             [-sb <color>] [-sf <color>] [-v]\n");
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");
	if(!(dpy = XOpenDisplay(NULL)))
		eprint("sysmenu: cannot open display\n");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	running = grabkeyboard();

	setup(topbar);
	XSync(dpy, False);
	run();
	XCloseDisplay(dpy);
	return ret;
}
