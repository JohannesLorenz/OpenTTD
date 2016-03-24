#include <getopt.h>
#include <iostream>
#include "mkgraph_options.h"

options::options(int argc, char** argv)
{
	int c;
	int digit_optind = 0;

	while (true) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"help",		no_argument,		0, 'h'},
			{"version",		no_argument,		0, 'v'},
			{"list-cargo",		no_argument,		0, 'l'},
			{"cargo",		required_argument,	0, 'c'},
			{"hide-short-trains",	no_argument,		0, 's'},
			{"hide-express-trains",	no_argument,		0, 'e'},
			{0,			0,			0,  0 }
		};

		c = getopt_long(argc, argv, "hvlc:",
			long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			printf("option %s", long_options[option_index].name);
			if (optarg)
				printf(" with arg %s", optarg);
			printf("\n");
			break;
		case 'l':
			command = cmd_list_cargo;
			break;
		case 'h':
			command = cmd_print_help;
			break;
		case 'v':
			command = cmd_print_version;
			break;
		case 'e':
			hide_express = true;
			break;
		case 's':
			hide_short_trains = true;
			break;
		case 'c':
		//	if(strlen(optarg) % 5 != 4)
		//	 throw "invalid cargo string. expected: AAAA,BBBB,...";
			if(!optarg)
			 throw "expecting argument";
			cargo = optarg;
			for(const char* ptr = cargo.c_str(); *ptr; )
			{
				if(!isupper(*ptr)|| !isupper(*++ptr)
					|| !isupper(*++ptr) || !isupper(*++ptr))
					throw "expected uppercase here";
				if(*++ptr==',')
				 ++ptr;
				else if(*ptr)
				 throw "expected comma here";
			}
			break;


/*		case '0':
		case '1':
		case '2':
			if (digit_optind != 0 && digit_optind != this_option_optind)
				printf("digits occur in two different argv-elements.\n");
			digit_optind = this_option_optind;
			printf("option %c\n", c);
			break;

		case 'a':
			printf("option a\n");
			break;

		case 'b':
			printf("option b\n");
			break;

		case 'c':
			printf("option c with value '%s'\n", optarg);
			break;

		case 'd':
			printf("option d with value '%s'\n", optarg);
			break;*/

		case '?':
			std::cerr << "unrecognized option found" << std::endl;
			exit(0);
			break;

		default:
			printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}
}

void options::usage()
{
	std::cerr << "usage:" << std::endl;
	std::cerr << "\t-h, --help\t\tprint help and exit" << std::endl;
	std::cerr << "\t-v, --version\t\tprint version and exit" << std::endl;
	std::cerr << "\t-l, --list-cargo\tprint all cargo types from file and exit" << std::endl;
	std::cerr << "\t-c, --cargo=C1,C2,...\tselect cargo for railnet graph" << std::endl;
	std::cerr << "\t-s, --hide-short-trains\ttrains that don't run the full order list are" << std::endl;
	std::cerr << "\t\t\t\tnot printed as extra railyway lines" << std::endl;
	std::cerr << "\t-e, --hide-express-trains\ttrains that stop less often are" << std::endl;
	std::cerr << "\t\t\t\tnot printed as extra railyway lines" << std::endl;
}

