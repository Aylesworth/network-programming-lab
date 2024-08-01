CC = g++
OPTIONS1 = -I/usr/include/libxml2
OPTIONS2 = -lcurl -lxml2
SOURCE = resolver.cpp
TARGET = resolver

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(OPTIONS1) -o $(TARGET) $(SOURCE) $(OPTIONS2)

clean:
	rm -f $(TARGET)