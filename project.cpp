//Jason Thai

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <GL/glx.h>
extern "C"
{
#include "fonts.h"
}

#include <X11/extensions/Xdbe.h>

#include <string>
#include <iostream>

#include <unistd.h>
using namespace std;
#define GRAVITY -9.8

//macro to swap two integers
#define SWAP(x,y) (x)^=(y);(y)^=(x);(x)^=(y)

//--------------------
//XWindows vaariables
Display *dpy;
Window win;
GC gc;
XdbeBackBuffer backBuffer;
XdbeSwapInfo swapInfo;
//--------------------
//a few more global variables
enum {
    MODE_NONE,
    MODE_LINES,
    LINETYPE_XWIN,
    LINETYPE_BRESENHAM
};

//Setup timers
const double physicsRate = 1.0 / 60.0;
const double oobillion = 1.0 / 1e9;
struct timespec timeStart, timeCurrect;
struct timespec timePause;
double physicsCountdown = 0.0;
double timeSpan = 0.0;
double timeDiff(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
        (double)(end->tv_nsec - start->tv_nsec) * oobillion;
}
void timeCopy(struct timespec *dest, struct timespec *source)
{
    memcpy(dest, source, sizeof(struct timespec));
}
//


struct Vec
{
    float x, y, z;
};

struct Shape
{
    float width, height;
    float radius;
    Vec center;
};

struct Bullet
{
    Vec pos;
    Vec vel;
    float color[3];
    struct timespec time;
    struct Bullet *prev;
    struct Bullet *next;
    Bullet()
    {
        prev = NULL;
        next = NULL;
    }
};

#define MAX_POINTS 400
struct Global {
    int xres, yres;
    int done;

    Shape circle;
    Vec aim;

    bool flashlight;
    float angle;
    int dist;
    Vec point1;
    Vec point2;

    int direction;

    Bullet *bhead;
    //int nbullets;

    Global() {
        //constructor
        xres = 640;
        yres = 480;

        circle.radius = 20.0;
        circle.center.x = xres/2;
        circle.center.y = yres/2;

        flashlight = 0;
        angle = 3.14/4;
        dist = 200;

        direction = 0;

        bhead = NULL;
        //nbullets = 0;

    }
} g;
//-------------------
//function prototypes
void initXwindows();
void cleanupXwindows();
void checkResize(XEvent *e);
void checkMouse(XEvent *e);
void checkKeys(XEvent *e);
void physics();
void showMenu(int x, int y);
void render();
void clearScreen();

int main()
{
    srand((unsigned)time(NULL));
    initXwindows();
    while (!g.done) {
        //Check the event queue
        while (XPending(dpy)) {
            //Handle evemts one-by-one
            XEvent e;
            XNextEvent(dpy, &e);
            checkResize(&e);
            checkMouse(&e);
            checkKeys(&e);
        }
        clearScreen();
        physics();
        render();
        XdbeSwapBuffers(dpy, &swapInfo, 1);
        usleep(8000);
    }
    cleanupXwindows();
    return 0;
}

void cleanupXwindows(void)
{
    if(!XdbeDeallocateBackBufferName(dpy, backBuffer))
    {
        fprintf(stderr, "Error : unable to deallocate back buffer.\n");
    }
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
}

void initXwindows(void)
{
    XSetWindowAttributes attributes;
    int major, minor;
    XdbeBackBufferAttributes *backAttr;

    dpy = XOpenDisplay(NULL);
    //List of events we want to handle
    attributes.event_mask = ExposureMask | StructureNotifyMask |
        PointerMotionMask | ButtonPressMask |
        ButtonReleaseMask | KeyPressMask;
    //Various window attributes
    attributes.backing_store = Always;
    attributes.save_under = True;
    attributes.override_redirect = False;
    attributes.background_pixel = 0x00000000;
    //Get default root window
    Window root = DefaultRootWindow(dpy);
    //Create a window
    win = XCreateWindow(dpy, root, 0, 0, g.xres, g.yres, 0,
            CopyFromParent, InputOutput, CopyFromParent,
            CWBackingStore | CWOverrideRedirect | CWEventMask |
            CWSaveUnder | CWBackPixel, &attributes);
    //Create gc
    gc = XCreateGC(dpy, win, 0, NULL);
    //Get DBE version
    if (!XdbeQueryExtension(dpy, &major, &minor)) {
        fprintf(stderr, "Error : unable to fetch Xdbe Version.\n");
        XFreeGC(dpy, gc);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        exit(1);
    }
    printf("Xdbe version %d.%d\n",major,minor);
    //Get back buffer and attributes (used for swapping)
    backBuffer = XdbeAllocateBackBufferName(dpy, win, XdbeUndefined);
    backAttr = XdbeGetBackBufferAttributes(dpy, backBuffer);
    swapInfo.swap_window = backAttr->window;
    swapInfo.swap_action = XdbeUndefined;
    XFree(backAttr);
    //Map and raise window
    XMapWindow(dpy, win);
    XRaiseWindow(dpy, win);
}


