#Name:Krshant Chettiar
#CruzID:kchettia
#Class: CSE 130
#Assignment: Assignment 2
#------------------------------------------------------------------------------
# Makefile for CSE 130 Programming Assignment 2
#
#       make            	makes httpserver
#       make all        	makes httpserver
#		make clean			Removes httpserver
#		make spotless		Removes httpserver and .o files
#------------------------------------------------------------------------------
COMPILE	= gcc -c -g -pthread -Wall -Wextra -Wpedantic -Wshadow
LINK	= gcc -pthread -o
REMOVE	= rm -f
TARGET = httpserver
BASE_SOURCES   = queue.c
BASE_OBJECTS   = queue.o
HEADERS        = queue.h

all : $(TARGET)

$(TARGET) : server.o $(BASE_OBJECTS)
	$(LINK) $(TARGET) server.o $(BASE_OBJECTS)

server.o: server.c $(HEADERS)
	$(COMPILE) server.c

$(BASE_OBJECTS) : $(BASE_SOURCES) $(HEADERS)
	$(COMPILE) $(BASE_SOURCES)

clean :
	$(REMOVE) *.o

spotless : clean
	$(REMOVE) httpserver
 