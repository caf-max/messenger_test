LIB=-L/usr/local/lib
INC=-I/usr/local/include

all:
	g++ -std=c++14 main.cpp $(INC) $(LIB) -lboost_system-mt -o file_server

clean:
	rm file_server
