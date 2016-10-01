all: project

project: project.cpp
	g++ -g -I ./include project.cpp -o project -lX11 -lm -lXext -Wall -Wextra ./libggfonts.so -lrt -lGL -pthread

clean:
	rm -f project
	rm -f *.o

