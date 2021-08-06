#define _GNU_SOURCE

#include <sqlite3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include <getopt.h>

#define DBFILE "/tmp/base.sqlite"

#define MAX_ATTEMPTS 5

char **results = NULL;

typedef struct _info {
	/* Key from info */
	char *key;
	/* Value from info */
	char *value;
	/* Timestamp of the record */
	long ts;
	/* Info type */
	const char* type;
	/* Id */
	long id;
} info;

/*
 * TODO:
 * - main: Gestion de l'initialisation de la base de donnée sqlite
 *   (voir si la table existe avant de l'initialiser).
 *
 * - putinfo: Meilleurs résolution du timestamp en utilisation clock_gettime.
 *
 * - getinfo: Récupération de toutes les valeurs dans results.
 *
 * - main: Gestion du nettoyage de la base de donnée (suppression des entrées
 *   au delà d'un timestamp).
 *
 * - getinfo: Meilleure gestion de la récupération des résultats (Libération
 *   de la mémoire allouée pour results).
 *
 * - main: Vérifier la compatibilité avec l'API existante.
 * */

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
	
	if (clock_gettime(CLOCK_MONOTONIC, &tp)) {
		perror("putinfo: Unable to get time");
		return 1;
	}
	//snprintf(sql, max_request_size, "INSERT INTO Vars VALUES(%d, '%s', '%s', '%s', %ld);", id, key, value, type, time(NULL));
	snprintf(sql, max_request_size, "INSERT INTO Vars VALUES(%d, '%s', '%s', '%s', %ld);", id, key, value, type, tp.tv_sec);
	//snprintf(sql, max_request_size, "INSERT INTO Vars VALUES(%d, '%s', '%s', '%s', %ld.%ld);", id, key, value, type, tp.tv_sec, tp.tv_nsec);

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

int get_result(char ***result, int argc, char **argv, char **azColName) {
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
		perror("getinfo: Unable to alloc buffer");
		return 1;
	}
	snprintf(sql, max_request_size, "SELECT Value, Ts FROM Vars WHERE Type='%s' AND Key='%s' AND ID='%d' ORDER BY Ts LIMIT 1;", type, key, id);

	while ((rc = sqlite3_exec(db, sql, (void *) get_result, &results, &err_msg)) == SQLITE_BUSY){
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

	rc = sqlite3_open_v2(dbfile, &db, SQLITE_OPEN_FULLMUTEX |
					  SQLITE_OPEN_READONLY, NULL);
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
		if (sqlite3_busy_timeout(db, (rand() + 100) % 1000) != SQLITE_OK)
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
		    "CREATE TABLE Vars(Id INT, Key TEXT, Value TEXT, Type TEXT, Ts BIGINT);";

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

struct _options {
    char *database;
    bool all;
    bool reset;
    bool explain;
    char *variable;
    char *value;
} options;

void help(char **argv){
	printf("%s [-a] [-d <directory>] [-e] [-r] variable [value]\n"
	       "    -a --all:            Display all variables.\n"
	       "    -d --database=<arg>: Database file.\n"
	       "    -e --explain:        Explain what will be done.\n"
	       "    -r --reset:          Reset all variables\n", argv[0]);
}

int main(int argc, char **argv, char **arge) {
	char opt;
	char *dbfile = NULL;
	sqlite3 *db = NULL;
	struct timespec ts;
	/*
	 * Options à gérer:
	 * -a: Donne l'ensemble des clefs/valeur de la base de donnée
	 * -d <arg>: Répertoire info à utiliser (gestion du dbinfo) (GLOBAL_TMPDIR ou /tmp + SLURM_JOB_ID) Si ENVIRONMENT=BATCH
	 * -e: ecriture sur stderr du résultat de la commande putinfo
	 * -r: purge complète si pas d'argument, et RAZ de la variable si un argument (reset)
	 * */

	static struct option long_options[] =
	{
	    {"all", no_argument, NULL, 'a'},
	    {"dir", required_argument, NULL, 'd'},
	    {"explain", no_argument, NULL, 'e'},
	    {"reset", optional_argument, NULL, 'r'},
	    {NULL, 0, NULL, 0}
	};

	while ((opt = getopt_long(argc, argv, "ad:erh", long_options, NULL)) != -1)
	{
	    // check to see if a single character or long option came through
	    switch (opt)
	    {
		 // short option 'a'
		 case 'a':
		     options.all = true;
		     break;
		 // short option 'd'
		 case 'd':
		     options.database = optarg; // or copy it if you want to
		     break;
		 // short option 'e'
		 case 'e':
		     options.explain = true; // or copy it if you want to
		     break;
		 // short option 'r'
		 case 'r':
		     options.reset = true; // or copy it if you want to
		     break;
		 default:
		     help(argv);
		     exit(1);
	    }
	}

	if (optind < argc)
		options.variable = argv[optind++];
	else
		options.variable = NULL;
	if (optind < argc)
		options.value = argv[optind++];
	else
		options.value = NULL;
	if (optind < argc){
		fprintf(stderr, "Too many argument.\n");
		help(argv);
		exit(1);
	}

	printf("main: all=%d directory=%s explain=%d reset=%d variable=%s value=%s\n",
		options.all, options.database, options.explain, options.reset,
		options.variable, options.value);

	if (options.database == NULL){
		if ((dbfile = secure_getenv("DBINFO")) == NULL)
			dbfile = DBFILE;
	} else {
		dbfile = options.database;
	}

	printf("main: opening db from: %s\n", dbfile);

	if (! check_db(dbfile)) {
		db = init_db(dbfile);
	} else {
		db = open_db(dbfile);
	}
	if (db == NULL) {
		perror("main: Unable to open db");
		exit(1);
	}

	if (options.variable){
		if (options.value)
			putinfo(db, 1, options.variable, options.value, "INTER");
		else
			getinfo(db, 1, options.variable, "INTER");
	} else {
		if (options.all)
			getinfo(db, 1, "%", "INTER");
		if (options.reset)
			cleanup(db, ts);
	}

	sqlite3_close(db);
	return 0;
}

/*vim:noexpandtab:ts=8:sw=8*/
