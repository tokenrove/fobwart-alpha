
typedef struct loginrec_s {
    char *password;
    objhandle_t object;
} loginrec_t;

typedef void dbhandle_t;

extern dbhandle_t *newdbh(void);
extern void deletedbh(dbhandle_t *db_);

extern bool loadobjectdb(dbhandle_t *);
extern void closeobjectdb(dbhandle_t *);
extern bool objectdb_get(dbhandle_t *db_, objhandle_t handle, object_t *o);
extern bool objectdb_put(dbhandle_t *db_, objhandle_t handle, object_t *o);
extern bool objectdb_remove(dbhandle_t *dbh_, objhandle_t handle);

extern bool loadroomdb(dbhandle_t *db_);
extern void closeroomdb(dbhandle_t *db_);
extern bool roomdb_get(dbhandle_t *db_, roomhandle_t handle, room_t *room);
extern bool roomdb_getall(dbhandle_t *db_, d_set_t *rooms);
extern bool roomdb_put(dbhandle_t *db_, roomhandle_t handle, room_t *room);
extern bool roomdb_remove(dbhandle_t *dbh_, roomhandle_t handle);

extern bool loadlogindb(dbhandle_t *);
extern void closelogindb(dbhandle_t *);
extern bool logindb_remove(dbhandle_t *db_, char *handle);
extern bool logindb_put(dbhandle_t *db_, char *handle, loginrec_t *lrec);
extern bool logindb_get(dbhandle_t *db_, char *handle, loginrec_t *lrec);
extern bool verifylogin(dbhandle_t *, char *, char *, objhandle_t *);
