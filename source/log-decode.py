#!/usr/bin/env python
"""Extract Infinite-IoT device log records from a Mongo database and decode them"""
import argparse
import signal
from datetime import date, datetime
from collections import namedtuple
from tempfile import NamedTemporaryFile
from struct import pack, unpack
from subprocess import check_output
from sys import exit, stdout
from pymongo import MongoClient

#  Prompt for informative prints to console
PROMPT = "LogDecode: "
# The number of items in a decoded log entry
# (us timestamp, event number, event string, parameter decimal, parameter hex)
ITEMS_IN_DECODED_LOG = 5
# Struct to carry a log segment with version numbers
LogSegment = namedtuple('LogSegmentStruct', ['application_version', 'client_version', 'records'])

def signal_handler(sig, frame):
    """CTRL-C Handler"""
    del sig
    del frame
    stdout.write('\n')
    print PROMPT + "Ctrl-C pressed, exiting"
    exit(0)

class LogDecode():
    """LogDecoder"""

    def __init__(self, converter_root, db_name, collection_name,
                 device_name, start_time, end_time):
        signal.signal(signal.SIGINT, signal_handler)
        self.converter_root = converter_root
        self.name = device_name
        print PROMPT + "Starting Log_Decoder"
        self.mongo = MongoClient()
        self.database = self.mongo[db_name]
        self.collection = self.database[collection_name]
        self.start_time = start_time
        self.end_time = end_time
        self.log_unix_time_base = long(0)
        self.log_timestamp_at_base = long(0)
        self.log_wrap_count = 0
        self.record_last_index = 0
        self.record_skip_count = 0

    def start_decoder(self):
        """Access database, extract records, run DECODER"""
        log_segment_count = 0
        log_record_count = 0
        print PROMPT+ "Decoding log messages for device " + self.name,
        if self.start_time is not None:
            print("after UTC " + date.strftime(self.start_time, "%Y-%m-%d %H:%M")),
        if self.end_time is not None:
            print("and before UTC " + date.strftime(self.end_time, "%Y-%m-%d %H:%M")),
        stdout.write('\n')
        # Retrieve all the records that match the given name in the order they were created
        record_list = self.collection.find({'n': self.name}).sort([['_id', 1]])
        for record in record_list:
            # Find the index in the record
            if "i" in record:
                if record["i"] == 0:
                    print "--- BOOT ---"
                    self.record_last_index = 0
                    self.record_skip_count = 0
                else:
                    if record["i"] != self.record_last_index + 1:
                        print "--- SKIPPED %d record(s) ---" % \
                            (record["i"] - self.record_last_index - 1)
                # Only proceed if the index has incremented
                # (otherwise this must be a retransmission)
                if (record["i"] == 0) or (record["i"] != self.record_last_index):
                    self.record_last_index = record["i"]
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
        log_in_time_range = True

        # Check for time/date bounds
        if self.start_time is not None:
            if "t" in log_segment:
                if datetime.utcfromtimestamp(log_segment["t"]) < self.start_time:
                    log_in_time_range = False
                else:
                    if self.end_time is not None and \
                        (datetime.utcfromtimestamp(log_segment["t"]) > self.end_time):
                        log_in_time_range = False
        # Now go find the log data
        if log_in_time_range:
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
        log_output = None
        # Write data to file
        log_file = NamedTemporaryFile()
        for log_record in log_segment_struct.records:
            if len(log_record) >= 3:
                # Binary write of 3 4-byte integers
                log_file.write(pack("3I", log_record[0], log_record[1], log_record[2]))
                log_record_count += 1
        log_file.seek(0)
        # Try to open the decoder
        # The decoder name should be converter_root followed by "_x_y", where
        # x is an integer representing the log_application_version and y
        # is an integer representing the log_client_version
        if log_segment_struct.application_version is not None and \
            log_segment_struct.client_version is not None:
            try:
                log_output = check_output([self.converter_root + "_" +
                                           str(log_segment_struct.application_version) + "_" +
                                           str(log_segment_struct.client_version), log_file.name])
                self.print_log_segment(log_output)
            except OSError:
                pass
        if log_output is None:
            # If that didn't work, try just the root decoder
            try:
                log_output = check_output([self.converter_root, log_file.name])
                self.print_log_segment(log_output)
            except OSError:
                pass
        if log_output is None:
            # If the root decoder can't be found, just output the raw log file,
            # using a format similar to that log-converter would use

            # read 12 bytes until end of the data
            for log_item in iter(lambda: log_file.read(12), ""):
                microsecond_time = unpack("I", log_item[0:4])[0]
                time_string_main = datetime.utcfromtimestamp(microsecond_time / 1000000) \
                                           .strftime("%Y-%m-%d_%H:%M:%S")
                print("%s.%03d %10d %10d 0x%08x" %
                      (time_string_main, microsecond_time % 1000,
                       unpack("I", log_item[4:8])[0],
                       unpack("I", log_item[8:12])[0],
                       unpack("I", log_item[8:12])[0]))
        # Tidy up
        log_file.close()
        return log_record_count

    def print_log_segment(self, log_segment_string):
        """Take a segment's worth of log strings and print them to the console"""
        log_segment_string.split('\n')
        for log_item_string in log_segment_string.split('\n'):
            log_items = log_item_string.split(',')
            # If there is a TIME_SET item in the string then grab the time from it
            if len(log_items) >= 3:
                if log_items[2].find('"  LOG_START"') >= 0:
                    self.log_unix_time_base = 0
                    self.log_timestamp_at_base = 0
                    self.log_wrap_count = 0;
                if log_items[2].find('"  TIME_SET"') >= 0:
                    self.log_unix_time_base = long(log_items[3])
                    self.log_timestamp_at_base = long(log_items[0])
                if log_items[2].find('"  LOG_TIME_WRAP"') >= 0:
                    self.log_wrap_count += 1
            # If there are enough items to make this a log entry, print it
            if len(log_items) == ITEMS_IN_DECODED_LOG:
                microsecond_time = long(log_items[0]) - self.log_timestamp_at_base + \
                                   (self.log_unix_time_base * 1000000) + \
                                   (long(0xFFFFFFFF) * self.log_wrap_count)
                time_string_main = datetime.utcfromtimestamp(microsecond_time / 1000000) \
                                           .strftime("%Y-%m-%d_%H:%M:%S")
                print("%s.%03d %s %s %s %s" %
                      (time_string_main, microsecond_time % 1000,
                       log_items[1], log_items[3], log_items[4], log_items[2][2:-1]))

