#!/usr/bin/env python
'''Wait for UDP packets from an Infinite-IoT device, decode them''' \
'''and, if valid, put the in a Mongo database'''
import argparse
from sys import exit, stdout
import socket
import signal
import json
from datetime import date, datetime
from pymongo import MongoClient

SIZE = 1500
PROMPT = "UDPJSONMongo: "

class MyException(Exception):
    '''Exception'''
    def __init__(self):
        self._message = ""
    def _get_message(self):
        return self._message
    def _set_message(self, message):
        self._message = message
    message = property(_get_message, _set_message)

def signal_handler(signal, frame):
    '''CTRL-C Handler'''
    del signal
    del frame
    stdout.write('\n')
    print PROMPT + "Ctrl-C pressed, exiting"
    exit(0)

class UDPJSONMongo():
    '''UDP-JSON to Mongo DB Server'''
    port = None
    address = None
    j = None

    def __init__(self, address, port, db_name, collection_name):
        '''UDPJSONMongo: initialization'''
        signal.signal(signal.SIGINT, signal_handler)
        self.address = address
        self.port = port
        print PROMPT + "Starting UDP-JSON to Mongo DB SERVER"
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.address, self.port))
        self.mongo = MongoClient()
        self.data_base = self.mongo[db_name]
        self.collection = self.data_base[collection_name]

    def start_server(self):
        '''Accept connection, respond with ack as required and write decoded JSON to Mongo DB'''
        count = 0
        dedup_dict = {}
        print PROMPT + "Waiting for UDP packets on port " + str(self.port)
        while 1:
            try:
                print PROMPT + "Waiting to receive JSON data in a UDP packet"
                data, address = self.sock.recvfrom(SIZE)
                if data:
                    j = None
                    print PROMPT + "Received a UDP packet from " + \
                          str(address) + " @ " + \
                          date.strftime(datetime.utcnow(), \
                                        "%Y/%m/%d %H:%M:%S UTC") + ":"
                    print PROMPT + data
                    try:
                        j = json.loads(data)
                    except ValueError:
                        print PROMPT + "JSON decode failed"
                    if j:
                        # Manage de-duplication
                        duplicate = False
                        if j["n"] is not None and j["i"] is not None:
                            if j["i"] == 0 or dedup_dict.get(j["n"]) is None:
                                dedup_dict[j["n"]] = []
                            else:
                                max_distance = 0
                                for index in dedup_dict[j["n"]]:
                                    distance = j["i"] - index
                                    if distance == 0:
                                        duplicate = True
                                        print PROMPT + j["n"] + ": index " + \
                                              str(j["i"]) + " is a duplicate"
                                        break
                                    if distance > max_distance:
                                        max_distance = distance
                                if not duplicate:
                                    # If the stored history is well
                                    # ahead of this index then we
                                    # may have missed an index
                                    # reset to clear the history
                                    # as a recovery mechanism
                                    if max_distance > 10:
                                        dedup_dict[j["n"]] = []
                                    dedup_dict[j["n"]].append(j["i"])
                            for history in dedup_dict.itervalues():
                                if len(history) > 10:
                                    # Make sure the lists don't get too full
                                    history.pop(0)
                            if not duplicate:
                                # If it's not a duplicate, stick it in the database
                                print PROMPT + "JSON decoded: "
                                print json.dumps(j)
                                print PROMPT + "Inserting into collection"
                                result = self.collection.insert_one(j)
                                print PROMPT + "Object ID: " + str(result.inserted_id)
                                count += 1
                                if j["a"] is not None and j["n"] is not None and \
                                   j["i"] is not None and j["a"] == 1:
                                    print PROMPT + "Ack required for index " + \
                                          str(j["i"]) + ", id \"" + j["n"] + "\""
                                    ack = "{\"n\":\"" + j["n"] + "\",\"i\":" + \
                                          str(j["i"]) + "}"
                                    print PROMPT + "Ack JSON: " + ack
                                    self.sock.sendto(ack, (address[0], address[1]))
                    else:
                        print PROMPT + "The UDP packet was probably not from our " \
                              "Infinite IoT device"
                else:
                    print PROMPT + "Invalid data received "
            except MyException as ex:
                print PROMPT + 'caught exception {}'.format(type(ex).__name__)
                print PROMPT + "Exception occured while receiving/transferring data"
                break

    def __del__(self):
        try:
            if self.sock and socket:
                self.sock.shutdown(socket.SHUT_RDWR)
                print PROMPT + "UDP SERVER shutdown"
                self.sock.close()
                print PROMPT + "UDP SERVER close"
        except MyException as ex:
            print "Exception while shutting down UDP SERVER: " + \
                  str(type(ex).__name__) + " - " + str(ex.message)

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description="A SERVER which " \
                                     "waits for JSON coded into UDP " \
                                     "packets and writes the JSON to " \
                                     "a Mongo database.")
    PARSER.add_argument("address", metavar='A', help="the public IP " \
                        "address of this machine")
    PARSER.add_argument("port", metavar='P', type=int, help="the port " \
                        "number to listen on")
    PARSER.add_argument("db_name", metavar='D', help="the name of the " \
                        "Mongo database to write to")
    PARSER.add_argument("collection_name", metavar='C', help="the name of " \
                         "the collection in the Mongo database to write to")
    ARGS = PARSER.parse_args()
    SERVER = UDPJSONMongo(ARGS.address, ARGS.port, ARGS.db_name, ARGS.collection_name)
    SERVER.start_server()
