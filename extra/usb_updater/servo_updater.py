#!/usr/bin/env python
# Copyright 2016 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Note: This is a py2/3 compatible file.

"""USB updater tool for servo and similar boards."""

from __future__ import print_function

import argparse
import json
import os
import re
import subprocess
import sys
import time
from typing import Tuple

from ecusb import tiny_servod
import ecusb.tiny_servo_common as c
import fw_update


class ServoUpdaterException(Exception):
    """Raised on exceptions generated by servo_updater."""


BOARD_C2D2 = "c2d2"
BOARD_SERVO_MICRO = "servo_micro"
BOARD_SERVO_V4 = "servo_v4"
BOARD_SERVO_V4P1 = "servo_v4p1"
BOARD_SWEETBERRY = "sweetberry"

# These lists are to facilitate exposing choices in the command-line tool
# below.
BOARDS = [
    BOARD_C2D2,
    BOARD_SERVO_MICRO,
    BOARD_SERVO_V4,
    BOARD_SERVO_V4P1,
    BOARD_SWEETBERRY,
]

# Servo firmware bundles four channels of firmware. We need to make sure the
# user does not request a non-existing channel, so keep the lists around to
# guard on command-line usage.

DEFAULT_CHANNEL = STABLE_CHANNEL = "stable"

PREV_CHANNEL = "prev"

# The ordering here matters. From left to right it's the channel that the user
# is most likely to be running. This is used to inform and warn the user if
# there are issues. e.g. if the all channels are the same, we want to let the
# user know they are running the 'stable' version before letting them know they
# are running 'dev' or even 'alpah' which (while true) might cause confusion.

CHANNELS = [DEFAULT_CHANNEL, PREV_CHANNEL, "dev", "alpha"]

DEFAULT_BASE_PATH = "/usr/"
TEST_IMAGE_BASE_PATH = "/usr/local/"

COMMON_PATH = "share/servo_updater"

FIRMWARE_DIR = "firmware/"
CONFIGS_DIR = "configs/"

RETRIES_COUNT = 10
RETRIES_DELAY = 1


def do_with_retries(func, *args):
    """Try a function several times

    Call function passed as argument and check if no error happened.
    If exception was raised by function,
    it will be retried up to RETRIES_COUNT times.

    Args:
      func: function that will be called
      args: arguments passed to 'func'

    Returns:
      If call to function was successful, its result will be returned.
      If retries count was exceeded, exception will be raised.
    """

    retry = 0
    while retry < RETRIES_COUNT:
        try:
            return func(*args)
        except Exception as e:
            print("Retrying function %s: %s" % (func.__name__, e))
            retry = retry + 1
            time.sleep(RETRIES_DELAY)
            continue

    raise ServoUpdaterException(
        "'{}' failed after {} retries".format(func.__name__, RETRIES_COUNT)
    )


def flash(brdfile, serialno, binfile):
    """Call fw_update to upload to updater USB endpoint.

    Args:
      brdfile: path to board configuration file
      serialno: device serial number
      binfile: firmware file
    """

    p = fw_update.Supdate()
    p.load_board(brdfile)
    p.connect_usb(serialname=serialno)
    p.load_file(binfile)

    # Start transfer and erase.
    p.start()
    # Upload the bin file
    print("Uploading %s" % binfile)
    p.write_file()

    # Finalize
    print("Done. Finalizing.")
    p.stop()


def flash2(vidpid, serialno, binfile):
    """Call fw update via usb_updater2 commandline.

    Args:
      vidpid: vendor id and product id of device
      serialno: device serial number (optional)
      binfile: firmware file
    """

    tool = "usb_updater2"
    cmd = "%s -d %s" % (tool, vidpid)
    if serialno:
        cmd += " -S %s" % serialno
    cmd += " -n"
    cmd += " %s" % binfile

    print(cmd)
    help_cmd = "%s --help" % tool
    with open("/dev/null", "rb") as devnull:
        valid_check = subprocess.call(
            help_cmd.split(), stdout=devnull, stderr=devnull
        )
    if valid_check:
        raise ServoUpdaterException(
            "%s exit with res = %d. Make sure the tool "
            "is available on the device." % (help_cmd, valid_check)
        )
    res = subprocess.call(cmd.split())

    if res in (0, 1, 2):
        return res
    raise ServoUpdaterException("%s exit with res = %d" % (cmd, res))


