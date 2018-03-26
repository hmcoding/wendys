#include<unistd.h>

int main()
{
	int i;
	for (i = 0; i < 10; i++)
		access("/home/class/jowens/COP4610/Projects/2", F_OK);
	
	//int p1p2[2];
	//pipe(p1p2);
	//fork();
	return 0;
}