void checkResize(XEvent *e)
{
    //ConfigureNotify is sent when window size changes.
    if (e->type != ConfigureNotify)
        return;
    XConfigureEvent xce = e->xconfigure;
    g.xres = xce.width;
    g.yres = xce.height;
}

void clearScreen(void)
{
    //XClearWindow(dpy, backBuffer);
    XSetForeground(dpy, gc, 0x00000000);
    XFillRectangle(dpy, backBuffer, gc, 0, 0, g.xres, g.yres);
}

void checkMouse(XEvent *e)
{
    static int savex = 0;
    static int savey = 0;
    //
    int mx = e->xbutton.x;
    int my = e->xbutton.y;
    //

    g.aim.x = mx;
    g.aim.y = my;

    if (e->type == ButtonRelease) {
        return;
    }
    if (e->type == ButtonPress) {
        //Log("ButtonPress %i %i\n", e->xbutton.x, e->xbutton.y);
        if (e->xbutton.button==1) {
            //Left button pressed
        }
        if (e->xbutton.button==3) {
            //Right button pressed
        }
    }
    if (savex != mx || savey != my) {
        //mouse moved
        savex = mx;
        savey = my;

        g.aim.x = savex;
        g.aim.y = savey;
    }
}

void checkKeys(XEvent *e)
{
    if (e->type != KeyPress)
        return;
    //A key was pressed
    int key = XLookupKeysym(&e->xkey, 0);

    switch (key) {
        case XK_Escape:
            //quitting the program
            g.done=1;
            break;
        case XK_w:
            g.circle.center.y -= g.circle.radius*2;
            g.direction = 0;
            break;
        case XK_a:
            g.circle.center.x -= g.circle.radius*2;
            g.direction = 1;
            break;
        case XK_s:
            g.circle.center.y += g.circle.radius*2;
            g.direction = 2;
            break;
        case XK_d:
            g.circle.center.x += g.circle.radius*2;
            g.direction = 3;
            break;

        case XK_f:
            g.flashlight ^= 1;
            break;

        case XK_space:
            Bullet *b = new Bullet;

            b->pos.x = g.circle.center.x;
            b->pos.y = g.circle.center.y;
            if(g.direction == 0)
            {
                b->vel.x = 0;
                b->vel.y = -g.circle.radius;
            }
            if(g.direction == 1)
            {
                b->vel.x = -g.circle.radius;
                b->vel.y = 0;
            }
            if(g.direction == 2)
            {
                b->vel.x = 0;
                b->vel.y = g.circle.radius;
            }
            if(g.direction == 3)
            {
                b->vel.x = g.circle.radius;
                b->vel.y = 0;
            }

            b->next = g.bhead;
            if(g.bhead != NULL)
                g.bhead->prev = b;
            g.bhead = b;

            break;

    }
    clearScreen();
}

void deleteBullet(Bullet *node)
{
    //remove a node from linked list
    if(node->prev == NULL)
    {
        if(node->next == NULL)
        {
            g.bhead = NULL;
        }
        else
        {
            node->next->prev = NULL;
            g.bhead = node->next;
        }
    }
    else
    {
        if(node->next == NULL)
        {
            node->prev->next = NULL;
        }
        else
        {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }
    }
    delete node;
    node = NULL;
}

void physics(void)
{
    Bullet *b = g.bhead;
    while(b)
    {
        b->pos.x += b->vel.x;
        b->pos.y += b->vel.y;
        if(b->pos.x < 0.0 || b->pos.x > (float)g.xres ||
                b->pos.y < 0.0 || b->pos.y > (float)g.yres)
        {
            deleteBullet(b);
        }
        b = b->next;
    }
}

void setColor3i(int r, int g, int b)
{
    unsigned long cref = 0L;
    cref += r;
    cref <<= 8;
    cref += g;
    cref <<= 8;
    cref += b;
    XSetForeground(dpy, gc, cref);
}

