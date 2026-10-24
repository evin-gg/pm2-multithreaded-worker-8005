all:
	gcc controller.c network.c -o ./controller -lcrypt
	gcc worker.c network.c -o ./worker -lcrypt

worker: worker.c network.c
	gcc worker.c network.c -o ./builds/worker

controller: controller.c network.c
	gcc controller.c network.c -o ./builds/controller

clean:
	rm -f ./worker ./controller

run1:
	gcc controller.c network.c -o ./controller -lcrypt
	gcc worker.c network.c -o ./worker -lcrypt
	./controller --port 9001 --password ?? --charset-file safe-charset.txt --timeout-ms 2000

run2:
	./worker --host 127.0.0.1 --port 9001 --id 1 --heartbeat-ms 1000 --threads 2
