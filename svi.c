/* See LICENSE file */
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

#include "util.h"

/* macros */
#define ISBS(c) (c == 127)
#define ISESC(c) (c == 27)
#define CURLINE getcury(stdscr) /* omitted () to match curses' LINES */
#define CURCOL getcurx(stdscr)
#define LINSIZ 64

/* types */
enum {INSERT, NORMAL};

typedef struct Line Line;
struct Line {
	char *s;  /* string content */
	size_t l; /* length excluding \0 */
	size_t v; /* visual length */
	size_t m; /* multiples of LINSIZ? */
	Line *p;  /* previous line */
	Line *n;  /* next line */
};

typedef struct {
	Line *l;
	size_t o;  /* offset */
} Position;

/* function declarations */
static void loadfile(char *name);
static void savefile(char *name);
static void init(void);
static void cleanup(void);
static void runinsert(void);
static void runnormal(void);
static void runcommand(void);
static void exec(char *cmd);
static void draw(void);
static Position calcdrw(Position p);
static size_t calcvlen(char *str, size_t o);
static size_t calcoffset(char *str, size_t x);
static void insertch(int c);
static Position insertstr(Position p, char *str);
static Position backspace(Position p);
static Line *newline(Line *p, Line *n);
static Position deleteline(Position p);
static void freeline(Line *l);
static void moveleft(void);
static void moveright(void);
static void moveup(void);
static void movedown(void);
static size_t prevutfsize(char *s, int o);
static size_t lflen(char *s); /* length til first \n */
static void setstatus(char *fmt, ...);
static void printstatus(void);

/* global variables */
static int edit = 1;
static int unsaved = 0;
static Position cur;
static Position drw; /* first to be drawn on screen */
static Line *fstln;
static int mode = NORMAL;
static char *status = NULL;
static char *filename = NULL;

/* function definitions */
void
loadfile(char *name)
{
	FILE *f;
	char *buf;
	Position p;
	size_t n;

	if (!(f = fopen(name, "r")))
		die("loadfile:");

	if (!(buf = ecalloc(BUFSIZ + 1, sizeof(char))))
		die("loadfile:");

	p = cur;
	while ((n = fread(buf, sizeof(char), BUFSIZ, f))) {
		p = insertstr(p, buf);
		memset(buf, 0, n);
	}

	if (p.l->s[0] == '\0')
		freeline(p.l);

	strncpy(filename, name, LINSIZ);
	fclose(f);
	free(buf);
}

void
savefile(char *name)
{
	FILE *f;
	Line *l;

	if (name && name[0] != '\0')
		strncpy(filename, name, LINSIZ);

	if (!(f = fopen(filename, "w"))) {
		setstatus("savefile: %s", strerror(errno));
		return;
	}

	for (l = fstln; l; l = l->n)
		fprintf(f, "%s\n", l->s);

	setstatus("\"%s\"", filename);
	unsaved = 0;
	fclose(f);
}

void
init(void)
{
	setlocale(LC_ALL, "");
	initscr();
	raw();
	noecho();
	cur.o = drw.o = 0;
	cur.l = drw.l = fstln = newline(NULL, NULL);
	status = ecalloc(LINSIZ + 1, sizeof(char));
	filename = ecalloc(LINSIZ + 1, sizeof(char));
}
void
cleanup(void)
{
	Line *l, *n;

	endwin();

	free(status);
	free(filename);
	for (l = fstln; l; l = n) {
		n = l->n;
		free(l->s);
		free(l);
	}
}

void
runinsert(void)
{
	int c;

	c = getchar();
	if (ISESC(c)) {
		mode = NORMAL;
		moveleft();
	} else if (ISBS(c)) {
		cur = backspace(cur);
		unsaved = 1;
	} else {
		insertch(c);
		unsaved = 1;
	}
}

void
runnormal(void)
{
	switch(getchar()) {
	case ':':
		runcommand();
		break;
	case 'h':
		moveleft();
		break;
	case 'i':
		mode = INSERT;
		break;
	case 'j':
		movedown();
		break;
	case 'k':
		moveup();
		break;
	case 'l':
		moveright();
		break;
	}
}

