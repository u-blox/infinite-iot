#!/usr/bin/env python
"""Extract Infinite-IoT device log records from a Mongo database and decode them"""
import argparse
import collections
import tempfile
from datetime import datetime
from datetime import date
import sys
import signal
from pymongo import MongoClient

PROMPT = "LogDecode: "
# Struct to carry a log segment with version numbers
LogSegment = collections.namedtuple('LogSegmentStruct', ['application_version', 'client_version', 'records'])

def signal_handler(sig, frame):
    """CTRL-C Handler"""
    del sig
    del frame
    print
    print(PROMPT + "Ctrl-C pressed, exiting")
    sys.exit(0)

class LogDecode():
    """LogDecoder"""

    def __init__(self, db_name, collection_name, device_name, start_time, end_time):
        signal.signal(signal.SIGINT, signal_handler)
        self.name = device_name
        print(PROMPT + "Starting Log_Decoder")
        self.mongo = MongoClient()
        self.database = self.mongo[db_name]
        self.collection = self.database[collection_name]
        self.start_time = start_time
        self.end_time = end_time

    def start_decoder(self):
        """Access database, extract records, run DECODER"""
        log_segment_count = 0
        log_record_count = 0
        print(PROMPT+ "Decoding log messages for device " + self.name),
        if self.start_time is not None:
            print("after " + date.strftime(self.start_time, "%Y/%m/%d %H:%M")),
        if self.end_time is not None:
            print("and before" + date.strftime(self.end_time, "%Y/%m/%d %H:%M")),
        print
        # Retrieve all the records that match the given name
        record_list = self.collection.find({'n': self.name})
        for record in record_list:
            # Find the report items in the record
            if "r" in record:
                r_list = record["r"]
                # Go through the list
                for r_item in r_list:
                    # See if there's a log segment in it
                    if "log" in r_item:
                        log_segment = r_item["log"]
                        log_segment_count += 1
                        log_segment_struct = self.get_log_segment(log_segment)
                        if log_segment_struct.records is not None:
                            log_record_count += self.decode_log_segment(log_segment_struct)
        print(PROMPT + "%r record(s) returned containing %r log segment(s) and %r log item(s)"
              % (record_list.count(), log_segment_count, log_record_count))

    def get_log_segment(self, log_segment):
        """We have a segment of log, grab it into a LogSegment struct"""
        log_records = None
        log_application_version = None
        log_client_version = None
        if "d" in log_segment:
            log_data = log_segment["d"]
            # Extract the log version
            if "v" in log_data:
                log_version = log_data["v"]
                if isinstance(log_version, basestring):
                    log_version_parts = log_version.split(".")
                    if (log_version_parts is not None and
                            len(log_version_parts) >= 2):
                        if (log_version_parts[0].isdigit() and
                                log_version_parts[1].isdigit()):
                            log_application_version = int(log_version_parts[0])
                            log_client_version = int(log_version_parts[1])
            # Extract the log records
            if "rec" in log_data:
                log_records = log_data["rec"]

        return LogSegment(log_application_version,
                          log_client_version,
                          log_records)

    def decode_log_segment(self, log_segment_struct):
        """Decode the log records"""
        log_record_count = 0
        # Write data to file
        log_file = tempfile.TemporaryFile()
        for log_record in log_segment_struct.records:
            if len(log_record) >= 3:
                #log_file.write(bytearray(log_record[0]))
                #log_file.write(bytearray(log_record[1]))
                #log_file.write(bytearray(log_record[2]))
                log_record_count += 1
        # Try to open the decoder
        
        # If the decoder can't be found, just output the raw log file
        #log_file.seek(0)
        #for item in iter(lambda: log_file.read(4),""): # read 4 bytes until end of the data
        #    print(str(item))
        # Tidy up
        log_file.close()
        return log_record_count

def get_date(string):
    """Get a date from a string"""
    date_time = None
    try:
        date_time = datetime.strptime(string, '%Y/%m/%d-%H:%M')
    except ValueError:
        msg = "%r is not a valid date string (expected format is YYYY/mm/dd-HH:MM)" % string
        raise argparse.ArgumentTypeError(msg)
    return date_time

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description=
                                     "Decode logs stored in a Mongo database from a \
                                      given Infinite IoT device.")
    PARSER.add_argument("db_name", metavar='D',
                        help="the name of the Mongo database to read from")
    PARSER.add_argument("collection_name", metavar='C',
                        help="the name of the collection in the Mongo database where the \
                              records from the device are to be found")
    PARSER.add_argument("device_name", metavar='N',
                        help="the name (i.e. IMEI) of the Infinite IoT device")
    PARSER.add_argument("start_time", nargs='?', default=None, type=get_date,
                        help="[optional] the start time of the log range, format YYYY/mm/dd-HH:MM")
    PARSER.add_argument("end_time", nargs='?', default=None, type=get_date,
                        help="[optional] the end time of the log range, format YYYY/mm/dd-HH:MM")
    ARGS = PARSER.parse_args()
    DECODER = LogDecode(ARGS.db_name, ARGS.collection_name, ARGS.device_name,
                        ARGS.start_time, ARGS.end_time)
    DECODER.start_decoder()
