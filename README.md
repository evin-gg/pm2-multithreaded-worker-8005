# pm2-multithreaded-worker-8005

## Build

From root folder:

make build

Creates:

controller
worker

## How To Run

### Controller

./controller --port <port> --password '<password>' --charset-file <path> --timeout-ms <ms>

./controller --port 8005 --password 'c?' --charset-file safe-charset.txt --timeout-ms 5000

### Worker

./worker --host <host> --port <port> --id <worker-id> --heartbeat-ms <ms> --threads <count>

./worker --host 127.0.0.1 --port 9000 --id41 --heartbeat-ms 1000 --threads 3