def select(tinys, region):
    """Jump to specified boot region

    Ensure the servo is in the expected ro/rw region.
    This function jumps to the required region and verify if jump was
    successful by executing 'sysinfo' command and reading current region.
    If response was not received or region is invalid, exception is raised.

    Args:
      tinys: TinyServod object
      region: region to jump to, only "rw" and "ro" is allowed
    """

    if region not in ["rw", "ro"]:
        raise ServoUpdaterException("Region must be ro or rw")

    if region == "ro":
        cmd = "reboot"
    else:
        cmd = "sysjump %s" % region

    tinys.pty._issue_cmd(cmd)

    tinys.close()
    time.sleep(2)
    tinys.reinitialize()

    res = tinys.pty._issue_cmd_get_results("sysinfo", [r"Copy:[\s]+(RO|RW)"])
    current_region = res[0][1].lower()
    if current_region != region:
        raise ServoUpdaterException(
            "Invalid region: %s/%s" % (current_region, region)
        )


def do_version(tinys):
    """Check version via ec console 'pty'.

    Args:
      tinys: TinyServod object

    Returns:
      detected version number

    Commands are:
    # > version
    # ...
    # Build:   tigertail_v1.1.6749-74d1a312e
    """
    cmd = "version"
    regex = r"Build:\s+(\S+)[\r\n]+"

    results = tinys.pty._issue_cmd_get_results(cmd, [regex])[0]

    return results[1].strip(" \t\r\n\0")


def do_updater_version(tinys):
    """Check whether this uses python updater or c++ updater

    Args:
      tinys: TinyServod object

    Returns:
      updater version number. 2 or 6.
    """
    vers = do_version(tinys)

    # Servo versions below 58 are from servo-9040.B. Versions starting with _v2
    # are newer than anything _v1, no need to check the exact number. Updater
    # version is not directly queryable.
    if re.search(r"_v[2-9]\.\d", vers):
        return 6
    m = re.search(r"_v1\.1\.(\d\d\d\d)", vers)
    if m:
        version_number = int(m.group(1))
        if version_number < 5800:
            return 2
        return 6
    raise ServoUpdaterException(
        "Can't determine updater target from vers: [%s]" % vers
    )


def _extract_version(boardname, binfile):
    """Find the version string from |binfile|.

    Args:
      boardname: the name of the board, eg. "servo_micro"
      binfile: path to the binary to search

    Returns:
      the version string.
    """
    if boardname is None:
        # cannot extract the version if the name is None
        return None
    rawstrings = subprocess.check_output(
        ["cbfstool", binfile, "read", "-r", "RO_FRID", "-f", "/dev/stdout"],
        **c.get_subprocess_args()
    )
    m = re.match(r"%s_v\S+" % boardname, rawstrings)
    if m:
        newvers = m.group(0).strip(" \t\r\n\0")
    else:
        raise ServoUpdaterException(
            "Can't find version from file: %s." % binfile
        )

    return newvers


def get_firmware_channel(bname, version):
    """Find out which channel |version| for |bname| came from.

    Args:
      bname: board name
      version: current version string

    Returns:
      one of the channel names if |version| came from one of those, or None
    """
    for channel in CHANNELS:
        # Pass |bname| as cname to find the board specific file, and pass None as
        # fname to ensure the default directory is searched
        _unused, _unused, vers = get_files_and_version(
            bname, None, channel=channel
        )
        if version == vers:
            return channel
    # None of the channels matched. This firmware is currently unknown.
    return None


def get_updater_path() -> Tuple[str, str, str]:
    """Return paths that servo_updater needs to work.

    Returns:
      path to updater data, path to firmware files, path to config files
    """
    for p in (DEFAULT_BASE_PATH, TEST_IMAGE_BASE_PATH):
        updater_path = os.path.join(p, COMMON_PATH)
        if os.path.exists(updater_path):
            break
    else:
        raise ServoUpdaterException(
            "servo_updater/ dir not found in known spots."
        )

    firmware_path = os.path.join(updater_path, FIRMWARE_DIR)
    configs_path = os.path.join(updater_path, CONFIGS_DIR)

    for p in (firmware_path, configs_path):
        if not os.path.exists(p):
            raise ServoUpdaterException("Could not find required path %r" % p)

    return updater_path, firmware_path, configs_path


