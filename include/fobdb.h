
typedef struct loginrec_s {
    char *password;
    objhandle_t object;
} loginrec_t;

typedef void dbhandle_t;

extern dbhandle_t *newdbh(void);
extern void deletedbh(dbhandle_t *db_);
extern bool objectdb_get(dbhandle_t *db_, word handle, object_t *o);
extern bool loadlogindb(dbhandle_t *);
extern void closelogindb(dbhandle_t *);
extern bool verifylogin(dbhandle_t *, char *, char *, objhandle_t *);
extern bool loadobjectdb(dbhandle_t *);
extern void closeobjectdb(dbhandle_t *);
