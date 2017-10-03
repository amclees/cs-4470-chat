CXX = g++
CXXFLAGS = -Wall -g -std=c++11 -pthread -I ./ 

chat : chat.cc chat.h
	${CXX} ${CXXFLAGS} -o chat chat.cc