void
runcommand(void)
{
	char *cmd;
	size_t i;

	cmd = ecalloc(LINSIZ, sizeof(char));

	setstatus(":");
	printstatus();
	refresh();

	for (i = 0; (cmd[i] = getchar()); i++) {
		if (cmd[i] == '\n' || cmd[i] == '\r') {
			cmd[i] = '\0';
			exec(cmd);
			break;
		} else if (ISESC(cmd[i])) {
			break;
		} else if (!isprint(cmd[i])) {
			cmd[i--] = '\0';
		} else {
			printw("%c", cmd[i]);
			refresh();
		}
	}

	free(cmd);
}

void
exec(char *cmd)
{
	if (cmd[0] == 'q') {
		if (!unsaved || (cmd[1] == '!'))
			edit = 0;
		else
			setstatus("unsaved changes; q! to override");
	} else if (cmd[0] == 'w') {
		if ((cmd[1] == ' ') && (cmd[2] != '\0'))
			savefile(cmd + 2);
		else
			savefile(NULL);
	} else if (cmd[0] == 'd') {
		cur = deleteline(cur);
		unsaved = 1;
	}
}

void
draw(void)
{
	Line *l;
	size_t o, i, x, y = 0;

	drw = calcdrw(drw);
	move(0,0);
	for (l = drw.l, o = drw.o; l; l = l->n, o = 0) {
		if (l == cur.l) {
			y = CURLINE;
			y += calcvlen(l->s + o, cur.o - o) / COLS;
		}
		for (i = o; l->s[i] && LINES - CURLINE - 1; i++)
			printw("%c", l->s[i]);
		printw("\n");
	}
	while (CURLINE < LINES - 1)
		printw("~\n");

	x = calcvlen(cur.l->s, cur.o);
	while (x >= (size_t)COLS)
		x -= COLS;

	printstatus();
	move(y, x);
	refresh();
}

Position
calcdrw(Position p)
{
	Line *l;
	size_t rows, taillen;

	if (cur.l == p.l) {
		p.o = 0;
		return p;
	} else if (cur.l == p.l->p) {
		p.l = p.l->p;
		p.o = 0;
		return p;
	}

	for (l = p.l, rows = 0; l && l->p != cur.l; l = l->n)
		rows += l->v / COLS + 1;
	rows -= calcvlen(p.l->s, p.o) / COLS;

	for (; rows >= (size_t)LINES; rows--) {
		taillen = calcvlen(p.l->s + p.o, MIN((int)(p.l->v - p.o), 0));
		if (taillen >= (size_t)COLS) {
			p.o += COLS;
		} else {
			p.l = p.l->n;
			p.o = 0;
		}
	}

	return p;
}

size_t
calcvlen(char *str, size_t o)
{
	size_t i, x;
	Rune p;

	for (i = x = 0; i < o; ) {
		if (str[i] == '\t')
			x += TABSIZE - (x % TABSIZE);
		else
			x++;
		i += chartorune(&p, str + i);
	}

	return x;
}

size_t
calcoffset(char *str, size_t x)
{
	size_t i, o;
	Rune p;

	for (i = o = 0; i < x ; ) {
		if (str[o] == '\t')
			i += TABSIZE - (i % TABSIZE);
		else
			i++;
		o += chartorune(&p, str + o);
	}

	return o;
}

void
insertch(int c)
{
	int i;
	char *s;

	s = ecalloc(UTFmax, sizeof(char));
	s[0] = (char)c;
	for (i = 1; i <= UTFmax; i++) {
		if (fullrune(s, i))
			break;
		s[i] = getchar();
	}
	if (i <= UTFmax)
		cur = insertstr(cur, s);
	free(s);
}

