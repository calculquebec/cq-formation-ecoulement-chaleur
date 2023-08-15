EXECUTABLE = ecoulement

CXX = mpic++
DEBUG = -g -Wall
OPT = -O3
CXX_FLAGS = -std=c++11 $(OPT) -lpng

$(EXECUTABLE): main.cpp Makefile
	$(CXX) $(CXX_FLAGS) -o $@ $<

clean:
	rm -f $(EXECUTABLE)
