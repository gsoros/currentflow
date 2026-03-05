CC = g++
CFLAGS = -std=c++17 -I. -I./mock_arduino -g
SOURCES = ../components/vesc_component/VescUart.cpp \
	../components/vesc_component/helpers.cpp \
	test_parse.cpp
TARGET = test_parse

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
