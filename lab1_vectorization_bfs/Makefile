CXX = clang++
COMMON = -std=c++17 -Wall -Wextra -Wpedantic
SRC = src/bfs.cpp

all: bfs_O0 bfs_O3

bfs_O0: $(SRC)
	$(CXX) $(COMMON) -O0 $< -o $@

bfs_O3: $(SRC)
	$(CXX) $(COMMON) -O3 -mcpu=native $< -o $@

clean:
	rm -f bfs_O0 bfs_O3
	rm -rf artifacts
