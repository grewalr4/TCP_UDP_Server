all: Bulls_And_Cows.o project1_server.o project1_client.o
	g++ -g -o project1_server project1_server.o Bulls_And_Cows.o 
	g++ -g -o project1_client project1_client.o Bulls_And_Cows.o

Bulls_And_Cows.o: Bulls_And_Cows.cc Bulls_And_Cows.h
	g++ -Wall -c Bulls_And_Cows.cc

project1_server.o: project1_server.cc project1_server.h packet.h Bulls_And_Cows.h
	g++ -Wall -c project1_server.cc 

project1_client.o: project1_client.cc project1_client.h packet.h Bulls_And_Cows.h 
	g++ -Wall -c project1_client.cc 

clean:
	rm -rf project1_server project1_client *.o