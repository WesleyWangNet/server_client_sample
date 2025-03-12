TARGET:=server

.PHONY: all clean

CXX_FLAGS:=-I./inc -std=c++11 -g



all: $(TARGET)
$(TARGET): *.cpp
	g++ $(CXX_FLAGS) -o $@ $^

clean:
	rm -f *.gch $(TARGET)

