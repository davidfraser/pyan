#include "ai.h"


int main(int argc, char *argv[])
{
	test();

#ifdef WIN32
	getch();
#endif

	return 0;
}
