CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

all: asx

asx: asx.cpp
	$(CXX) $(CXXFLAGS) -o asx asx.cpp

clean:
	rm -f asx *.o *.l *.st
