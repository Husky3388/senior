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

#include "maps.h"
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

struct Grenade
{
    Vec pos;
    Vec vel;
    float color[3];
    struct timespec time;
    struct Grenade *prev;
    struct Grenade *next;
    Grenade()
    {
        prev = NULL;
        next = NULL;
    }
};

struct Tower
{
    Vec pos;
    float color[3];
    int ammo;
    struct Tower *prev;
    struct Tower *next;
    Tower()
    {
        prev = NULL;
        next = NULL;
    }
};

struct Resource
{
    int resourceA;
    int resourceB;
    int resourceC;
    int bullets;
    int grenades;
    int towers;
    Resource()
    {
        resourceA = 5;
        resourceB = 5;
        resourceC = 5;
        bullets = 5;
        grenades = 5;
        towers = 5;
    }
};

struct Grid
{
    int size;
    int column[100];
    int row[100];
};

struct Options
{
    int menu;
    bool help;
    int resolution;
    Options()
    {
        menu = 0;
        help = 1;
    }
};

struct Resolution
{
    int menu;
    int selection;
    Resolution()
    {
        menu = 0;
        selection = 0;
    }
};

#define MAX_POINTS 400
struct Global {
    int xres, yres;
    int done;

    struct timespec timer;

    Shape circle;
    Vec aim;

    bool flashlight;
    float angle;
    int dist;
    Vec point1;
    Vec point2;

    int direction;

    int inventory;

    Bullet *bhead;
    //int nbullets;

    Grenade *ghead;

    Tower *thead;
    int totalTowers;

    Resource resource;

    bool combine;

    bool title;
    int menu;

    Grid grid;
    bool drawGrid;

    Options options;
    bool showUI;
    bool showOptions;
    Resolution resolution;
    bool showRes;

    int area;

