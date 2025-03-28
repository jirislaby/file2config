#include <getopt.h>

#include "treewalker/treewalker.h"

using namespace std;

unsigned verbose;

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
			verbose++;
			break;
		default:
			usage(argv[0]);
			return 0;
		}

	}

	std::filesystem::path path{"."};
	if (argc > optind)
		path = argv[optind];

	TW::TreeWalker tw(path);
	tw.walk();

	return 0;
}
