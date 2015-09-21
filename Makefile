all : main haha

main : main.c coroutine.c
	gcc -lpthread -g -Wall -o $@ $^

haha : haha.c coroutine.c
	gcc -lpthread -g -Wall -o $@ $^

clean :
	rm main
	rm haha

