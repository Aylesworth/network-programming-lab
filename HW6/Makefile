CC = gcc
S_TARGET = server
S_SOURCE = server.c
C_TARGET = client
C_SOURCE = client.c
L_FLAGS = -lssl -lcrypto

all: $(S_TARGET) $(C_TARGET)

$(S_TARGET): $(S_SOURCE)
	$(CC) -o $(S_TARGET) $(S_SOURCE) $(L_FLAGS)

$(C_TARGET): $(C_SOURCE)
	$(CC) -o $(C_TARGET) $(C_SOURCE)

clean:
	rm -f $(S_TARGET) $(C_TARGET)