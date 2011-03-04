import struct
import win32file
import winioctlcon
import win32api
import winerror
import pywintypes

USN_BUFFER_SIZE = 65536
   
def open_volume(drive):
    volh = win32file.CreateFile('\\\\.\\' + drive, win32file.GENERIC_READ,
            win32file.FILE_SHARE_READ | win32file.FILE_SHARE_WRITE, None, 
            win32file.OPEN_EXISTING, win32file.FILE_ATTRIBUTE_NORMAL, None)
    return volh

def create_journal(volh):
    inp = struct.pack('QQ', 0, 0)
    win32file.DeviceIoControl(volh, winioctlcon.FSCTL_CREATE_USN_JOURNAL, inp, None)

def query_journal(volh):
    fmt = 'QQQQQQQ'
    len = struct.calcsize(fmt)
    buf = win32file.DeviceIoControl(volh, winioctlcon.FSCTL_QUERY_USN_JOURNAL, None, len)
    tup = struct.unpack(fmt, buf)
    return tup

def get_volume_info(drive):
    return win32api.GetVolumeInformation('\\\\.\\' + drive + '\\')

def decode_usn_data(buf):
    outfmt = 'LHHQQQQLLLLHH'
    outlen = struct.calcsize(outfmt)
    head_usn = struct.unpack('Q', buf[:8])[0]
    buf = buf[8:]
    tups = []
    while len(buf) > 0:
        tup = struct.unpack(outfmt, buf[:outlen])
        recordlen = tup[0]
        filenamelen = tup[11]
        filenameoffset = tup[12]
        name1 = buf[filenameoffset:filenameoffset+filenamelen]
        name = name1.decode('UTF-16', 'replace')
        tups.append((tup, name))
        buf = buf[recordlen:]
    return head_usn, tups

ALL_INTERESTING_CHANGES = (winioctlcon.USN_REASON_BASIC_INFO_CHANGE | winioctlcon.USN_REASON_CLOSE
        | winioctlcon.USN_REASON_DATA_EXTEND | winioctlcon.USN_REASON_DATA_OVERWRITE | winioctlcon.USN_REASON_DATA_TRUNCATION
        | winioctlcon.USN_REASON_FILE_CREATE | winioctlcon.USN_REASON_FILE_DELETE
        | winioctlcon.USN_REASON_RENAME_NEW_NAME | winioctlcon.USN_REASON_RENAME_OLD_NAME)

def read_journal(volh, journal_id, first_usn):
    reason_mask = ALL_INTERESTING_CHANGES
    inp = struct.pack('QLLQQQ', first_usn, reason_mask, 0, 0, 0, journal_id)
    buf = win32file.DeviceIoControl(volh, winioctlcon.FSCTL_READ_USN_JOURNAL, inp, USN_BUFFER_SIZE)
    return decode_usn_data(buf)

def enum_usn_data(volh, first_frn, low_usn, high_usn):
    inp = struct.pack('QQQ', first_frn, low_usn, high_usn)
    try:
        buf = win32file.DeviceIoControl(volh, winioctlcon.FSCTL_ENUM_USN_DATA, inp, USN_BUFFER_SIZE)
    except pywintypes.error, ex:
        if ex.args[0] == winerror.ERROR_HANDLE_EOF:
            return None, []
        raise
    head_usn = struct.unpack('Q', buf[:8])[0]
    return decode_usn_data(buf)

def generate_journal(volh, journal_id, first_usn):
    while True:
        first_usn, tups = read_journal(volh, journal_id, first_usn)
        if len(tups) == 0:
            break
        for t,n in tups:
            yield t,n

def generate_usns(volh, low_usn, high_usn):
    first_frn = 0
    while True:
        first_frn, tups = enum_usn_data(volh, first_frn, low_usn, high_usn)
        if len(tups) == 0:
            break
        for t,n in tups:
            yield t,n
