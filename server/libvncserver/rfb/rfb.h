#ifndef RFB_H
#define RFB_H

/*
 * rfb.h - header file for RFB DDX implementation.
 */

/*
 *  Copyright (C) 2002 RealVNC Ltd.
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#if(defined __cplusplus)
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rfb/rfbproto.h>
#include <rfb/rfbregion.h>
#include <gnutls/gnutls.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifndef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
#include <pthread.h>
#if 0 /* debugging */
#define LOCK(mutex) (rfbLog("%s:%d LOCK(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_lock(&(mutex)))
#define UNLOCK(mutex) (rfbLog("%s:%d UNLOCK(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_unlock(&(mutex)))
#define MUTEX(mutex) pthread_mutex_t (mutex)
#define INIT_MUTEX(mutex) (rfbLog("%s:%d INIT_MUTEX(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_init(&(mutex),NULL))
#define TINI_MUTEX(mutex) (rfbLog("%s:%d TINI_MUTEX(%s)\n",__FILE__,__LINE__,#mutex), pthread_mutex_destroy(&(mutex)))
#define TSIGNAL(cond) (rfbLog("%s:%d TSIGNAL(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_signal(&(cond)))
#define WAIT(cond,mutex) (rfbLog("%s:%d WAIT(%s,%s)\n",__FILE__,__LINE__,#cond,#mutex), pthread_cond_wait(&(cond),&(mutex)))
#define COND(cond) pthread_cond_t (cond)
#define INIT_COND(cond) (rfbLog("%s:%d INIT_COND(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_init(&(cond),NULL))
#define TINI_COND(cond) (rfbLog("%s:%d TINI_COND(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_destroy(&(cond)))
#define IF_PTHREADS(x) x
#else
#define LOCK(mutex) pthread_mutex_lock(&(mutex));
#define UNLOCK(mutex) pthread_mutex_unlock(&(mutex));
#define MUTEX(mutex) pthread_mutex_t (mutex)
#define INIT_MUTEX(mutex) pthread_mutex_init(&(mutex),NULL)
#define TINI_MUTEX(mutex) pthread_mutex_destroy(&(mutex))
#define TSIGNAL(cond) pthread_cond_signal(&(cond))
#define WAIT(cond,mutex) pthread_cond_wait(&(cond),&(mutex))
#define COND(cond) pthread_cond_t (cond)
#define INIT_COND(cond) pthread_cond_init(&(cond),NULL)
#define TINI_COND(cond) pthread_cond_destroy(&(cond))
#define IF_PTHREADS(x) x
#endif
#else
#define LOCK(mutex)
#define UNLOCK(mutex)
#define MUTEX(mutex)
#define INIT_MUTEX(mutex)
#define TINI_MUTEX(mutex)
#define TSIGNAL(cond)
#define WAIT(cond,mutex) this_is_unsupported
#define COND(cond)
#define INIT_COND(cond)
#define TINI_COND(cond)
#define IF_PTHREADS(x)
#endif

/* end of stuff for autoconf */

/* if you use pthreads, but don't define LIBVNCSERVER_HAVE_LIBPTHREAD, the structs
   get all mixed up. So this gives a linker error reminding you to compile
   the library and your application (at least the parts including rfb.h)
   with the same support for pthreads. */
#if 0
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
#ifdef HAVE_LIBZ
#define rfbInitServer rfbInitServerWithPthreadsAndZRLE
#else
#define rfbInitServer rfbInitServerWithPthreadsButWithoutZRLE
#endif
#else
#ifdef HAVE_LIBZ
#define rfbInitServer rfbInitServerWithoutPthreadsButWithZRLE
#else
#define rfbInitServer rfbInitServerWithoutPthreadsAndZRLE
#endif
#endif
#endif

struct _rfbClientRec;
struct _rfbScreenInfo;
struct rfbCursor;

enum rfbNewClientAction {
	RFB_CLIENT_ACCEPT,
	RFB_CLIENT_ON_HOLD,
	RFB_CLIENT_REFUSE
};

