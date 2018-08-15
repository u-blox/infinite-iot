from time import sleep
import argparse
from datetime import datetime
from datetime import date
import sys
import os
import re
import signal
import json
from socket import SOL_SOCKET, SO_REUSEADDR
from pymongo import MongoClient

size = 1500
prompt = "Log_Decode: "


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

'''Log_Decoder'''
class Log_Decode():
    j = None

    '''Log_Decode: initialization'''
    def __init__(self, dbName, collectionName, deviceName, startTime, endTime):
        signal.signal(signal.SIGINT, Signal_Handler)
        self.name = deviceName
        print prompt + "Starting Log_Decoder"
        self.mongo = MongoClient()
        self.db = self.mongo[dbName]
        self.collection = self.db[collectionName]
        self.startTime = startTime
        self.endTime = endTime

    '''Access database, extract records, run decoder'''
    def Start_Decoder(self):
        logSegmentCount = 0
        logItemCount = 0
        print prompt + "Decoding log messages for device " + self.name,
        if self.startTime is not None:
            print "after " + date.strftime(self.startTime, "%Y/%m/%d %H:%M"),
        if self.endTime is not None:
            print "and before " + date.strftime(self.endTime, "%Y/%m/%d %H:%M"),
        print
        # Retrieve all the records that match the given name
        recordList = self.collection.find({'n': self.name})
        for record in recordList:
            # Find the report list in the record
            for rEntry in record["r"]:
                for key, logSegment in rEntry.iteritems():
                    if "log" in key:
                        logSegmentCount += 1
                        for key, logData in logSegment.iteritems():
                            if "d" in key:
                                for key, logList in logData.iteritems():
                                    if "rec" in key:
                                        for logItem in logList:
                                            print logItem
                                            logItemCount += 1
        print prompt + "%r record(s) returned containing %r log segment(s) and %r log item(s)" % (recordList.count(), logSegmentCount, logItemCount)

def getDate(string):
    dateTime = None
    try:
        dateTime = datetime.strptime(string, '%Y/%m/%d-%H:%M')
    except ValueError:
        msg = "%r is not a valid date string (expected format is YYYY/mm/dd-HH:MM)" % string
        raise argparse.ArgumentTypeError(msg)
    return dateTime

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = "Decode logs stored in a Mongo database from a given Infinite IoT device.")
    parser.add_argument("dbName", metavar='D', help="the name of the Mongo database to read from")
    parser.add_argument("collectionName", metavar='C', help="the name of the collection in the Mongo database where the records from the device are to be found")
    parser.add_argument("deviceName", metavar='N', help="the name (i.e. IMEI) of the Infinite IoT device")
    parser.add_argument("startTime", nargs='?', default=None, type=getDate, help="[optional] the start time of the log range, format YYYY/mm/dd-HH:MM")
    parser.add_argument("endTime", nargs='?', default=None, type=getDate, help="[optional] the end time of the log range, format YYYY/mm/dd-HH:MM")
    args = parser.parse_args()
    decoder = Log_Decode(args.dbName, args.collectionName, args.deviceName, args.startTime, args.endTime)
    decoder.Start_Decoder()    
