#include "info.h"

int debug = 0;

char * info_gettag(void){
    char *tag;

    if ((tag=secure_getenv("TAGINFO")) == NULL)
        return "none";
    return tag;
}

char * info_getdb(void){
    char *dbfile;

    if ((dbfile = secure_getenv("DBINFO")) == NULL)
        dbfile = DBFILE;
    return dbfile;
}

info * alloc_info(void){
	info *inf;
	inf = (info *) malloc(sizeof(info));
	if (inf == NULL) {
		perror("Unable to alloc result->next buffer");
		return NULL;
	}
	inf->key=NULL;
	inf->value=NULL;
	inf->next=NULL;
	inf->prev=NULL;
	inf->tag=NULL;

	return inf;
}

void free_info(info *inf){
	if (inf->key)
		free(inf->key);
	if (inf->value)
		free(inf->value);
	if (inf->tag)
		free(inf->tag);
	free(inf);
}

int get_max_length(sqlite3 *db){
	int max_request_size;
	if ((max_request_size=sqlite3_limit(db, SQLITE_LIMIT_LENGTH, -1)) == -1) {
		max_request_size= 1024;
	}
	_log("Maximum lenght of request: %d\n", max_request_size);
	return max_request_size;
}

int putinfo(sqlite3 *db, char *key, char *value, char *tag) {
	char *err_msg = 0;
	int rc;
	int max_request_size;
	char *sql;
	struct timespec tp;
	max_request_size = get_max_length(db);

	sql = (char *) malloc(max_request_size);
	if (sql == NULL) {
		perror("putinfo: Unable to alloc buffer");
		return 1;
	}
	
	if (clock_gettime(CLOCK_REALTIME, &tp)) {
		perror("putinfo: Unable to get time");
		return 1;
	}
	if (snprintf(sql, max_request_size,
		     "INSERT INTO Vars(ROWID, Key, Value, Type, Ts)"
		     "VALUES(NULL, '%s', '%s', '%s', %ld);",
		     key, value, tag, tp.tv_sec) > max_request_size){
		perror("putinfo: Wrong copy for request (overflow detected)");
		return 1;
	}

	while ((rc = sqlite3_exec(db, sql, 0, 0, &err_msg)) == SQLITE_BUSY){
		continue;
	}

	if (rc != SQLITE_OK ) {
		fprintf(stderr, "putinfo: SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		rc = 1;
	}
	free(sql);
	return rc;
}

int get_result(info *result, int argc, char **argv, char **azColName) {
	int len;
	/* 2 fields per entry: Key value (argv[0]) and timestamp (argv[1])*/

	/* walk to the last result */
	while (result->next != NULL)
		result = result->next;

	len = strlen(argv[0]) + 1;
	result->key = (char *) malloc(len * sizeof(char));
	if (result->key == NULL) {
		perror("Unable to alloc result->key buffer");
		return 1;
	}
	if (snprintf(result->key, len, argv[0]) > len){
		perror("Wrong copy for key (overflow detected)");
		return 1;
	} 

	len = strlen(argv[1]) + 1;
	result->value = (char *) malloc(len * sizeof(char));
	if (result->value == NULL) {
		perror("Unable to alloc result->value buffer");
		return 1;
	}
	if (snprintf(result->value, len, argv[1]) > len){
		perror("Wrong copy for value (overflow detected)");
		return 1;
	} 

	for (int i = 0; i < argc; i++) {
		_log("%s = %s (result=%s:%s)\n", azColName[i],
						   argv[i] ? argv[i] : "NULL",
						   result->key, result->value);
	}
	result->next = alloc_info();
	if (result->next == NULL) {
		perror("Unable to alloc result->next buffer");
		return 1;
	}
	result->prev = result;
	return 0;
}

void free_results(info *result){
	if (result){
		free_results(result->next);
		free_info(result);
	}
}

void print_results(info *result, bool all){
	info *walk = result;
	while (walk->next != NULL){
		if (all)
			printf("%s=%s\n", walk->key, walk->value);
		else
			printf("%s\n", walk->value);
		walk = walk->next;
	}
}

int getinfo(sqlite3 *db, char *key, char *tag) {
	info *results = NULL;
	char *err_msg = 0;
	char *sql;
	int rc, max_request_size;

	max_request_size = get_max_length(db);

	sql = (char *) malloc(max_request_size);
	if (sql == NULL) {
		perror("getinfo: Unable to alloc buffer");
		return 1;
	}
	if (snprintf(sql, max_request_size,
		     "SELECT Key, Value, Max(Ts) FROM Vars " \
		     "WHERE Type='%s' AND Key LIKE'%s' group by key;",
		     tag, key) > max_request_size){
		perror("getinfo: Wrong copy for request (overflow detected)");
		return 1;

	}

	results = alloc_info();
	if (results == NULL) {
		perror("Unable to alloc buffer");
		return 1;
	}

	while ((rc = sqlite3_exec(db, sql, (void *) get_result,
				  results, &err_msg)) == SQLITE_BUSY){
		continue;
	}
	if (rc != SQLITE_OK ) {
		fprintf(stderr, "getinfo: SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		rc = 1;
	} else {
		_log("getinfo: results: %p\n", results);
		print_results(results, !strncmp(key, "%\0", 2));
		free_results(results);
	}
	free(sql);
	return rc;
}

int check_db(char *dbfile) {
	int rc;
	sqlite3 *db;

	rc = sqlite3_open_v2(dbfile, &db, SQLITE_OPEN_READONLY, NULL);
	if (rc == SQLITE_OK) {
		sqlite3_close(db);
		return 1;
	} else {
		return 0;
	}
}

sqlite3 * open_db(char *dbfile){
	int rc;
	sqlite3 *db;

	rc = sqlite3_open_v2(dbfile, &db, SQLITE_OPEN_FULLMUTEX |
					  SQLITE_OPEN_READWRITE |
					  SQLITE_OPEN_CREATE, NULL);
	if (rc == SQLITE_OK){
		if (sqlite3_busy_timeout(db, 100) != SQLITE_OK)
			perror("open_db: Unable to set busy timeout");
		return db;
	}
	return NULL;
}

/*
 * cleanup:
 *
 * Cleanup database history from the given timestamp
 *
 * */
int cleanup(sqlite3 *db, struct timespec timestamp){
	char *err_msg = 0;
	int rc;
	int max_request_size;
	char *sql;
	max_request_size = get_max_length(db);

	sql = (char *) malloc(max_request_size);
	if (sql == NULL) {
		perror("cleanup: Unable to alloc buffer");
		return 1;
	}
	
	if (snprintf(sql, max_request_size, "DELETE FROM Vars WHERE Ts < %ld;",
		     timestamp.tv_sec) > max_request_size){
		perror("cleanup: Wrong copy for request (overflow detected)");
		return 1;
	}

	while ((rc = sqlite3_exec(db, sql, 0, 0, &err_msg)) == SQLITE_BUSY){
		continue;
	}

	if (rc != SQLITE_OK ) {
		fprintf(stderr, "cleanup: SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		rc = 1;
	}
	free(sql);
	return rc;
	return 0;
}

sqlite3 * init_db(char *dbfile){
	/* Initialize Key Value Store (KVS) database */
	sqlite3 *db;
	char *err_msg = 0;
	int rc;

	db = open_db(dbfile);
	if (db == NULL) {
		perror("init_db: Unable to open db");
		return NULL;
	}
	
	char *sql = "DROP TABLE IF EXISTS Vars;"
		    "CREATE TABLE Vars(rowid INT PRIMARY KEY, " \
		    "Key TEXT, Value TEXT, Type TEXT, Ts BIGINT);";

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

	if (rc != SQLITE_OK ) {
		fprintf(stderr, "init_db: SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return NULL;
	}
	_log("Init successfull\n");
	return db;
}

/*
int  getinfo(char *nom, char* valeur)
int  putinfo(char *nom, char* valeur)
*/

// vim:noexpandtab:ts=8:sw=8
