#!/usr/bin/env python
"""Extract Infinite-IoT device log records from a Mongo database and decode them"""
import argparse
import signal
from datetime import date, datetime, timedelta, tzinfo
from collections import namedtuple
from tempfile import NamedTemporaryFile
from struct import pack, unpack
from subprocess import check_output
from sys import exit, stdout
from pymongo import MongoClient
from bson.objectid import ObjectId
try:
    from datetime import timezone
    utc = timezone.utc
except ImportError:
    #Python2 version
    class UTC(tzinfo):
        """UTC for Python 2"""
        def utcoffset(self, dt):
            del dt
            return timedelta(0)
        def tzname(self, dt):
            del dt
            return "UTC"
        def dst(self, dt):
            del dt
            return timedelta(0)
    utc = UTC()

#  Prompt for informative prints to console
PROMPT = "LogDecode: "
# The number of items in a decoded log entry
# (us timestamp, event number, event string, parameter decimal, parameter hex)
ITEMS_IN_DECODED_LOG = 5
# Struct to carry a log segment with version numbers
LogSegment = namedtuple('LogSegmentStruct', \
                       ['application_version', 'client_version', 'index', 'records'])

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
                 device_name, start_time, end_time, trim):
        if not trim:
            print PROMPT + "Starting Log_Decoder"
        signal.signal(signal.SIGINT, signal_handler)
        self.converter_root = converter_root
        self.name = device_name
        self.mongo = MongoClient()
        self.database = self.mongo[db_name]
        self.collection = self.database[collection_name]
        self.start_time = start_time
        self.start_pre_time = None
        self.end_time = end_time
        self.trim = trim
        self.log_unix_time_base = long(0)
        self.log_timestamp_at_base = long(0)
        self.log_wrap_count = 0
        self.record_last_index = 0
        self.record_skip_count = 0

    def start_decoder(self):
        """Access database, extract records, run DECODER"""
        log_last_index = -1
        log_segment_count = 0
        log_record_count = 0
        record_list_count = 0
        log_record_in_range = False
        out_of_order_record_list = []
        out_of_order_gap = 0

        if not self.trim:
            print PROMPT + "Decoding log messages that arrived at the server from device " + \
                  self.name,
            if self.start_time is not None:
                print("after around UTC " + date.strftime(self.start_time, "%Y-%m-%d %H:%M")),
            if self.end_time is not None:
                print("and before around UTC " + date.strftime(self.end_time, "%Y-%m-%d %H:%M")),
            stdout.write('\n')

        # Set some default start and end times if they are None
        if self.start_time is None:
            self.start_time = datetime.strptime('Jan 2 1970', '%b %d %Y')
        if self.end_time is None:
            self.end_time = datetime.strptime('Jan 1 2038', '%b %d %Y')

        # Set the start pre-time, earlier to make sure we pick up a timestamp
        self.start_pre_time = self.start_time - timedelta(minutes=10)

        # Create some dummy object IDs from the dates/times to use with PyMongo
        object_id_start_pre_time = ObjectId.from_datetime(self.start_pre_time)
        object_id_end_time = ObjectId.from_datetime(self.end_time)

        # Retrieve all the records that match the given name in the order they were created
        record_list = self.collection.find({"n": self.name, \
                                            "_id": {"$gte": object_id_start_pre_time, \
                                                    "$lte": object_id_end_time}}). \
                                      sort([["_id", 1]])
        for record in record_list:
            log_record_in_range = record["_id"].generation_time > self.start_time
            if log_record_in_range:
                record_list_count += 1
            # Find the index in the record
            if "i" in record:
                if record["i"] == 0:
                    if log_record_in_range:
                        print "--- BOOT ---"
                    self.record_last_index = 0
                    self.record_skip_count = 0
                else:
                    # Only proceed if the index has incremented
                    # (otherwise this must be a retransmission)
                    if (record["i"] == 0) or (record["i"] > self.record_last_index):
                        self.record_last_index = record["i"]
                        # Find the report items in the record
                        if "r" in record:
                            r_list = record["r"]
                            # Go through the list
                            for r_item in r_list:
                                # See if there's a log segment in it
                                if "log" in r_item:
                                    log_segment = r_item["log"]
                                    if log_record_in_range:
                                        log_segment_count += 1
                                    log_segment_struct = self.get_log_segment(log_segment)
                                    # If the segment index is 0 or in order, decode it
                                    if log_segment_struct.index == 0:
                                        # If the index has restarted, clear the store
                                        out_of_order_record_list = []
                                    if (log_segment_struct.index == 0) or \
                                       (log_segment_struct.index == log_last_index + 1):
                                        out_of_order_gap = 0
                                        log_last_index = log_segment_struct.index
                                        if log_segment_struct.records is not None:
                                            log_record_count += \
                                                          self. \
                                                          decode_log_segment(log_segment_struct, \
                                                                             log_record_in_range)
                                    else:
                                        # See if we can find the out of order segment
                                        if out_of_order_gap > 30:
                                            if log_record_in_range:
                                                print "--- MISSED some log record(s) ---"
                                            # We've been waiting for an out of order entry for
                                            # too long, just get on with it
                                            out_of_order_gap = 0
                                            log_last_index = log_segment_struct.index
                                            if log_segment_struct.records is not None:
                                                log_record_count += \
                                                            self. \
                                                            decode_log_segment(log_segment_struct, \
                                                                               log_record_in_range)
                                        if log_segment_struct.index > log_last_index + 1:
                                            # Segment is in the future, store it until later
                                            out_of_order_record_list.append(log_segment_struct)
                                        if len(out_of_order_record_list) > 20:
                                            # Make sure the list doesn't get too full
                                            out_of_order_record_list.pop(0)
                                        # Now see if the segment we need is somewhere in
                                        # the out of order list
                                        found_it = False
                                        for out_of_order_record in out_of_order_record_list:
                                            if out_of_order_record.index == log_last_index + 1:
                                                # Found what we need: decode it and remove it
                                                found_it = True
                                                log_last_index = out_of_order_record.index
                                                if out_of_order_record.records is not None:
                                                    log_record_count += \
                                                            self. \
                                                            decode_log_segment(out_of_order_record,\
                                                                               log_record_in_range)
                                                out_of_order_record_list.remove(out_of_order_record)
                                                break
                                        if not found_it:
                                            # If it was not in the out of order store,
                                            # keep a track of how many times we've tried
                                            out_of_order_gap += 1
        if not self.trim:
            print(PROMPT + "%r record(s) returned containing %r log segment(s) and %r log item(s)"
                  % (record_list_count, log_segment_count, log_record_count))

    def get_log_segment(self, log_segment):
        """We have a segment of log, grab it into a LogSegment struct"""
        log_records = None
        log_index = 0
        log_application_version = None
        log_client_version = None

        # Go find the log data
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
            if "i" in log_data:
                log_index = log_data["i"]
            # Extract the log records
            if "rec" in log_data:
                log_records = log_data["rec"]

        return LogSegment(log_application_version,
                          log_client_version,
                          log_index,
                          log_records)

    def decode_log_segment(self, log_segment_struct, print_it):
        """Decode the log records"""
        log_record_count = 0
        log_output = None
        # Write data to file
        log_file = NamedTemporaryFile()
        for log_record in log_segment_struct.records:
            if len(log_record) >= 3:
                # Binary write of 3 4-byte integers
                log_file.write(pack("3I", log_record[0], log_record[1], log_record[2]))
                if print_it:
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
                self.parse_log_segment(log_output, print_it)
            except OSError:
                pass
        if log_output is None:
            # If that didn't work, try just the root decoder
            try:
                log_output = check_output([self.converter_root, log_file.name])
                self.parse_log_segment(log_output, print_it)
            except OSError:
                pass
        if log_output is None and print_it:
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

    def parse_log_segment(self, log_segment_string, print_it):
        """Take a segment's worth of log strings and print them to the console"""
        log_segment_string.split('\n')
        for log_item_string in log_segment_string.split('\n'):
            log_items = log_item_string.split(',')
            # If there is a TIME_SET item in the string then grab the time from it
            if len(log_items) >= 3:
                # Closing quote missed deliberately to catch LOG_START_AGAIN also
                if log_items[2].find('"  LOG_START') >= 0:
                    self.log_unix_time_base = 0
                    self.log_timestamp_at_base = 0
                    self.log_wrap_count = 0
                if log_items[2].find('"  TIME_SET"') >= 0:
                    self.log_unix_time_base = long(log_items[3])
                    self.log_timestamp_at_base = long(log_items[0])
                    self.log_wrap_count = 0
                if log_items[2].find('"  CURRENT_TIME_UTC"') >= 0:
                    self.log_unix_time_base = long(log_items[3])
                    self.log_timestamp_at_base = long(log_items[0])
                    self.log_wrap_count = 0
                if log_items[2].find('"  LOG_TIME_WRAP"') >= 0:
                    self.log_wrap_count += 1
            # If there are enough items to make this a log entry, print it
            if print_it and len(log_items) == ITEMS_IN_DECODED_LOG:
                microsecond_time = long(log_items[0]) - self.log_timestamp_at_base + \
                                   (self.log_unix_time_base * 1000000) + \
                                   (long(0xFFFFFFFF) * self.log_wrap_count)
                seconds = microsecond_time / 1000000
                time_string_main = datetime.utcfromtimestamp(seconds) \
                                           .strftime("%Y-%m-%d_%H:%M:%S")
                print("%s.%03d %s %s %s %s" %
                      (time_string_main, (microsecond_time - (seconds * 1000000)) / 1000,
                       log_items[1], log_items[3], log_items[4], log_items[2][2:-1]))

def get_date(string):
    """Get a date from a string"""
    date_time = None
    try:
        date_time = datetime.strptime(string, '%Y-%m-%d_%H:%M')
        date_time = date_time.replace(tzinfo=utc)
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
    PARSER.add_argument("-t", "--trim", default=False, action="store_true",
                        help="trimmed output: just the data, no fluff")
    ARGS = PARSER.parse_args()
    DECODER = LogDecode(ARGS.converter_root, ARGS.db_name, ARGS.collection_name,
                        ARGS.device_name, ARGS.start_time, ARGS.end_time, ARGS.trim)
    DECODER.start_decoder()
