/*
 * fobnet.h ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
 */

typedef void nethandle_t;

/* Network related constants.
 * If you change these, please update HACKING */
enum { PACK_YEAWHAW = 0,	/* server-side handshake */
       PACK_IRECKON = 1,	/* client-side handshake */
       PACK_AYUP = 2,		/* general ack packet */
       PACK_LOGIN = 3,		/* client login request */
       PACK_SYNC = 4,		/* reserved */
       PACK_EVENT = 5,		/* packet contains object event */
       PACK_FRAME = 6,		/* client blocks for event framing */
       PACK_GETOBJECT = 7,	/* client request for object */
       PACK_GETROOM = 8,	/* client request for room */
       PACK_OBJECT = 9,		/* packet contains an object */
       PACK_ROOM = 10,		/* packet contains a room */
       PACK_GETRESLIST = 11,	/* client request for resource checksums */
       PACK_RESLIST = 12,       /* packet contains resource checksums */
       PACK_GETFILE = 13,	/* client request for updated resource */
       PACK_FILE = 14		/* packet contains resource file */
};


/* packet_t
 * Packet structure. */
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

	reslist_t reslist;

	string_t string;

	struct {
	    string_t name;
	    dword length, checksum;
	    byte *data;
	} file;
    } body;
} packet_t;


/* network.c */
extern bool net_readpack(nethandle_t *, packet_t *p);
extern bool net_writepack(nethandle_t *, packet_t p);
extern void net_close(nethandle_t *);

/* clinet.c */
extern bool net_syncevents(nethandle_t *nh, eventstack_t *evsk);
extern bool net_login(nethandle_t *nh, char *uname, char *password,
                      objhandle_t *localobj);
extern bool net_newclient(nethandle_t **nh_, char *servname, int port);

/* servnet.c */
extern bool net_newserver(nethandle_t **, int port);
extern void net_servselect(nethandle_t *servnh_, bool *servisactive,
                           d_set_t *clients, dword *activeclients);
extern bool net_accept(nethandle_t *servnh_, nethandle_t **clinh_);

/* EOF fobnet.h */