typedef void (*KbdAddEventProcPtr) (rfbBool down, rfbKeySym keySym, struct _rfbClientRec* cl);
typedef void (*PtrAddEventProcPtr) (int buttonMask, int x, int y, struct _rfbClientRec* cl);
typedef void (*SetXCutTextProcPtr) (char* str,int len, struct _rfbClientRec* cl);
typedef enum rfbNewClientAction (*PasswordCheckProcPtr)(struct _rfbClientRec* cl,const char* encryptedPassWord,int len);
typedef enum rfbNewClientAction (*NewClientHookPtr)(struct _rfbClientRec* cl);
typedef enum rfbNewClientAction (*AuthenticatedClientHookPtr)(struct _rfbClientRec* cl);

typedef struct {
  uint32_t count;
  rfbBool is16; /* is the data format short? */
  union {
    uint8_t* bytes;
    uint16_t* shorts;
  } data; /* there have to be count*3 entries */
} rfbColourMap;

/*
 * Per-screen (framebuffer) structure.  There can be as many as you wish,
 * each serving different clients. However, you have to call
 * rfbProcessEvents for each of these.
 */

typedef struct _rfbScreenInfo
{
    int width;
    int paddedWidthInBytes;
    int height;
    int depth;
    int bitsPerPixel;
    int sizeInBytes;

    rfbPixel blackPixel;
    rfbPixel whitePixel;

    /* some screen specific data can be put into a struct where screenData
     * points to. You need this if you have more than one screen at the
     * same time while using the same functions.
     */
    void* screenData;
  
    /* additions by libvncserver */

    rfbPixelFormat rfbServerFormat;
    rfbColourMap colourMap; /* set this if rfbServerFormat.trueColour==FALSE */
    const char* desktopName;
    char rfbThisHost[255];

    rfbBool autoPort;
    int rfbPort;
    SOCKET rfbListenSock;
    int maxSock;
    int maxFd;
    fd_set allFds;

    rfbBool socketInitDone;
    SOCKET inetdSock;
    rfbBool inetdInitDone;

    int rfbMaxClientWait;

    int securityTypes[RFB_MAX_N_SECURITY_TYPES];
    int nSecurityTypes;
    int authTypes[RFB_MAX_N_AUTH_TYPES];
    int nAuthTypes;
    PasswordCheckProcPtr passwordCheck;
    gnutls_anon_server_credentials anonCredentials;
    gnutls_dh_params dhParams;

    /* send only this many rectangles in one update */
    int maxRectsPerUpdate;
    /* this is the amount of milliseconds to wait at least before sending
     * an update. */
    int rfbDeferUpdateTime;
    char* rfbScreen;
    rfbBool rfbAlwaysShared;
    rfbBool rfbNeverShared;
    rfbBool rfbDontDisconnect;
    struct _rfbClientRec* rfbClientHead;

    /* cursor */
    int cursorX, cursorY,underCursorBufferLen;
    char* underCursorBuffer;
    struct rfbCursor* cursor;

    /* the frameBufferhas to be supplied by the serving process.
     * The buffer will not be freed by 
     */
    char* frameBuffer;
    KbdAddEventProcPtr kbdAddEvent;
    PtrAddEventProcPtr ptrAddEvent;
    SetXCutTextProcPtr setXCutText;
  
    /* newClientHook is called just after a new client is created */
    NewClientHookPtr newClientHook;

    /* authenticatedClientHook is called just after the client has
     * been authenticated
     */
    AuthenticatedClientHookPtr authenticatedClientHook;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    MUTEX(cursorMutex);
    rfbBool backgroundLoop;
#endif

} rfbScreenInfo, *rfbScreenInfoPtr;


/*
 * rfbTranslateFnType is the type of translation functions.
 */

typedef void (*rfbTranslateFnType)(char *table, rfbPixelFormat *in,
                                   rfbPixelFormat *out,
                                   char *iptr, char *optr,
                                   int bytesBetweenInputLines,
                                   int width, int height);


/* region stuff */

typedef sraRegion* sraRegionPtr;
typedef sraRect* sraRectPtr;

/*
 * Per-client structure.
 */

typedef void (*ClientGoneHookPtr)(struct _rfbClientRec* cl);

