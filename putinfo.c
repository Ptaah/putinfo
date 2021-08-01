#include <sqlite3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define DBFILE "/tmp/base.sqlite"

char **results = NULL;

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
	max_request_size = get_max_length(db);

	sql = (char *) malloc(max_request_size);
	if (sql == NULL) {
		perror("Unable to alloc buffer");
		return 1;
	}
	snprintf(sql, max_request_size, "INSERT INTO Vars VALUES(%d, '%s', '%s', '%s', %ld);", id, key, value, type, time(NULL));

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    
	if (rc != SQLITE_OK ) {
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);        
		sqlite3_close(db);
		rc = 1;
	} 
	free(sql);
	return rc;
}

int get_result(char ***result, int argc, char **argv, 
                    char **azColName) {
	/* 2 fields per entry: Key value (argv[0]) and timestamp (argv[1])*/
	bool first = false;

	if (results == NULL) {
		results = (char **) malloc(argc * sizeof(char*));
		if (results == NULL) {
			perror("Unable to alloc buffer");
			return 1;
		}
		for (int i=0 ; i<argc ; i++) {
			results[i] = (char *) malloc(strlen(argv[i]) * sizeof(char));
			if (results[i] == NULL) {
				perror("Unable to alloc buffer");
				return 1;
			}
		}
		first = true;
	}
	if (first || (atol(argv[1]) > atol(results[1]))){
		bcopy(argv[0], results[0], strlen(argv[0]) + 1);
		bcopy(argv[1], results[1], strlen(argv[1]) + 1);
	}
	for (int i = 0; i < argc; i++) {
		printf("%s = %s (result=%s)\n", azColName[i], argv[i] ? argv[i] : "NULL", results[i]);
	}
	return 0;
}

void print_results(char **results){
	printf("%s - %s\n", results[0], results[1]);
}

int getinfo(sqlite3 *db, int id, char *key, char *type) {
	char *err_msg = 0;
	char *sql;
	int rc, max_request_size;

	max_request_size = get_max_length(db);

	sql = (char *) malloc(max_request_size);
	if (sql == NULL) {
		perror("Unable to alloc buffer");
		return 1;
	}
	snprintf(sql, max_request_size, "SELECT Value, Ts FROM Vars WHERE Type='%s' AND Key='%s' AND ID='%d';", type, key, id);

        rc = sqlite3_exec(db, sql, (void *) get_result, &results, &err_msg);
	if (rc != SQLITE_OK ) {
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);        
		sqlite3_close(db);
		rc = 1;
	} 
	free(sql);

	printf("results: %p\n", results);
	print_results(results);
	return rc;
}

int main(int argn, char **argv, char **arge) {
	sqlite3 *db;
	char *err_msg = 0;
	int rc;

	if (sqlite3_open(DBFILE, &db) != SQLITE_OK) {
		perror("Unable to open db");
		exit(1);
	}
	
	char *sql = "DROP TABLE IF EXISTS Vars;" 
		    "CREATE TABLE Vars(Id INT, Key TEXT, Value TEXT, Type TEXT, Ts BIGINT);";

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    
	if (rc != SQLITE_OK ) {
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);        
		sqlite3_close(db);
		return 1;
	} 

	printf("Init successfull\n");
	putinfo(db, 1, "Test", "41400", "INTER");
	putinfo(db, 1, "Test", "bouuu", "INTER");
	putinfo(db, 1, "Toto", "bouuu", "INTER");
	putinfo(db, 1, "Test", "41400", "BATCH");

	getinfo(db, 1, "Test", "INTER");
	printf("Getinfo successfull\n");
    
	sqlite3_close(db);
	return 0;
}