    Global() {
        //constructor
        xres = 640;
        yres = 480;

        clock_gettime(CLOCK_REALTIME, &timer);

        circle.radius = 20.0;
        circle.center.x = xres/2 - circle.radius;
        circle.center.y = yres/2 - circle.radius;

        flashlight = 0;
        angle = 3.14/4;
        dist = 200;

        direction = 0;

        inventory = 1;

        bhead = NULL;
        //nbullets = 0;

        ghead = NULL;

        thead = NULL;
        totalTowers = 0;

        combine = 0;

        title = 1;
        menu = 0;

        drawGrid = false;

        showUI = 0;
        showOptions = 0;
        showRes = 0;

        area = 0;
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

void tower();

void initGrid();

int main()
{
    initGrid();
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
        //tower();
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
    initGrid();
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

void shootBullet(float xLocation, float yLocation, float xDirection, float yDirection)
{
    Bullet *b = new Bullet;

    struct timespec bt;
    clock_gettime(CLOCK_REALTIME, &bt);
    timeDiff(&g.timer, &bt);
    timeCopy(&g.timer, &bt);
    timeCopy(&b->time, &bt);

    b->pos.x = xLocation;
    b->pos.y = yLocation;

    b->vel.x = xDirection;
    b->vel.y = yDirection;

    b->next = g.bhead;
    if(g.bhead != NULL)
        g.bhead->prev = b;
    g.bhead = b;
}

void checkKeys(XEvent *e)
{
    if (e->type != KeyPress)
        return;
    //A key was pressed
    int key = XLookupKeysym(&e->xkey, 0);

    if(g.title && !g.showOptions && !g.showRes) // title
    {
        switch(key)
        {
            case XK_Escape:
                g.done = 1;
                break;
            case XK_w:
                if(g.menu-1 < 0)
                {
                    g.menu = 3;
                }
                g.menu = (g.menu-1)%3;
                break;
            case XK_s:
                g.menu = (g.menu+1)%3;
                break;
            case XK_space:
                if(g.menu == 0)
                {
                    g.title ^= 1;
                }
                else if(g.menu == 1)
                {
                    g.title ^= 1;
                    g.showOptions ^= 1;
                }
                else if(g.menu == 2)
                {
                    g.done = 1;
                }
                break;
        }
    }
    else if(!g.title && g.showOptions && !g.showRes) // options
    {
        switch(key)
        {
            case XK_Escape:
                g.title ^= 1;
                g.showOptions ^= 1;
                break;
            case XK_w:
                if(g.options.menu-1 < 0)
                {
                    g.options.menu = 3;
                }
                g.options.menu = (g.options.menu-1)%3;
                break;
            case XK_s:
                g.options.menu = (g.options.menu+1)%3;
                break;
            case XK_space:
                if(g.options.menu == 0)
                {
                    g.options.help ^= 1;
                }
                else if(g.options.menu == 1)
                {
                    g.showOptions ^= 1;
                    g.showRes ^= 1;
                }
                else if(g.options.menu == 2)
                {
                    g.title ^= 1;
                    g.showOptions ^= 1;
                }
                break;
        }
    }
    else if(!g.title && !g.showOptions && g.showRes) // resolution
    {
        switch(key)
        {
            case XK_Escape:
                g.showRes ^= 1;
                g.showOptions ^= 1;
                break;
            case XK_w:
                if(g.resolution.menu-1 < 0)
                {
                    g.resolution.menu = 4;
                }
                g.resolution.menu = (g.resolution.menu-1)%4;
                break;
            case XK_s:
                g.resolution.menu = (g.resolution.menu+1)%4;
                break;
            case XK_space:

                if(g.resolution.menu == 0)
                {
                    g.xres = 640;
                    g.yres = 480;
                    g.resolution.selection = 0;
                    g.circle.radius = 20.0;
                    cleanupXwindows();
                    initXwindows();
                }
                else if(g.resolution.menu == 1)
                {
                    g.xres = 840;
                    g.yres = 640;
                    g.resolution.selection = 1;
                    g.circle.radius = 40.0;
                    cleanupXwindows();
                    initXwindows();

                }
                else if(g.resolution.menu == 2)
                {
                    g.xres = 1080;
                    g.yres = 840;
                    g.resolution.selection = 2;
                    g.circle.radius = 40.0;
                    cleanupXwindows();
                    initXwindows();
                }
                else if(g.resolution.menu == 3)
                {
                    g.showRes ^= 1;
                    g.showOptions ^= 1;
                }
                break;
        }
    }
    else if(!g.title && !g.showOptions && !g.showRes) // game
    {
        switch (key) {
            case XK_Escape:
                //quitting the program
                //g.done=1;
                g.title ^= 1;
                break;
            case XK_w:
                //g.circle.center.y -= g.circle.radius*2;
                //g.direction = 0;
                if(g.direction == 0 && (g.circle.center.y - g.circle.radius*2) > 0)
                    g.circle.center.y -= g.circle.radius*2;
                if(g.direction == 1 && (g.circle.center.x - g.circle.radius*2) > 0)
                    g.circle.center.x -= g.circle.radius*2;
                if(g.direction == 2 && (g.circle.center.y + g.circle.radius*2) < g.yres)
                    g.circle.center.y += g.circle.radius*2;
                if(g.direction == 3 && (g.circle.center.x + g.circle.radius*2) < g.xres)
                    g.circle.center.x += g.circle.radius*2;
                tower();
                // NOTE: location of circle regarding grid is offset by 0.5
                //cout << g.circle.center.x/g.grid.size << endl << g.circle.center.y/g.grid.size << endl << endl;
                break;
            case XK_a:
                //g.circle.center.x -= g.circle.radius*2;
                //g.direction = 1;
                g.direction = (g.direction+1)%4;
                tower();
                break;
            case XK_s:
                //g.circle.center.y += g.circle.radius*2;
                //g.direction = 2;
                g.direction = (g.direction+2)%4;
                tower();
                break;
            case XK_d:
                //g.circle.center.x += g.circle.radius*2;
                //g.direction = 3;
                g.direction = (g.direction+3)%4;
                tower();
                break;

            case XK_f:
                g.flashlight ^= 1;
                break;

            case XK_1:
                if(g.combine == 0)
                    g.inventory = 1;
                else if(g.combine == 1 && g.resource.resourceA > 0 && g.resource.resourceB > 0)
                {
                    g.resource.resourceA--;
                    g.resource.resourceB--;
                    g.resource.bullets += 3;
                }
                break;

            case XK_2:
                if(g.combine == 0)
                    g.inventory = 2;
                else if(g.combine == 1 && g.resource.resourceA > 0 && g.resource.resourceC > 0)
                {
                    g.resource.resourceA--;
                    g.resource.resourceC--;
                    g.resource.grenades++;;
                }
                break;

            case XK_3:
                if(g.combine == 0)
                    g.inventory = 3;
                else if(g.combine == 1 && g.resource.resourceB > 0 && g.resource.resourceC > 0)
                {
                    g.resource.resourceB--;
                    g.resource.resourceC--;
                    g.resource.towers++;;
                }
                break;

            case XK_space:
                if(g.inventory == 1)
                {
                    if(g.resource.bullets > 0)
                    {
                        if(g.direction == 0)
                            shootBullet(g.circle.center.x, g.circle.center.y, 
                                    0, -g.circle.radius);
                        if(g.direction == 1)
                            shootBullet(g.circle.center.x, g.circle.center.y,
                                    -g.circle.radius, 0);
                        if(g.direction == 2)
                            shootBullet(g.circle.center.x, g.circle.center.y, 
                                    0, g.circle.radius);
                        if(g.direction == 3)
                            shootBullet(g.circle.center.x, g.circle.center.y, 
                                    g.circle.radius, 0);
                        g.resource.bullets--;
                    }
                }
                else if(g.inventory == 2)
                {
                    if(g.resource.grenades > 0)
                    {
                        Grenade *gr = new Grenade;

                        struct timespec grt;
                        clock_gettime(CLOCK_REALTIME, &grt);
                        timeDiff(&g.timer, &grt);
                        timeCopy(&g.timer, &grt);
                        timeCopy(&gr->time, &grt);

                        gr->pos.x = g.circle.center.x;
                        gr->pos.y = g.circle.center.y;
                        if(g.direction == 0)
                        {
                            gr->vel.x = 0;
                            gr->vel.y = -g.circle.radius/4;
                        }
                        if(g.direction == 1)
                        {
                            gr->vel.x = -g.circle.radius/4;
                            gr->vel.y = 0;
                        }
                        if(g.direction == 2)
                        {
                            gr->vel.x = 0;
                            gr->vel.y = g.circle.radius/4;
                        }
                        if(g.direction == 3)
                        {
                            gr->vel.x = g.circle.radius/4;
                            gr->vel.y = 0;
                        }

                        gr->next = g.ghead;
                        if(g.ghead != NULL)
                            g.ghead->prev = gr;
                        g.ghead = gr;
                        g.resource.grenades--;
                    }
                }
                else if(g.inventory == 3)
                {
                    if(g.resource.towers > 0)
                    {
                        Tower *t = new Tower;

                        if(g.direction == 0)
                        {
                            t->pos.x = g.circle.center.x;
                            t->pos.y = g.circle.center.y - g.circle.radius*2;
                        }
                        if(g.direction == 1)
                        {
                            t->pos.x = g.circle.center.x - g.circle.radius*2;
                            t->pos.y = g.circle.center.y;
                        }
                        if(g.direction == 2)
                        {
                            t->pos.x = g.circle.center.x;
                            t->pos.y = g.circle.center.y + g.circle.radius*2;
                        }
                        if(g.direction == 3)
                        {
                            t->pos.x = g.circle.center.x + g.circle.radius*2;
                            t->pos.y = g.circle.center.y;
                        }
                        t->ammo = 5;

                        t->next = g.thead;
                        if(g.thead != NULL)
                            g.thead->prev = t;
                        g.thead = t;
                        g.totalTowers++;
                        g.resource.towers--;
                    }
                }

                break;

            case XK_c:
                g.combine ^= 1;
                break;

            case XK_u:
                g.showUI ^= 1;
                break;

                // DEBUG ///////////////////////////////////////////////
            case XK_r:
                g.resource.resourceA = 5;
                g.resource.resourceB = 5;
                g.resource.resourceC = 5;
                g.resource.bullets = 5;
                g.resource.grenades = 5;
                g.resource.towers = 5;
                break;

            case XK_g:
                g.drawGrid ^= 1;
                break;
                // DEBUG ///////////////////////////////////////////////
        }
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

void deleteGrenade(Grenade *node)
{
    //remove a node from linked list
    if(node->prev == NULL)
    {
        if(node->next == NULL)
        {
            g.ghead = NULL;
        }
        else
        {
            node->next->prev = NULL;
            g.ghead = node->next;
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

void deleteTower(Tower *node)
{
    //remove a node from linked list
    if(node->prev == NULL)
    {
        if(node->next == NULL)
        {
            g.thead = NULL;
        }
        else
        {
            node->next->prev = NULL;
            g.thead = node->next;
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
    g.totalTowers--;
}

Vec vecNormalize(Vec v)
{
    float length = v.x*v.x + v.y*v.y;
    if(length == 0.0)
        return v;
    length = 1.0 / sqrt(length);
    v.x *= length;
    //cout << v.x << endl;
    v.y *= length;

    return v;
}

void tower(void)
{
    Tower *t = g.thead;
    while(t)
    {
        Vec v;
        v.x = g.circle.center.x - t->pos.x;
        v.y = g.circle.center.y - t->pos.y;
        v = vecNormalize(v);

        shootBullet(t->pos.x, t->pos.y, v.x*g.circle.radius, v.y*g.circle.radius);

        t->ammo--;

        if(t->ammo <= 0)
            deleteTower(t);
        t = t->next;
    }
}

void physics(void)
{
    Bullet *b = g.bhead;
    struct timespec bt;
    clock_gettime(CLOCK_REALTIME, &bt);

    while(b)
    {
        b->pos.x += b->vel.x;
        b->pos.y += b->vel.y;
        if(b->pos.x < 0.0 || b->pos.x > (float)g.xres ||
                b->pos.y < 0.0 || b->pos.y > (float)g.yres)
        {
            deleteBullet(b);
        }

        double ts = timeDiff(&b->time, &bt);
        if(ts > 0.3)
        {
            deleteBullet(b);
        }

        b = b->next;
    }

    Grenade *gr = g.ghead;
    struct timespec grt;
    clock_gettime(CLOCK_REALTIME, &grt);

    while(gr)
    {
        gr->pos.x += gr->vel.x;
        gr->pos.y += gr->vel.y;
        if(gr->pos.x < 0.0 || gr->pos.x > (float)g.xres ||
                gr->pos.y < 0.0 || gr->pos.y > (float)g.yres)
        {
            deleteGrenade(gr);
        }

        double ts = timeDiff(&gr->time, &grt);
        if(ts > 0.5)
        {
            shootBullet(gr->pos.x, gr->pos.y, 0, -g.circle.radius);
            shootBullet(gr->pos.x, gr->pos.y, g.circle.radius, -g.circle.radius);
            shootBullet(gr->pos.x, gr->pos.y, g.circle.radius, 0);
            shootBullet(gr->pos.x, gr->pos.y, g.circle.radius, g.circle.radius);
            shootBullet(gr->pos.x, gr->pos.y, 0, g.circle.radius);
            shootBullet(gr->pos.x, gr->pos.y, -g.circle.radius, g.circle.radius);
            shootBullet(gr->pos.x, gr->pos.y, -g.circle.radius, 0);
            shootBullet(gr->pos.x, gr->pos.y, -g.circle.radius, -g.circle.radius);

            deleteGrenade(gr);
        }
        gr = gr->next;
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

    (g.showUI==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
    sprintf(ts, "(U) UI: %s", (g.showUI==1)?"ON":"OFF");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    if(g.showUI)
    {
        y += 32;
        setColor3i(255, 100, 100);
        sprintf(ts,"Health: ");
        for(int i = 0; i < 5; i++)
        {
            strcat(ts, "I ");
        }
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

        if(g.options.help)
        {
            y += 32;
            (g.flashlight==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "(F) Flashlight: %s", (g.flashlight==1)?"ON":"OFF");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        }
        if(g.combine == 0)
        {
            y += 32;
            (g.inventory==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "(1) Gun: %s", (g.inventory==1)?"ON":"OFF");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

            y += 16;
            (g.inventory==2) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "(2) Grenade: %s", (g.inventory==2)?"ON":"OFF");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

            y += 16;
            (g.inventory==3) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "(3) Tower: %s", (g.inventory==3)?"ON":"OFF");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        }
        else if(g.combine == 1)
        {
            y += 32;
            (g.resource.resourceA > 0 && g.resource.resourceB > 0) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "1 ResourceA + 1 ResourceB = 3 bullets");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
            y += 16;
            (g.resource.resourceA > 0 && g.resource.resourceC > 0) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "1 ResourceA + 1 ResourceC = 1 grenade");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
            y += 16;
            (g.resource.resourceB > 0 && g.resource.resourceC > 0) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "1 ResourceB + 1 ResourceC = 1 tower");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        }

        if(g.options.help)
        {
            y += 32;
            setColor3i(255,255,255);
            sprintf(ts, "(Space) Shoot");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        }

        y += 32;
        setColor3i(255,255,255);
        sprintf(ts, "Resource A: %i", g.resource.resourceA);
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        y += 16;
        setColor3i(255,255,255);
        sprintf(ts, "Resource B: %i", g.resource.resourceB);
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        y += 16;
        setColor3i(255,255,255);
        sprintf(ts, "Resource C: %i", g.resource.resourceC);
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        y += 32;
        setColor3i(255,255,255);
        sprintf(ts, "Bullets: %i", g.resource.bullets);
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        y += 16;
        setColor3i(255,255,255);
        sprintf(ts, "Grenades: %i", g.resource.grenades);
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        y += 16;
        setColor3i(255,255,255);
        sprintf(ts, "Towers: %i", g.resource.towers);
        XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

        if(g.options.help)
        {
            y += 32;
            (g.combine==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "(C) Combine: %s", (g.combine==1)?"ON":"OFF");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

            y += 32;
            setColor3i(0,255,255);
            sprintf(ts, "(DEBUG) (R) Reset resources");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

            y += 16;
            (g.drawGrid==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
            sprintf(ts, "(DEBUG) (G) Show Grid");
            XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
        }
    }
}

void showTitle(int x, int y)
{
    char ts[64];

    setColor3i(255, 255, 255);
    sprintf(ts,"Insert Title Here");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 32;
    sprintf(ts, "%s Start Game", (g.menu==0)?"-->":"   ");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 16;
    sprintf(ts, "%s Options", (g.menu==1)?"-->":"   ");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 16;
    sprintf(ts, "%s Exit Game", (g.menu==2)?"-->":"   ");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
}

void showOption(int x, int y)
{
    char ts[64];

    setColor3i(255, 255, 255);
    sprintf(ts,"Options");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 32;
    (g.options.help==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
    sprintf(ts, "%s Set Help: %s", (g.options.menu==0)?"-->":"   ", (g.options.help==1)?"ON":"OFF");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    setColor3i(255, 255, 255);
    y += 16;
    sprintf(ts, "%s Set Resolution", (g.options.menu==1)?"-->":"   ");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 32;
    sprintf(ts, "%s Return", (g.options.menu==2)?"-->":"   ");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
}

void showRes(int x, int y)
{
    char ts[64];

    setColor3i(255,255,255);
    sprintf(ts, "Resolutions");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 32;
    (g.resolution.selection==0) ? setColor3i(0,255,0) : setColor3i(255,0,0);
    sprintf(ts, "%s 640 x 480: %s", (g.resolution.menu==0)?"-->":"   ", (g.resolution.selection==0)?"ON":"OFF");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 16;
    (g.resolution.selection==1) ? setColor3i(0,255,0) : setColor3i(255,0,0);
    sprintf(ts, "%s 840 x 640: %s", (g.resolution.menu==1)?"-->":"   ", (g.resolution.selection==1)?"ON":"OFF");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    y += 16;
    (g.resolution.selection==2) ? setColor3i(0,255,0) : setColor3i(255,0,0);
    sprintf(ts, "%s 1080 x 840: %s", (g.resolution.menu==2)?"-->":"   ", (g.resolution.selection==2)?"ON":"OFF");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));

    setColor3i(255,255,255);
    y += 32;
    sprintf(ts, "%s Return", (g.resolution.menu==3)?"-->":"   ");
    XDrawString(dpy, backBuffer, gc, x, y, ts, strlen(ts));
}


void render(void)
{
    if(g.title)
    {
        showTitle(4, 16);
    }
    else if(!g.title && g.showOptions)
    {
        showOption(4, 16);
    }
    else if(g.showRes)
    {
        showRes(4, 16);
    }
    else if(!g.title && !g.showOptions)
    {
        if(g.drawGrid)
        {
            setColor3i(255,255,255);
            for(size_t i = 0; i < sizeof(g.grid.column)/4; i++)
            {
                XDrawLine(dpy, backBuffer, gc, g.grid.column[i], 0, g.grid.column[i], g.yres);
            }
            for(size_t i = 0; i < sizeof(g.grid.row)/4; i++)
            {
                XDrawLine(dpy, backBuffer, gc, 0, g.grid.row[i], g.xres, g.grid.row[i]);
            }
        }
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

        Grenade *gr = g.ghead;
        while(gr)
        {
            setColor3i(0,255,255);
            XDrawRectangle(dpy, backBuffer, gc, gr->pos.x-g.circle.radius/4, gr->pos.y-g.circle.radius/4, 
                    g.circle.radius/2, g.circle.radius/2);
            XFillRectangle(dpy, backBuffer, gc, gr->pos.x-g.circle.radius/4, gr->pos.y-g.circle.radius/4, 
                    g.circle.radius/2, g.circle.radius/2);
            gr = gr->next;
        }

        Tower *t = g.thead;
        while(t)
        {
            setColor3i(0, 0, 255);

            XDrawArc(dpy, backBuffer, gc, t->pos.x-g.circle.radius, t->pos.y-g.circle.radius, 
                    g.circle.radius*2, g.circle.radius*2, 0, 360*64);
            XFillArc(dpy, backBuffer, gc, t->pos.x-g.circle.radius, t->pos.y-g.circle.radius, 
                    g.circle.radius*2, g.circle.radius*2, 0, 360*64);
            t = t->next;
        }
    }
}

void initGrid()
{
    //g.circle.radius = (g.xres + g.yres)/100;
    g.grid.size = g.circle.radius * 2;
    //g.grid.size = g.xres / 10;
    int position = 0;
    for(int i = 0; i < g.xres; i += g.grid.size)
    {
        g.grid.column[position] = i;
        position++;
    }
    position = 0;
    for(int i = 0; i < g.yres; i += g.grid.size)
    {
        g.grid.row[position] = i;
        position++;
    }
    //g.circle.center.x = g.xres/g.grid.size;
    //g.circle.center.y = g.yres/g.grid.size;
}
