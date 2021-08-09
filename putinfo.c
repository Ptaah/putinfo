#define _GNU_SOURCE

#include <stdbool.h>
#include <unistd.h>

#include <getopt.h>
#include "info.h"

#define DBFILE "/tmp/base.sqlite"

#define MAX_ATTEMPTS 5

void debug() {

}

/*
 * TODO:
 * - main: Gestion de l'initialisation de la base de donnée sqlite
 *   (voir si la table existe avant de l'initialiser).
 *
 * - putinfo: Meilleurs résolution du timestamp en utilisation clock_gettime.
 *
 * - getinfo: Récupération de toutes les valeurs dans results.
 *   Les résultats doivent être stockés dans une  liste chainée.
 *
 * - main: Gestion du nettoyage de la base de donnée (suppression des entrées
 *   au delà d'un timestamp).
 *
 * - getinfo: Meilleure gestion de la récupération des résultats (Libération
 *   de la mémoire allouée pour results).
 *
 * - main: Vérifier la compatibilité avec l'API existante.
 *
 * - main: Ajouter des options log et debug
 *
 * - global: support d'etcd pour éviter des appels au système de fichiers
 * */

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

int main(int argc, char **argv) {
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
		if (options.reset) {
			if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
				perror("main: Unable to get time");
				return 1;
			}
			cleanup(db, ts);
		}
	}

	sqlite3_close(db);
	return 0;
}

/*vim:noexpandtab:ts=8:sw=8*/
