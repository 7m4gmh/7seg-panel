CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lportaudio -lpthread

SRCS = src/main.cpp src/common.cpp src/udp.cpp src/audio.cpp src/video.cpp src/led.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = 7seg-server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
	