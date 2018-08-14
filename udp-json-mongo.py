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
from pymongo import MongoClient

size = 1500
prompt = "UDP_JSON_Mongo: "


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

'''UDP-JSON to Mongo DB Server'''
class UDP_JSON_Mongo():
    port = None
    address = None
    j = None

    '''UDP_JSON_Mongo: initialization'''
    def __init__(self, address, port, dbName, collectionName):
        signal.signal(signal.SIGINT, Signal_Handler)
        self.address = address
        self.port = port
        print prompt + "Starting UDP-JSON to Mongo DB server"
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind((self.address, self.port))
        '''Blocking sockets'''
        self.mongo = MongoClient()
        self.db = self.mongo[dbName]
        self.collection = self.db[collectionName]

    '''Accept connection, respond with ack as required and write decoded JSON to Mongo DB'''
    def Start_Server(self):
        count = 0
        print prompt + "Waiting for UDP packets on port " + str(self.port)
        while 1:
            try:                
                print prompt + "Waiting to receive JSON data in a UDP packet"
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
                        print prompt + "Inserting into collection"
                        self.collection.insert_one(j)
                        count += 1;
                        if j["a"] is not None and j["n"] is not None and j["i"] is not None and j["a"] == 1:
                            print prompt + "Ack required for index " + str(j["i"]) + ", id \"" + j["n"] + "\""
                            ack = "{\"n\":\"" + j["n"] + "\",\"i\":" + str(j["i"]) + "}"
                            print prompt + "Ack JSON: " + ack
                            self.s.sendto(ack, (address[0], address[1]))
                    else:
                        print prompt + "The UDP packet was probably not from our Infinite IoT device"
                else:
                    print prompt + "Invalid data received "
            except My_Exception as ex:
                print(prompt + 'caught exception {}'.format(type(ex).__name__))
                print prompt + "Exception occured while receiving/transferring data"
                break

    def __del__(self):
        try:
            if self.s and socket:
                self.s.shutdown(socket.SHUT_RDWR)
                print prompt + "UDP server shutdown"
                self.s.close()
                print prompt + "UDP server close"
        except My_Exception as ex:
            print "Exception while shutting down UDP server: " + str(type(ex).__name__) + " - " + str(ex.message)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = "A server which waits for JSON coded into UDP packets and writes the JSON to a Mongo database.")
    parser.add_argument("address", metavar='A', help = "the public IP address of this machine")
    parser.add_argument("port", metavar='P', type = int, help="the port number to listen on")
    parser.add_argument("dbName", metavar='D', help="the name of the Mongo database to write to")
    parser.add_argument("collectionName", metavar='C', help="the name of the collection in the Mongo database to write to")
    args = parser.parse_args()
    server = UDP_JSON_Mongo(args.address, args.port, args.dbName, args.collectionName)
    server.Start_Server()    
