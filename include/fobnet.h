/*
 * fobnet.h ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
 */

typedef void nethandle_t;

/* Network related */
enum { PACK_YEAWHAW = 0, PACK_IRECKON, PACK_AYUP,
       PACK_LOGIN, PACK_SYNC, PACK_EVENT, PACK_FRAME,
       PACK_GETOBJECT, PACK_GETROOM, PACK_OBJECT, PACK_ROOM };

typedef struct packet_s {
    byte type;
    union {
        byte handshake;
        word handle;
        struct {
            string_t name, password;
        } login;
        event_t event;
        object_t object;
        room_t room;
    } body;
} packet_t;

extern bool net_readpack(nethandle_t *, packet_t *p);
extern bool net_writepack(nethandle_t *, packet_t p);
extern void net_close(nethandle_t *);

extern bool net_syncevents(nethandle_t *nh, eventstack_t *evsk);
extern bool net_login(nethandle_t *nh, char *uname, char *password,
                      objhandle_t *localobj);
extern bool net_newclient(nethandle_t **nh_, char *servname, int port);

extern bool net_newserver(nethandle_t **, int port);
extern void net_servselect(nethandle_t *servnh_, bool *servisactive,
                           d_set_t *clients, dword *activeclients);
extern bool net_accept(nethandle_t *servnh_, nethandle_t **clinh_);

/* EOF fobnet.h */