void showMenu(int x, int y)
{
    char ts[64];

    setColor3i(100, 255, 255);
    sprintf(ts,"Jason Thai");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 16;
    (g.flashlight==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
    sprintf(ts, "(F) flashlight: %s", (g.flashlight==1)?"ON":"OFF");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
    
    y += 16;
    setColor3i(255,255,255);
    sprintf(ts, "(Space) Shoot");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
}

void render(void)
{
    showMenu(4, 16);

    setColor3i(255,255,255);
    XDrawArc(dpy, backBuffer, gc, g.circle.center.x-g.circle.radius, g.circle.center.y-g.circle.radius, 
            g.circle.radius*2, g.circle.radius*2, 0, 360*64);
    XFillArc(dpy, backBuffer, gc, g.circle.center.x-g.circle.radius, g.circle.center.y-g.circle.radius, 
            g.circle.radius*2, g.circle.radius*2, 0, 360*64);

    //setColor3i(255,0,0);
    //XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
    //        g.aim.x, g.aim.y);

    if(g.direction == 0)
    {
        for(float i = g.angle; i < g.angle*2; i += 0.005)
        {
            if(g.flashlight)
            {
                setColor3i(255,255,0);
                g.point1.x = g.circle.center.x - (cos(i)*g.dist);
                g.point1.y = g.circle.center.y - (sin(i)*g.dist);
                g.point2.x = g.circle.center.x + (cos(i)*g.dist);
                g.point2.y = g.circle.center.y - (sin(i)*g.dist);
            }
            else
            {
                setColor3i(150,150,150);
                g.point1.x = g.circle.center.x - (cos(i)*g.dist/2);
                g.point1.y = g.circle.center.y - (sin(i)*g.dist/2);
                g.point2.x = g.circle.center.x + (cos(i)*g.dist/2);
                g.point2.y = g.circle.center.y - (sin(i)*g.dist/2);
            }
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point1.x, g.point1.y);
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point2.x, g.point2.y);
        }
    }
    if(g.direction == 1)
    {
        for(float i = 0.0; i < g.angle; i += 0.005)
        {
            if(g.flashlight)
            {
                setColor3i(255,255,0);
                g.point1.x = g.circle.center.x - (cos(i)*g.dist);
                g.point1.y = g.circle.center.y + (sin(i)*g.dist);
                g.point2.x = g.circle.center.x - (cos(i)*g.dist);
                g.point2.y = g.circle.center.y - (sin(i)*g.dist);
            }
            else
            {
                setColor3i(150,150,150);
                g.point1.x = g.circle.center.x - (cos(i)*g.dist/2);
                g.point1.y = g.circle.center.y + (sin(i)*g.dist/2);
                g.point2.x = g.circle.center.x - (cos(i)*g.dist/2);
                g.point2.y = g.circle.center.y - (sin(i)*g.dist/2);
            }
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point1.x, g.point1.y);
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point2.x, g.point2.y);
        }
    }
    if(g.direction == 2)
    {
        for(float i = g.angle; i < g.angle*2; i += 0.005)
        {
            if(g.flashlight)
            {
                setColor3i(255,255,0);
                g.point1.x = g.circle.center.x + (cos(i)*g.dist);
                g.point1.y = g.circle.center.y + (sin(i)*g.dist);
                g.point2.x = g.circle.center.x - (cos(i)*g.dist);
                g.point2.y = g.circle.center.y + (sin(i)*g.dist);
            }
            else
            {
                setColor3i(150,150,150);
                g.point1.x = g.circle.center.x + (cos(i)*g.dist/2);
                g.point1.y = g.circle.center.y + (sin(i)*g.dist/2);
                g.point2.x = g.circle.center.x - (cos(i)*g.dist/2);
                g.point2.y = g.circle.center.y + (sin(i)*g.dist/2);
            }
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point1.x, g.point1.y);
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point2.x, g.point2.y);
        }
    }
    if(g.direction == 3)
    {
        for(float i = 0; i < g.angle; i += 0.005)
        {
            if(g.flashlight)
            {
                setColor3i(255,255,0);
                g.point1.x = g.circle.center.x + (cos(i)*g.dist);
                g.point1.y = g.circle.center.y - (sin(i)*g.dist);
                g.point2.x = g.circle.center.x + (cos(i)*g.dist);
                g.point2.y = g.circle.center.y + (sin(i)*g.dist);
            }
            else
            {
                setColor3i(150,150,150);
                g.point1.x = g.circle.center.x + (cos(i)*g.dist/2);
                g.point1.y = g.circle.center.y - (sin(i)*g.dist/2);
                g.point2.x = g.circle.center.x + (cos(i)*g.dist/2);
                g.point2.y = g.circle.center.y + (sin(i)*g.dist/2);
            }
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point1.x, g.point1.y);
            XDrawLine(dpy, backBuffer, gc, g.circle.center.x, g.circle.center.y,
                    g.point2.x, g.point2.y);
        }
    }

    Bullet *b = g.bhead;
    while(b)
    {
        setColor3i(255,255,255);
        XDrawRectangle(dpy, backBuffer, gc, b->pos.x-g.circle.radius/4, b->pos.y-g.circle.radius/4, 
                g.circle.radius/2, g.circle.radius/2);
        XFillRectangle(dpy, backBuffer, gc, b->pos.x-g.circle.radius/4, b->pos.y-g.circle.radius/4, 
                g.circle.radius/2, g.circle.radius/2);
        b = b->next;
    }
}

