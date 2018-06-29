from time import sleep
import argparse
import sys
import os
import socket 
import select
import re
import signal
import json
from socket import SOL_SOCKET, SO_REUSEADDR

size = 1500

prompt = "infinite_iot: "

'''Exception'''
class My_Exception(Exception):
    def _get_message(self): 
        return self._message
    def _set_message(self, message): 
        self._message = message
    message = property(_get_message, _set_message)

'''CTRL-C Handler'''
def Signal_Handler(signal, frame):
    print
    print prompt + "Ctrl-C pressed, exiting"
    sys.exit(0)

'''Modem Test Server'''
class Modem_Test_Server():
    port = None
    address = None
    j = None

    '''Modem_Test_Server: initialization'''
    def __init__(self, address, port):
        signal.signal(signal.SIGINT, Signal_Handler)
        self.address = address
        self.port = port
        print prompt + "Starting Modem Test Server"
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind((self.address, self.port))
        '''Blocking sockets'''

    '''Accept connection and respond with ack every 2nd message that requires an ack'''
    def Start_Server(self):
        count = 0
        print prompt + "Waiting for UDP packets on port " + str(self.port)
        while 1:
            try:                
                print prompt + "Waiting to receive UDP data "
                data, address = self.s.recvfrom(size)
                if data:
                    j = None
                    print prompt + "Received a UDP packet from " + str(address) + ":"
                    print prompt + data
                    try:
                       j = json.loads(data)
                    except ValueError:
                        print prompt + "JSON decode failed"
                    if j:
                        print prompt + "JSON decoded: "
                        print json.dumps(j)
                        if j["a"] is not None and j["n"] is not None and j["i"] is not None:
                            if  j["a"] == 1:
                                count += 1
                                if count % 2 == 0:
                                    print prompt + "Ack required but not sending it this time to be difficult"
                                else:
                                    print prompt + "Ack required for index " + str(j["i"]) + ", id \"" + j["n"] + "\""
                                    ack = "{\"n\":\"" + j["n"] + "\",\"i\":" + str(j["i"]) + "}"
                                    print prompt + "Ack JSON: " + ack
                                    self.s.sendto(ack, (address[0], address[1]))
                    else:
                        print prompt + "UDP packet was not from our Energy Harvesting device"
                else:
                    print prompt + "Invalid data received "
            except My_Exception as ex:
                print(prompt + 'caught exception {}'.format(type(ex).__name__))
                print prompt + "Exception occured while receiving/transfering data"
                break
                
    def __del__(self):
        try:
            if self.s and socket:
                self.s.shutdown(socket.SHUT_RDWR)
                print prompt + "UDP_Server shutdown"
                self.s.close()
                print prompt + "UDP_Server close"
        except My_Exception as ex:
            print "Exception while shutting down UDP_Server: " + str(type(ex).__name__) + " - " + str(ex.message)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = "A test server for the Energy Harvesting modem unit test.")
    parser.add_argument("address", metavar='A', help = "the public IP address of this machine")
    parser.add_argument("port", metavar='P', type = int, help="the port number to listen on")
    args = parser.parse_args()
    server = Modem_Test_Server(args.address, args.port)
    server.Start_Server()    
