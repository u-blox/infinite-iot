from time import sleep
import argparse
import sys
import os
import socket 
import select
import re
import json
from socket import SOL_SOCKET, SO_REUSEADDR

size = 1500

prompt = "tec_eh: "

'''Modem Test Server'''
class Modem_Test_Server():
    port = None
    address = None

    '''Modem_Test_Server: initialization'''
    def __init__(self, address, port):
        self.address = address
        self.port = port
        print prompt + "Starting Modem Test Server"
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind((self.address, self.port))
        '''Blocking sockets'''

    '''Accept connection and respond with ack to every 2nd message that requires an ack'''
    def Start_Server(self):
        count = 0
        print prompt + "Waiting for UDP packets on port " + str(self.port)
        while 1:
            try:                
                print prompt + "Waiting to receive UDP data "
                data, address = self.s.recvfrom(size)
                if data:
                    print prompt + "Received a UDP packet from " + str(address) + ":"
                    print prompt + data
                    j = json.loads(data)
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
            except Exception as ex:
                print(prompt + 'caught exception {}'.format(type(ex).__name__))
                print prompt + "Exception occurred while receiving/transferring data"
                break
                
    def __del__(self):
        print prompt + "UDP_Server __del__ called"
        try:
            self.s.shutdown(socket.SHUT_RDWR)
            print prompt + "UDP_Server shut-down"
            self.s.close()
            print prompt + "UDP_Server close"
        except Exception as ex:
            print('Exception while shutting down UDP_Server:  {} - {}'.format(type(ex).__name__, ex.message))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='A test server for the Energy Harvesting modem unit test.')
    parser.add_argument('address', metavar='A', help='the public IP address of this machine')
    parser.add_argument('port', metavar='P', type=int, help='the port number to listen on')
    args = parser.parse_args()
    server = Modem_Test_Server(args.address, args.port)
    server.Start_Server()    
