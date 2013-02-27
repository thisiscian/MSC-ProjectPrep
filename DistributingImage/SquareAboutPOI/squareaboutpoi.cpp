#include<iostream>
#include<cstdlib>
#include<cmath>
#include<algorithm>
using namespace std;
int main()
{

	int max = 1;
	int squaresize = 30;

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
		for(int j=0; j<200; j++)
		{
			image[i][j] = 0;
		}
	}

	for(int i=0; i<10; i++)
	{
		int x_min = poi[i][0] - squaresize;
		if(x_min < 0) x_min = 0;
		int x_max = poi[i][0] + squaresize;
		if(x_max > 199) x_max = 199;
		int y_min = poi[i][1] - squaresize;
		if(y_min < 0) y_min = 0;
		int y_max = poi[i][1] + squaresize;
		if(y_max > 199) y_max = 199;
		
		for(int j=x_min; j<x_max; j++)
		{
			for(int k=y_min; k<y_max; k++)
			{
				image[j][k]++;
				if(image[j][k] > max)
				{
					max = image[j][k];
				}
			}
		}
	}

	cout << "P2 200 200 " << max << endl;
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