typedef struct _rfbClientRec {
  
    /* back pointer to the screen */
    rfbScreenInfoPtr screen;
  
    /* private data. You should put any application client specific data
     * into a struct and let clientData point to it. Don't forget to
     * free the struct via clientGoneHook!
     *
     * This is useful if the IO functions have to behave client specific.
     */
    void* clientData;
    ClientGoneHookPtr clientGoneHook;

    SOCKET sock;
    gnutls_session tlsSession;
    rfbBool useTLS;
    char *host;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    pthread_t client_thread;
#endif
                                /* Possible client states: */
    enum {
        RFB_PROTOCOL_VERSION,   /* establishing protocol version */
        RFB_SECURITY_TYPE,      /* negotiating security type */
	RFB_TLS_HANDSHAKE,      /* completing the TLS handshake */
	RFB_AUTH_TYPE,          /* negotiating authentication type */
        RFB_AUTHENTICATION,     /* authenticating */
        RFB_AUTH_DEFERRED,      /* authentication deferred */
        RFB_INITIALISATION,     /* sending initialisation messages */
        RFB_NORMAL              /* normal protocol messages */
    } state;

    rfbBool onHold;
    rfbBool readyForSetColourMapEntries;
    rfbBool useCopyRect;
    int minorVersion;
    int preferredEncoding;
    int correMaxWidth, correMaxHeight;

    rfbBool viewOnly;

    /* The following member is only used during VNC authentication */
    uint8_t authChallenge[CHALLENGESIZE];

    /* The following members represent the update needed to get the client's
       framebuffer from its present state to the current state of our
       framebuffer.

       If the client does not accept CopyRect encoding then the update is
       simply represented as the region of the screen which has been modified
       (modifiedRegion).

       If the client does accept CopyRect encoding, then the update consists of
       two parts.  First we have a single copy from one region of the screen to
       another (the destination of the copy is copyRegion), and second we have
       the region of the screen which has been modified in some other way
       (modifiedRegion).

       Although the copy is of a single region, this region may have many
       rectangles.  When sending an update, the copyRegion is always sent
       before the modifiedRegion.  This is because the modifiedRegion may
       overlap parts of the screen which are in the source of the copy.

       In fact during normal processing, the modifiedRegion may even overlap
       the destination copyRegion.  Just before an update is sent we remove
       from the copyRegion anything in the modifiedRegion. */

    sraRegionPtr copyRegion;	/* the destination region of the copy */
    int copyDX, copyDY;		/* the translation by which the copy happens */

    sraRegionPtr modifiedRegion;

    sraRect lastDrawnCursorBounds;

    /* As part of the FramebufferUpdateRequest, a client can express interest
       in a subrectangle of the whole framebuffer.  This is stored in the
       requestedRegion member.  In the normal case this is the whole
       framebuffer if the client is ready, empty if it's not. */

    sraRegionPtr requestedRegion;

    /* The following member represents the state of the "deferred update" timer
       - when the framebuffer is modified and the client is ready, in most
       cases it is more efficient to defer sending the update by a few
       milliseconds so that several changes to the framebuffer can be combined
       into a single update. */

      struct timeval startDeferring;

    /* translateFn points to the translation function which is used to copy
       and translate a rectangle from the framebuffer to an output buffer. */

    rfbTranslateFnType translateFn;
    char *translateLookupTable;
    rfbPixelFormat format;

    /*
     * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
     * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
     * means 8K minimum.
     */

#define UPDATE_BUF_SIZE 30000

    char updateBuf[UPDATE_BUF_SIZE];
    int ublen;

    /* statistics */

    int rfbBytesSent[MAX_ENCODINGS];
    int rfbRectanglesSent[MAX_ENCODINGS];
    int rfbLastRectMarkersSent;
    int rfbLastRectBytesSent;
    int rfbCursorShapeBytesSent;
    int rfbCursorShapeUpdatesSent;
    int rfbCursorPosBytesSent;
    int rfbCursorPosUpdatesSent;
    int rfbFramebufferUpdateMessagesSent;
    int rfbRawBytesEquivalent;
    int rfbKeyEventsRcvd;
    int rfbPointerEventsRcvd;

#ifdef HAVE_LIBZ
    /* zlib encoding -- necessary compression state info per client */

    struct z_stream_s compStream;
    rfbBool compStreamInited;
    uint32_t zlibCompressLevel;

#ifdef HAVE_LIBJPEG
    /* tight encoding -- preserve zlib streams' state for each client */
    z_stream zsStruct[4];
    rfbBool zsActive[4];
    int zsLevel[4];
    int tightCompressLevel;
    int tightQualityLevel;
#endif
#endif

    rfbBool enableLastRectEncoding;   /* client supports LastRect encoding */
    rfbBool enableCursorUpdates;      /* client supports cursor position and shape updates */
    rfbBool useRichCursorEncoding;    /* rfbEncodingRichCursor is preferred */
    rfbBool cursorWasChanged;         /* cursor shape update should be sent */
    rfbBool cursorWasMoved;           /* cursor position update should be sent */

    rfbBool useNewFBSize;             /* client supports NewFBSize encoding */
    rfbBool newFBSizePending;         /* framebuffer size was changed */

    struct _rfbClientRec *prev;
    struct _rfbClientRec *next;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    /* whenever a client is referenced, the refCount has to be incremented
       and afterwards decremented, so that the client is not cleaned up
       while being referenced.
       Use the functions rfbIncrClientRef(cl) and rfbDecrClientRef(cl);
    */
    int refCount;
    MUTEX(refCountMutex);
    COND(deleteCond);

    MUTEX(outputMutex);
    MUTEX(updateMutex);
    COND(updateCond);
#endif

#ifdef HAVE_LIBZ
    void* zrleData;
#endif

} rfbClientRec, *rfbClientPtr;

