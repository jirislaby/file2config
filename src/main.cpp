#include "treewalker/treewalker.h"

using namespace std;

unsigned verbose;

int main(int argc, char **argv)
{
	std::filesystem::path path{"."};
	if (argc > 1)
		path = argv[1];

	TW::TreeWalker tw(path);
	tw.walk();

	return 0;
}
