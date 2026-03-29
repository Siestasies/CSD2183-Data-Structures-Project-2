CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

simplify: main.cpp
	$(CXX) $(CXXFLAGS) -o simplify main.cpp

clean:
	rm -f simplify

.PHONY: clean
