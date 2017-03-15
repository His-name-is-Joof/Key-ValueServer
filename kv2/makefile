CPP_FILES = server.cpp main.cpp
HPP_FILES = server.hpp
OBJ_FILES = $(patsubst %.cpp,o/%.o,$(CPP_FILES))

OUT = server
CPP_FLAGS = -std=c++11 -Wall -Wextra -Werror -pedantic -fPIC -g

all: debug

debug:
	$(CXX) -O0 -c $(CPP_FLAGS) server.cpp -o o/server.o
	$(CXX) -O0 -c $(CPP_FLAGS) main.cpp -o o/main.o
	$(CXX) -o $(OUT) $(OBJ_FILES) -lpthread

release:
	$(CXX) -O3 -DNDEBUG -c $(CPP_FLAGS) server.cpp -o o/server.o
	$(CXX) -O3 -DNDEBUG -c $(CPP_FLAGS) main.cpp -o o/main.o
	$(CXX) -o $(OUT) $(OBJ_FILES) -lpthread

clean:
	rm -f $(OBJ_FILES) $(OUT) test.log server.log
