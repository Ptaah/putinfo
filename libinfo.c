#include "info.h"

int get_max_length(sqlite3 *db){
	int max_request_size;
	if ((max_request_size=sqlite3_limit(db, SQLITE_LIMIT_LENGTH, -1)) == -1) {
		max_request_size= 1024;
	}
	/*printf("Maximum lenght of request: %d\n", max_request_size);*/
	return max_request_size;
}

int putinfo(sqlite3 *db, int id, char *key, char *value, char *type) {
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
		     "INSERT INTO Vars(ROWID, Id, Key, Value, Type, Ts)"
		     "VALUES(NULL, %d, '%s', '%s', '%s', %ld);",
		     id, key, value, type, tp.tv_sec) > max_request_size){
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
	//strncpy(result->key, argv[0], strlen(argv[0]) + 1); 
	//strcpy(result->key, argv[0]); 

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
	//strncpy(result->value, argv[1], strlen(argv[1]) + 1); 
	//strcpy(result->value, argv[1]); 
	//bcopy(argv[1], result->value, strlen(argv[1])); 

	for (int i = 0; i < argc; i++) {
		printf("%s = %s (result=%s:%s)\n", azColName[i],
						   argv[i] ? argv[i] : "NULL",
						   result->key, result->value);
	}
	/*result->prev = result;
	result->next = (info *) malloc(sizeof(info));
	if (result->next == NULL) {
		perror("Unable to alloc result->next buffer");
		return 1;
	}
	result = result->next;*/
	return 0;
}

void print_results(info *result){
	info *walk = result;
	while (walk){
		printf("%s - %s\n", walk->key, walk->value);
		walk = walk->next;
	}
}

int getinfo(sqlite3 *db, int id, char *key, char *type) {
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
	if (snprintf(sql, max_request_size, "SELECT Key, Value, Ts FROM Vars WHERE Type='%s' AND Key='%s' AND ID='%d' ORDER BY Ts LIMIT 1;", type, key, id) > max_request_size){
		perror("getinfo: Wrong copy for request (overflow detected)");
		return 1;

	}

	results = (info *) malloc(sizeof(info));
	if (results == NULL) {
		perror("Unable to alloc buffer");
		return 1;
	}
	results->key=NULL;
	results->value=NULL;
	results->next=NULL;
	results->prev=NULL;
	results->type=NULL;

	while ((rc = sqlite3_exec(db, sql, (void *) get_result, results, &err_msg)) == SQLITE_BUSY){
		continue;
	}
	if (rc != SQLITE_OK ) {
		fprintf(stderr, "getinfo: SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		rc = 1;
	} else {
		printf("getinfo: results: %p\n", results);
		print_results(results);
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
		    "CREATE TABLE Vars(rowid INT PRIMARY KEY, Id INT, Key TEXT, Value TEXT, Type TEXT, Ts BIGINT);";

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

	if (rc != SQLITE_OK ) {
		fprintf(stderr, "init_db: SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return NULL;
	}
	printf("Init successfull\n");
	return db;
}

/*
int  getinfo(char *nom, char* valeur)
int  putinfo(char *nom, char* valeur)
*/

/*vim:noexpandtab:ts=8:sw=8*/