/*
 * This macro is used to test whether there is a framebuffer update needing to
 * be sent to the client.
 */

#define FB_UPDATE_PENDING(cl)                                                            \
     (((cl)->enableCursorUpdates && ((cl)->cursorWasMoved || (cl)->cursorWasChanged)) || \
     ((cl)->useNewFBSize && (cl)->newFBSizePending) ||                                   \
     !sraRgnEmpty((cl)->copyRegion) || !sraRgnEmpty((cl)->modifiedRegion))

/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap24(l) ((((l) & 0xff) << 16) | (((l) >> 16) & 0xff) | \
                   (((l) & 0x00ff00)))

#define Swap32(l) (((l) >> 24) | \
                   (((l) & 0x00ff0000) >> 8)  | \
                   (((l) & 0x0000ff00) << 8)  | \
                   ((l) << 24))


extern char rfbEndianTest;

#define Swap16IfLE(s) (rfbEndianTest ? Swap16(s) : (s))
#define Swap24IfLE(l) (rfbEndianTest ? Swap24(l) : (l))
#define Swap32IfLE(l) (rfbEndianTest ? Swap32(l) : (l))

/* sockets.c */

extern int rfbMaxClientWait;

extern void rfbInitSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbCloseClient(rfbClientPtr cl);
extern int ReadExact(rfbClientPtr cl, char *buf, int len);
extern int ReadExactTimeout(rfbClientPtr cl, char *buf, int len,int timeout);
extern int WriteExact(rfbClientPtr cl, const char *buf, int len);
extern void rfbProcessNewConnection(rfbScreenInfoPtr rfbScreen);
extern void rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec);
extern int rfbConnect(rfbScreenInfoPtr rfbScreen, char* host, int port);
extern int ConnectToTcpAddr(char* host, int port);
extern int ListenOnTCPPort(int port);

/* rfbserver.c */

extern rfbClientPtr pointerClient;


/* Routines to iterate over the client list in a thread-safe way.
   Only a single iterator can be in use at a time process-wide. */
typedef struct rfbClientIterator *rfbClientIteratorPtr;

extern void rfbClientListInit(rfbScreenInfoPtr rfbScreen);
extern rfbClientIteratorPtr rfbGetClientIterator(rfbScreenInfoPtr rfbScreen);
extern rfbClientPtr rfbClientIteratorHead(rfbClientIteratorPtr iterator);
extern rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator);
extern void rfbReleaseClientIterator(rfbClientIteratorPtr iterator);

extern void rfbIncrClientRef(rfbClientPtr cl);
extern void rfbDecrClientRef(rfbClientPtr cl);