def get_files_and_version(cname, fname=None, channel=DEFAULT_CHANNEL):
    """Select config and firmware binary files.

    This checks default file names and paths.
    In: /usr/share/servo_updater/[firmware|configs]
    check for board.json, board.bin

    Args:
      cname: board name, or config name. eg. "servo_v4" or "servo_v4.json"
      fname: firmware binary name. Can be None to try default.
      channel: the channel requested for servo firmware. See |CHANNELS| above.

    Returns:
      cname, fname, version: validated filenames selected from the path.
    """
    _unused_updater_path, firmware_path, configs_path = get_updater_path()

    if not os.path.isfile(cname):
        # If not an existing file, try checking on the default path.
        newname = os.path.join(configs_path, cname)
        if os.path.isfile(newname):
            cname = newname
        else:
            # Try appending ".json" to convert board name to config file.
            cname = newname + ".json"
        if not os.path.isfile(cname):
            raise ServoUpdaterException("Can't find config file: %s." % cname)

    # Always retrieve the boardname
    with open(cname, encoding="utf-8") as data_file:
        data = json.load(data_file)
    boardname = data["board"]

    if not fname:
        # If no |fname| supplied, look for the default locations with the board
        # and channel requested.
        binary_file = "%s.%s.bin" % (boardname, channel)
        newname = os.path.join(firmware_path, binary_file)
        if os.path.isfile(newname):
            fname = newname
        else:
            raise ServoUpdaterException(
                "Can't find firmware binary: %s." % binary_file
            )
    elif not os.path.isfile(fname):
        # If a name is specified but not found, try the default path.
        newname = os.path.join(firmware_path, fname)
        if os.path.isfile(newname):
            fname = newname
        else:
            raise ServoUpdaterException("Can't find file: %s." % fname)

    # Lastly, retrieve the version as well for decision making, debug, and
    # informational purposes.
    binvers = _extract_version(boardname, fname)

    return cname, fname, binvers


def update(dev, serialno, args, devmap):
    """Update |dev|'s firmware

    Args:
        dev: pyUSB object representing the device to update
        serialno: serial number of the device to update
        args: arguments passed in by the user
        devmap: map of devices supported by servo_updater
    """
    vid, pid = dev.idVendor, dev.idProduct
    vidpid = "%04x:%04x" % (vid, pid)
    _unused_board, boardname, iface, brdfile, binfile, newvers = devmap[vidpid]

    # We need a tiny_servod to query some information. Set it up first.
    tinys = tiny_servod.TinyServod(vid, pid, iface, serialno, args.verbose)

    if not args.force:
        vers = do_version(tinys)
        print("Current %s version is   %s" % (boardname, vers))
        print("Available %s version is %s" % (boardname, newvers))

        if newvers == vers:
            print("No version update needed")
            if args.reboot:
                select(tinys, "ro")
            return
        print("Updating to recommended version.")

    # Make sure the servo MCU is in RO
    print("===== Jumping to RO =====")
    do_with_retries(select, tinys, "ro")

    print("===== Flashing RW =====")
    vers = do_with_retries(do_updater_version, tinys)
    # To make sure that the tiny_servod here does not interfere with other
    # processes, close it out.
    tinys.close()

    if vers == 2:
        flash(brdfile, serialno, binfile)
    elif vers == 6:
        do_with_retries(flash2, vidpid, serialno, binfile)
    else:
        raise ServoUpdaterException("Can't detect updater version")

    # Make sure device is up.
    c.wait_for_usb([vidpid], serialname=serialno)
    # After we have made sure that it's back/available, reconnect the tiny servod.
    tinys.reinitialize()

    # Make sure the servo MCU is in RW
    print("===== Jumping to RW =====")
    do_with_retries(select, tinys, "rw")

    print("===== Flashing RO =====")
    vers = do_with_retries(do_updater_version, tinys)

    if vers == 2:
        flash(brdfile, serialno, binfile)
    elif vers == 6:
        do_with_retries(flash2, vidpid, serialno, binfile)
    else:
        raise ServoUpdaterException("Can't detect updater version")

    # Make sure the servo MCU is in RO
    print("===== Rebooting =====")
    do_with_retries(select, tinys, "ro")
    # Perform additional reboot to free USB/UART resources, taken by tiny servod.
    # See https://issuetracker.google.com/196021317 for background.
    tinys.pty._issue_cmd("reboot")


