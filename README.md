**# Assignment 2 - httpserver with multi-threading**

Given a functional http server (assignment 1), implement multi-threading and 
logging. The server will have 4 threads running by default, but it will take 
command line arguments that can specify the number of threads. If the log flag
is specified in the command line arguments, logging is enabled and will keep 
track of all requests and errors. If client requests healthcheck, the number of
errors and requests are sent to the client.

**Objectives**
1. Parse additional command line arguments
2. Implement multithreading and ensure synchronization between threads
3. Implement logging. Files must be logged in Hex and must be formatted
4. Implement health check which is a GET method that sends the number of errors and log entries to the client.

## Building the program
| Command | Description |
| ---      |  ------  |
| make | makes httpserver |
| make all | makes httpserver |
| make clean | Removes httpserver |
|make spotless | 	Removes httpserver and .o files |



### Running the program

Runs httpserver 

`httpserver PORTNUMBER -l log_file -N num_of_worker_Threads`


-l and -N are optional arguements. Portnumber must be specifed
