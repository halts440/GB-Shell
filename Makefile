# GBS Shell Makefile

CC = gcc
CFLAGS  = -Wall -g
OBJ = gbsh.o

all: gbsh

gbsh: $(OBJ)
	$(CXX) $(CFLAGS) -o gbsh $(OBJ)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $<
