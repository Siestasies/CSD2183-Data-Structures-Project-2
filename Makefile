CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

FILE ?= input.csv
TARGET ?= 99
OUTPUT ?= output.txt
DIR ?= test_cases

simplify: main.cpp
	$(CXX) $(CXXFLAGS) -o simplify main.cpp

run: simplify
	./simplify $(DIR)/$(FILE) $(TARGET) > $(OUTPUT)

benchmark: simplify
	bash benchmark.sh

clean:
	rm -f simplify massif.out massif_report.txt

.PHONY: clean run memcheck benchmark

# can replace "run" with "memcheck"

# for original test cases
# make run DIR=test_cases FILE=input_original_01.csv TARGET=99 OUTPUT=my_output.txt

# for your generated datasets
# make run DIR=generated_polygon_datasets FILE=Vertexes_100__Holes_0.csv TARGET=50 OUTPUT=my_output.txt