Position
insertstr(Position p, char *src)
{
	size_t inslen;
	char *ins;

	inslen = lflen(src);
	if (p.l->l + inslen >= LINSIZ*p.l->m) {
		p.l->m += 1 + inslen/LINSIZ;
		p.l->s = realloc(p.l->s, LINSIZ*p.l->m);
	}

	ins = p.l->s + p.o;
	memmove(ins + inslen, ins, strlen(ins) + 1);
	memmove(ins, src, inslen);
	p.o += inslen;
	p.l->l += inslen;
	p.l->v += calcvlen(src, inslen);

	if (inslen < strlen(src)) {
		p.l = newline(p.l, p.l->n);
		p.o = 0;
		p = insertstr(p, src + inslen + 1);
		insertstr(p, ins + inslen);
		ins[inslen] = '\0';
	}

	return p;
}

Position
backspace(Position p)
{
	char *dest, *src;
	size_t n, l;

	if (!p.o)
		return p;

	l = prevutfsize(p.l->s, p.o);
	src = p.l->s + p.o;
	dest = src - l;
	n = strlen(src);

	memmove(dest, src, n + 1);

	p.o -= l;
	p.l->l -= l;
	p.l->v = calcvlen(p.l->s, p.l->l);
	return p;
}

Line *
newline(Line *p, Line *n)
{
	Line *l;

	l = ecalloc(1, sizeof(Line));
	l->s = ecalloc(LINSIZ, sizeof(char));
	l->l = l->v = 0;
	l->m = 1;
	l->p = p;
	l->n = n;

	if (p) p->n = l;
	if (n) n->p = l;

	return l;
}

Position
deleteline(Position p)
{
	if (p.l->n) {
		if (p.l == fstln)
			fstln = p.l->n;
		if (p.l == drw.l) {
			drw.l = p.l->n;
			drw.o = 0;
		}
		p.l = p.l->n;
		p.o = 0;
		freeline(p.l->p);
	} else if (p.l->p) {
		p.l = p.l->p;
		p.o = 0;
		freeline(p.l->n);
	}

	return p;
}

void
freeline(Line *l)
{
	if (l->p)
		l->p->n = l->n;
	if (l->n)
		l->n->p = l->p;
	free(l->s);
	free(l);
}

void
moveleft(void)
{
	if (!cur.o)
		return;

	cur.o -= prevutfsize(cur.l->s, cur.o);
}

void
moveright(void)
{
	Rune p;

	if (cur.l->s[cur.o] == '\0')
		return;

	cur.o += chartorune(&p, cur.l->s + cur.o);
}

void
moveup(void)
{
	size_t pos;

	if (!cur.l->p)
		return;

	pos = calcvlen(cur.l->s, cur.o);
	pos = MIN(pos, cur.l->p->v);
	cur.o = calcoffset(cur.l->p->s, pos);

	cur.l = cur.l->p;
}

void
movedown(void)
{
	size_t pos;

	if (!cur.l->n)
		return;

	pos = calcvlen(cur.l->s, cur.o);
	pos = MIN(pos, cur.l->n->v);
	cur.o = calcoffset(cur.l->n->s, pos);

	cur.l = cur.l->n;
}

size_t
prevutfsize(char *s, int o)
{
	int max, i, n;
	Rune r;

	max = MIN(UTFmax, o);
	for (i = max; i > 0; i--) {
		n = charntorune(&r, s+o-i, i);
		if (n == i)
			return n;
	}
	return 0;
}

size_t
lflen(char *s)
{
	char *lf;

	if ((lf = strchr(s, '\n'))) /* fuck CRLF */
		return lf - s;
	else
		return strlen(s);
}

void
setstatus(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(status, LINSIZ, fmt, ap);
	va_end(ap);
}

void
printstatus(void)
{
	size_t i;

	move(LINES - 1, 0);
	for (i = 0; i < (size_t)COLS; i++)
		printw(" ");
	move(LINES - 1, 0);
	printw(status);
}

int
main(int argc, char *argv[])
{
	if (argc > 2)
		die("usage: %s [file]", argv[0]);

	init();
	atexit(cleanup);

	if (argc == 2)
		loadfile(argv[1]);

	while (edit) {
		draw();

		if (mode == INSERT)
			runinsert();
		else if (mode == NORMAL)
			runnormal();
	}

	return 0;
}
