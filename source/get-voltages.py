#!/usr/bin/env python
"""Extract Voltage readings for an Infinite-IoT device"""
import argparse
import signal
from datetime import date, datetime, timedelta, tzinfo
from collections import namedtuple
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
PROMPT = "GetVoltages: "
# Struct to carry a voltages item
VoltagesStruct = namedtuple('VoltagesStruct', \
                            ['timestamp', 'v_bat_ok_mv', 'v_in_mv', 'v_primary_mv'])

def signal_handler(sig, frame):
    """CTRL-C Handler"""
    del sig
    del frame
    stdout.write('\n')
    print PROMPT + "Ctrl-C pressed, exiting"
    exit(0)

class GetVoltages():
    """GetVoltages"""

    def __init__(self, db_name, collection_name,
                 device_name, start_time, end_time, trim):
        if not trim:
            print PROMPT + "Starting GetVoltages"
        signal.signal(signal.SIGINT, signal_handler)
        self.name = device_name
        self.mongo = MongoClient()
        self.database = self.mongo[db_name]
        self.collection = self.database[collection_name]
        self.start_time = start_time
        self.end_time = end_time
        self.trim = trim
        self.record_last_index = 0
        self.record_skip_count = 0

    def start(self):
        """Access database, extract voltage data items, print them"""
        record_list_count = 0
        item_count = 0
        output_list = []
        record_in_range = False

        if not self.trim:
            print PROMPT + "Getting Voltage readings that arrived at the server from device " + \
                  self.name,
            if self.start_time is not None:
                print("after around UTC " + date.strftime(self.start_time, "%Y-%m-%d %H:%M")),
            if self.end_time is not None:
                print("and before around UTC " + date.strftime(self.end_time, "%Y-%m-%d %H:%M")),
            stdout.write('\n')
            print PROMPT + "V_BAT_OK, V_IN, V_PRIMARY (all in milliVolts)"

        # Set some default start and end times if they are None
        if self.start_time is None:
            self.start_time = datetime.strptime('Jan 1 1970', '%b %d %Y').replace(tzinfo=utc)
        if self.end_time is None:
            self.end_time = datetime.strptime('Jan 1 2038', '%b %d %Y').replace(tzinfo=utc)

        # Create some dummy object IDs from the dates/times to use with PyMongo
        object_id_start_time = ObjectId.from_datetime(self.start_time)
        object_id_end_time = ObjectId.from_datetime(self.end_time)

        # Retrieve all the records that match the given name in the order they were created
        record_list = self.collection.find({"n": self.name, \
                                            "_id": {"$gte": object_id_start_time, \
                                                    "$lte": object_id_end_time}}). \
                                      sort([["_id", 1]])
        for record in record_list:
            record_in_range = record["_id"].generation_time > self.start_time
            if record_in_range:
                record_list_count += 1
                # Find the index in the record, just to be sure it's a record
                if "i" in record:
                    # Find the report items in the record
                    if "r" in record:
                        r_list = record["r"]
                        # Go through the list
                        for r_item in r_list:
                            # See if there's a voltages item in it
                            if "vlt" in r_item:
                                voltages_item = r_item["vlt"]
                                item_count += 1
                                # Decode the voltages and add them to our list
                                voltages_struct = self.get_voltages_item(voltages_item)
                                output_list.append(voltages_struct)
        # Having got everything, sort it in time order
        output_list.sort(key=sort_func)
        # Now print it
        for item in output_list:
            time_string = datetime.utcfromtimestamp(item.timestamp) \
                                  .strftime("%Y-%m-%d_%H:%M:%S")
            print("%s %d %d %d" % (time_string, item.v_bat_ok_mv,
                                   item.v_in_mv, item.v_primary_mv))
        if not self.trim:
            print(PROMPT + "%r record(s) returned containing %r voltage reading(s)"
                  % (record_list_count, item_count))

    def get_voltages_item(self, voltages_item):
        """We have a voltages item, fill in a VoltagesStruct"""
        timestamp = 0
        v_bat_ok_mv = 0
        v_in_mv = 0
        v_primary_mv = 0

        # Go find the timestamp and the data
        if "t" in voltages_item:
            timestamp = voltages_item["t"]
            if "d" in voltages_item:
                voltages_data = voltages_item["d"]
                # Extract items
                if "vbx1000" in voltages_data:
                    v_bat_ok_mv = voltages_data["vbx1000"]
                if "vix1000" in voltages_data:
                    v_in_mv = voltages_data["vix1000"]
                if "vpx1000" in voltages_data:
                    v_primary_mv = voltages_data["vpx1000"]

        return VoltagesStruct(timestamp,
                              v_bat_ok_mv,
                              v_in_mv,
                              v_primary_mv)

def sort_func(structure):
    """Sort key is timestamp"""
    return structure.timestamp

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
                                     "Get Voltage readings for an Infinite-IoT device.")
    PARSER.add_argument("db_name", metavar='D',
                        help="the name of the Mongo database to read from")
    PARSER.add_argument("collection_name", metavar='C',
                        help="the name of the collection in the Mongo database where the \
                              records from the device are to be found")
    PARSER.add_argument("device_name", metavar='N',
                        help="the name (i.e. IMEI) of the Infinite IoT device")
    PARSER.add_argument("start_time", nargs='?', default=None, type=get_date,
                        help="[optional] the UTC start time that the records would \
                              have been reported, format YYYY-MM-DD_HH:MM")
    PARSER.add_argument("end_time", nargs='?', default=None, type=get_date,
                        help="[optional] the UTC end time that the records would have been \
                              reported, format YYYY-MM-DD_HH:MM")
    PARSER.add_argument("-t", "--trim", default=False, action="store_true",
                        help="trimmed output: just the data, no fluff")
    ARGS = PARSER.parse_args()
    VOLTAGES = GetVoltages(ARGS.db_name, ARGS.collection_name, ARGS.device_name,
                           ARGS.start_time, ARGS.end_time, ARGS.trim)
    VOLTAGES.start()
