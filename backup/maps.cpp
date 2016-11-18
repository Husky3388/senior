#include<iostream>
#include"maps.h"
using namespace std;

int area[10][4];

void clearMap(int area[10][4])
{
    for(int i = 0; i < 10; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            area[i][j] = 0;
        }
    }
}

void initMap1()
{
    clearMap(area);
    area[0][3] = 1;
    area[1][1] = 1;
    area[1][0] = 1;
    area[2][2] = 1;
    area[2][3] = 1;
    area[3][1] = 1;

    return;
}