def get_date(string):
    """Get a date from a string"""
    date_time = None
    try:
        date_time = datetime.strptime(string, '%Y-%m-%d_%H:%M')
    except ValueError:
        msg = "%r is not a valid date string (expected format is YYYY-mm-dd_HH:MM)" % string
        raise argparse.ArgumentTypeError(msg)
    return date_time

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description=
                                     "Decode logs stored in a Mongo database from a \
                                      given Infinite IoT device.")
    PARSER.add_argument("converter_root", metavar='L',
                        help="the path to the root name of the log-converter, \
                              e.g. ./log-converter/log-converter")
    PARSER.add_argument("db_name", metavar='D',
                        help="the name of the Mongo database to read from")
    PARSER.add_argument("collection_name", metavar='C',
                        help="the name of the collection in the Mongo database where the \
                              records from the device are to be found")
    PARSER.add_argument("device_name", metavar='N',
                        help="the name (i.e. IMEI) of the Infinite IoT device")
    PARSER.add_argument("start_time", nargs='?', default=None, type=get_date,
                        help="[optional] the UTC start time that the logs would \
                              have been reported, format YYYY-MM-DD_HH:MM")
    PARSER.add_argument("end_time", nargs='?', default=None, type=get_date,
                        help="[optional] the UTC end time that the logs would have been \
                              reported, format YYYY-MM-DD_HH:MM")
    ARGS = PARSER.parse_args()
    DECODER = LogDecode(ARGS.converter_root, ARGS.db_name, ARGS.collection_name,
                        ARGS.device_name, ARGS.start_time, ARGS.end_time)
    DECODER.start_decoder()