extern void rfbNewClientConnection(rfbScreenInfoPtr rfbScreen,int sock);
extern rfbClientPtr rfbNewClient(rfbScreenInfoPtr rfbScreen,int sock);
extern void rfbClientConnectionGone(rfbClientPtr cl);
extern void rfbProcessClientMessage(rfbClientPtr cl);
extern void rfbProcessClientInitMessage(rfbClientPtr cl);
extern void rfbClientConnFailed(rfbClientPtr cl, char *reason);
extern rfbBool rfbSendFramebufferUpdate(rfbClientPtr cl, sraRegionPtr updateRegion);
extern rfbBool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);
extern rfbBool rfbSendUpdateBuf(rfbClientPtr cl);
extern void rfbSendServerCutText(rfbScreenInfoPtr rfbScreen,char *str, int len);
extern rfbBool rfbSendCopyRegion(rfbClientPtr cl,sraRegionPtr reg,int dx,int dy);
extern rfbBool rfbSendLastRectMarker(rfbClientPtr cl);
extern rfbBool rfbSendNewFBSize(rfbClientPtr cl, int w, int h);
extern rfbBool rfbSendSetColourMapEntries(rfbClientPtr cl, int firstColour, int nColours);
extern void rfbSendBell(rfbScreenInfoPtr rfbScreen);

void rfbGotXCutText(rfbScreenInfoPtr rfbScreen, char *str, int len);

/* translate.c */

extern rfbBool rfbEconomicTranslate;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
                             rfbPixelFormat *out,
                             char *iptr, char *optr,
                             int bytesBetweenInputLines,
                             int width, int height);
extern rfbBool rfbSetTranslateFunction(rfbClientPtr cl);
extern rfbBool rfbSetClientColourMap(rfbClientPtr cl, int firstColour, int nColours);
extern void rfbSetClientColourMaps(rfbScreenInfoPtr rfbScreen, int firstColour, int nColours);

/* httpd.c */

extern void httpInitSockets(rfbScreenInfoPtr rfbScreen);
extern void httpCheckFds(rfbScreenInfoPtr rfbScreen);



/* auth.c */

extern void rfbAuthInitScreen(rfbScreenInfoPtr rfbScreen);
extern void rfbAuthCleanupScreen(rfbScreenInfoPtr rfbScreen);
extern void rfbAuthNewClient(rfbClientPtr cl);
extern void rfbAuthCleanupClient(rfbClientPtr cl);
extern void rfbAuthProcessSecurityTypeMessage(rfbClientPtr cl);
extern void rfbAuthProcessAuthTypeMessage(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);
extern void rfbAuthProcessTLSHandshake(rfbClientPtr cl);
extern void rfbAuthPasswordChecked(rfbClientPtr cl, enum rfbNewClientAction result);


/* rre.c */

extern rfbBool rfbSendRectEncodingRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* corre.c */

extern rfbBool rfbSendRectEncodingCoRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* hextile.c */

extern rfbBool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
                                       int h);


#ifdef HAVE_LIBZ
/* zlib.c */

/* Minimum zlib rectangle size in bytes.  Anything smaller will
 * not compress well due to overhead.
 */
#define VNC_ENCODE_ZLIB_MIN_COMP_SIZE (17)

/* Set maximum zlib rectangle size in pixels.  Always allow at least
 * two scan lines.
 */
#define ZLIB_MAX_RECT_SIZE (128*256)
#define ZLIB_MAX_SIZE(min) ((( min * 2 ) > ZLIB_MAX_RECT_SIZE ) ? \
			    ( min * 2 ) : ZLIB_MAX_RECT_SIZE )

extern rfbBool rfbSendRectEncodingZlib(rfbClientPtr cl, int x, int y, int w,
				    int h);

#ifdef HAVE_LIBJPEG
/* tight.c */

#define TIGHT_DEFAULT_COMPRESSION  6

extern rfbBool rfbTightDisableGradient;

extern void rfbTightCleanup(void);

extern int rfbNumCodedRectsTight(rfbClientPtr cl, int x,int y,int w,int h);
extern rfbBool rfbSendRectEncodingTight(rfbClientPtr cl, int x,int y,int w,int h);
#endif
#endif


/* cursor.c */

