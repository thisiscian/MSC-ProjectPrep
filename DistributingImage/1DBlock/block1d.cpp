#include<iostream>
#include<cstdlib>
#include<cmath>
#include<algorithm>
using namespace std;
int main()
{

	int val = -1;
	srand (time(NULL));
	int image[200][200];
	int poi[10][2];
	for(int i=0; i<10; i++)
	{
		poi[i][0] = rand()%200;
		poi[i][1] = rand()%200;
	}


	for(int i=0; i<200; i++)
	{
			if(i % (200/10) == 0) val++;
		for(int j=0; j<200; j++)
		{
			
			image[i][j] = val;
		}
	}

	cout << "P2 200 200 10" << endl;
	for(int i=0; i<200; i++)
	{
		for(int j=0; j<200; j++)
		{
			cout << image[i][j] << " ";
		}
		cout << endl;
	}
	return 0;
}
