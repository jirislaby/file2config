#include <getopt.h>
#include <iostream>

#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

static void usage(const char *prgname)
{
	std::cout << prgname << ": [-v] [start_path]\n";
}

int main(int argc, char **argv)
{
	const struct option opts[] = {
		{ "verbose", 0, nullptr, 'v' },
		{}
	};
	int opt;
	while ((opt = getopt_long(argc, argv, "v", opts, nullptr)) >= 0) {
		switch (opt) {
		case 'v':
			F2C::verbose++;
			break;
		default:
			usage(argv[0]);
			return 0;
		}

	}

	std::filesystem::path path{"."};
	if (argc > optind)
		path = argv[optind];

	TW::ConsoleMakeVisitor visitor;
	TW::TreeWalker tw(path, visitor);
	tw.walk();

	return 0;
}
