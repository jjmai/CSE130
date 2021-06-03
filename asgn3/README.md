# --------------------------------------------
# Jordan Mai, 1645861
# CSE130, Spring 2021
# README.md
# ------------------------------------------

# Program

The purpose of this program is to be a proxy for client and serverd. This program is suppose to receive and send from each sides. It is also responsible for caching request and contents from a GET if necessary.

# Function

httpproxy.c - This is the main function that runs the proxy server. It connects and accepts from client and server

# Instruction

To compile, type 'make'

To run the program, type ./httpproxy [any # greater than 1024] [any # greater than 1024] OPTIONAL [-c INT] [-u] [-m INT]

The expected output will no errors in sending and receiving between clients
