#include<unistd.h>

int main()
{
	int i;
	for (i = 0; i < 8; i++)
		access("/usr/src/test_kernel", F_OK);

	return 0;
}
