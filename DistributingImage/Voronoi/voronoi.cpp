#include<iostream>
#include<cstdlib>
#include<cmath>
#include<algorithm>
using namespace std;
int main()
{

	int closest = 0;
	int dist = 0;
	int offset = 1;

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
			closest = 0;
			dist = pow(i-poi[0][0],2)+pow(j-poi[0][1],2);
			for(int k=1; k<10; k++)
			{
				int temp_dist = pow(i-poi[k][0],2)+pow(j-poi[k][1],2);
				if( temp_dist < dist)
				{
					dist = temp_dist;
					closest = k;
				}
			}
			image[i][j] = closest+offset;
		}
	}

	for(int i=0; i<10; i++)
	{
		image[poi[i][0]][poi[i][1]] = 0;
	}

	cout << "P2 200 200 " << 10 + offset<< endl;
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