def print_versions(outfile, boards, file, channel):
    """Print live firmware versions for given servo boards.

    This directly calls print(), this does not return any information.

    The output format is mean for human readability and is subject to change
    without notice.  Do NOT parse it from code, use the JSON output option for
    that.

    Args:
        outfile: file-like object compatible with print()
        boards: iterable of str - servo board names from BOARDS
        file: None or str - firmware binary name
        channel: str - update channel from CHANNELS
    """
    for i, board in enumerate(boards):
        brdfile, binfile, newvers = get_files_and_version(board, file, channel)
        if i:
            print(file=outfile)
        print("board:", board, file=outfile)
        print("channel:", channel, file=outfile)
        print("firmware:", newvers, file=outfile)
        print("firmware file:", binfile, file=outfile)


def print_json(outfile, boards, file, channel):
    """Print live firmware versions for given servo boards.

    This directly calls print(), this does not return any information.

    The JSON output /structure/ is stable and meant for machine parsing by a
    JSON parser.  Formatting details that do not change the JSON data meaning
    are subject to change.

    Args:
        outfile: file-like object compatible with print()
        boards: iterable of str - servo board names from BOARDS
        file: None or str - firmware binary name
        channel: str - update channel from CHANNELS
    """
    output = []
    for board in boards:
        brdfile, binfile, newvers = get_files_and_version(board, file, channel)
        output.append(
            {
                "board": board,
                "channel": channel,
                "firmware": newvers,
                "firmware file": binfile,
            }
        )
    # print() gives system-correct trailing newline, unlike json.dump()
    print(json.dumps(output, indent=2, sort_keys=True), file=outfile)


def main():
    parser = argparse.ArgumentParser(
        description="""
        Image a servo device. Normally this supports flashing the firmware
        of exactly one device and will exit with an error if more than
        one device is found on USB that matches the specification.
    """
    )
    parser.add_argument(
        "-p",
        "--print",
        dest="print_only",
        action="store_true",
        default=False,
        help="only print available firmware for board/channel (human friendly, do not parse, format subject to change)",
    )
    parser.add_argument(
        "--json",
        dest="json_only",
        action="store_true",
        default=False,
        help="only emit available firmware for board/channel as JSON (stable data structure when parsed as JSON, raw printed formatting is subject to change)",
    )
    parser.add_argument(
        "-s",
        "--serialno",
        type=str,
        help="serial number to program",
        default=None,
    )
    parser.add_argument(
        "-b",
        "--board",
        action="append",
        help="servo board name (can be specified more than once)",
        default=None,
        choices=BOARDS,
    )
    parser.add_argument(
        "-c",
        "--channel",
        type=str,
        help="Firmware channel to use",
        default=DEFAULT_CHANNEL,
        choices=CHANNELS,
    )
    parser.add_argument(
        "-f", "--file", type=str, help="Complete ec.bin file", default=None
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Update even if version match",
        default=False,
    )
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        help="Allow updating multiple matching devices.",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Chatty output"
    )
    parser.add_argument(
        "-r",
        "--reboot",
        action="store_true",
        help="Always reboot, even after probe.",
    )

    args = parser.parse_args()

    if args.board is None:
        boards = BOARDS
    else:
        boards = args.board

    # If the user only wants channel information, just print and return (exit).
    if args.print_only or args.json_only:
        if args.print_only and args.json_only:
            raise ServoUpdaterException("Can't use both --print and --json.")
        if args.board is None and not args.all:
            raise ServoUpdaterException(
                "Use --all if printing info for all boards is intended, or --board to specify specific servo boards."
            )
        if args.print_only:
            print_versions(sys.stdout, boards, args.file, args.channel)
        elif args.json_only:
            print_json(sys.stdout, boards, args.file, args.channel)
        return

    serialno = args.serialno

    vidpids = set()
    devmap = {}
    for board in boards:
        brdfile, binfile, newvers = get_files_and_version(
            board, args.file, args.channel
        )

        with open(brdfile, encoding="utf-8") as data_file:
            data = json.load(data_file)
        vid, pid = int(data["vid"], 0), int(data["pid"], 0)
        vidpid = "%04x:%04x" % (vid, pid)
        iface = int(data["console"], 0)
        boardname = data["board"]

        vidpids.add(vidpid)
        devmap[vidpid] = [board, boardname, iface, brdfile, binfile, newvers]

    # Make sure device is up.
    print("===== Waiting for USB device =====")
    devs = c.wait_for_usb(vidpids, serialname=serialno, timeout=5.0)
    if len(devs) > 1 and not args.all:
        raise ServoUpdaterException(
            "Found %d matching devices to update. "
            "Use --all if updating multiple devices is intended." % (len(devs),)
        )

    for dev in devs:
        update(dev, serialno, args, devmap)

    print("===== Finished =====")


if __name__ == "__main__":
    main()
