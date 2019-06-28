CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -pedantic-errors `sdl2-config --cflags` -fsanitize=address
LDFLAGS= `sdl2-config --libs` -lGL `pkg-config --cflags --libs libsystemd` -lsqlite3 -Wl,-rpath,. -L. -ldb_api -fsanitize=address
SOURCES=demo.cpp imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_impl_opengl2.cpp imgui_impl_sdl.cpp imgui_widgets.cpp
OBJECTS=$(SOURCES:.cpp=.cpp.o)
EXECUTABLE=demo

all: $(EXECUTABLE) daemon libdb_api.so

$(EXECUTABLE): $(OBJECTS) libdb_api.so
	$(CXX) $(OBJECTS) $(LDFLAGS)  -o $@

%.cpp.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

daemon: 
	$(CC) daemon.c $(CFLAGS) $(LDFLAGS) $< -o $@ 

libdb_api.so: db_api.c
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@

%.c.o: %.c
	$(CC) $(CFLAGS) $< -o $@