typedef struct rfbCursor {
    /* set this to true if LibVNCServer has to free this cursor */
    rfbBool cleanup, cleanupSource, cleanupMask, cleanupRichSource;
    unsigned char *source;			/* points to bits */
    unsigned char *mask;			/* points to bits */
    unsigned short width, height, xhot, yhot;	/* metrics */
    unsigned short foreRed, foreGreen, foreBlue; /* device-independent colour */
    unsigned short backRed, backGreen, backBlue; /* device-independent colour */
    unsigned char *richSource; /* source bytes for a rich cursor */
} rfbCursor, *rfbCursorPtr;
extern unsigned char rfbReverseByte[0x100];

extern rfbBool rfbSendCursorShape(rfbClientPtr cl/*, rfbScreenInfoPtr pScreen*/);
extern rfbBool rfbSendCursorPos(rfbClientPtr cl);
extern void rfbConvertLSBCursorBitmapOrMask(int width,int height,unsigned char* bitmap);
extern rfbCursorPtr rfbMakeXCursor(int width,int height,const char* cursorString,const char* maskString);
extern char* rfbMakeMaskForXCursor(int width,int height,char* cursorString);
extern void MakeXCursorFromRichCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void MakeRichCursorFromXCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void rfbFreeCursor(rfbCursorPtr cursor);
extern rfbBool rfbGetCursorBounds(rfbScreenInfoPtr screen, sraRectPtr bounds);
extern void rfbDrawCursor(rfbScreenInfoPtr rfbScreen, sraRectPtr bounds);
extern void rfbUndrawCursor(rfbScreenInfoPtr rfbScreen, sraRectPtr bounds);
extern void rfbSetCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr c,rfbBool freeOld);
extern void rfbSetCursorPosition(rfbScreenInfoPtr rfbScreen, rfbClientPtr client, int x, int y);

/* cursor handling for the pointer */
extern void defaultPtrAddEvent(int buttonMask,int x,int y,rfbClientPtr cl);

/* zrle.c */
#ifdef HAVE_LIBZ
extern rfbBool rfbSendRectEncodingZRLE(rfbClientPtr cl, int x, int y, int w,int h);
extern void FreeZrleData(rfbClientPtr cl);
#endif

/* stats.c */

extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);

/* main.c */

extern void rfbLogEnable(int enabled);
typedef void (*rfbLogProc)(const char *format, ...);
extern rfbLogProc rfbLog, rfbErr;
extern void rfbLogPerror(const char *str);

void rfbScheduleCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy);
void rfbScheduleCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy);

void rfbDoCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy);
void rfbDoCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy);

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2);
void rfbMarkRegionAsModified(rfbScreenInfoPtr rfbScreen,sraRegionPtr modRegion);
enum rfbNewClientAction defaultNewClientHook(rfbClientPtr cl);

/* functions to make a vnc server */
extern rfbScreenInfoPtr rfbGetScreen(int* argc,char** argv,
 int width,int height,int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);
extern void rfbInitServer(rfbScreenInfoPtr rfbScreen);
extern void rfbNewFramebuffer(rfbScreenInfoPtr rfbScreen,char *framebuffer,
 int width,int height, int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);

extern void rfbScreenCleanup(rfbScreenInfoPtr screenInfo);

/* functions to accept/refuse a client that has been put on hold
   by a NewClientHookPtr function. Must not be called in other
   situations. */
extern void rfbStartOnHoldClient(rfbClientPtr cl);
extern void rfbRefuseOnHoldClient(rfbClientPtr cl);

/* call one of these two functions to service the vnc clients.
 usec are the microseconds the select on the fds waits.
 if you are using the event loop, set this to some value > 0, so the
 server doesn't get a high load just by listening. */

extern void rfbRunEventLoop(rfbScreenInfoPtr screenInfo, long usec, rfbBool runInBackground);
extern void rfbProcessEvents(rfbScreenInfoPtr screenInfo,long usec);
extern void rfbUpdateClient(rfbClientPtr cl);

extern void rfbAddSecurityType(rfbScreenInfoPtr rfbScreen, int securityType);
extern void rfbClearSecurityTypes(rfbScreenInfoPtr rfbScreen);
extern void rfbAddAuthType(rfbScreenInfoPtr rfbScreen, int securityType);
extern void rfbClearAuthTypes(rfbScreenInfoPtr rfbScreen);

#endif

#if(defined __cplusplus)
}
#endif
