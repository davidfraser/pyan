import zipfile
import struct
import sys
import os

def write_data(filename, data):
    if filename.endswith('/'):
        os.mkdir(filename)
    else:
        f = open(filename, 'wb')
        f.write(data)
        f.close()

def main(filename):
    print 'Reading %s' % sys.argv[1]
    f = open(filename, 'rb')

    while True:
        # Read and parse a file header
        fheader = f.read(zipfile.sizeFileHeader)
        if len(fheader) < zipfile.sizeFileHeader:
            print 'Found end of file.  Some entries missed.'
            break
        
        fheader = struct.unpack(zipfile.structFileHeader, fheader)
        if fheader[zipfile._FH_SIGNATURE] == 'PK\x01\x02':
            print 'Found start of central directory.  All entries processed.'
            break
        
        fname = f.read(fheader[zipfile._FH_FILENAME_LENGTH])
        if fheader[zipfile._FH_EXTRA_FIELD_LENGTH]:
            f.read(fheader[zipfile._FH_EXTRA_FIELD_LENGTH])
        print 'Found %s' % fname
        
        # Fake a zipinfo record
        zi = zipfile.ZipInfo()
        zi.compress_size = fheader[zipfile._FH_COMPRESSED_SIZE]
        zi.compress_type = fheader[zipfile._FH_COMPRESSION_METHOD]
        zi.flag_bits = fheader[zipfile._FH_GENERAL_PURPOSE_FLAG_BITS]
        zi.file_size = fheader[zipfile._FH_UNCOMPRESSED_SIZE]
        
        # Read the file contents
        zef = zipfile.ZipExtFile(f, 'rb', zi)
        data = zef.read()
        
        # Sanity checks
        if len(data) != fheader[zipfile._FH_UNCOMPRESSED_SIZE]:
            raise Exception("Unzipped data doesn't match expected size! %d != %d, in %s" % (len(data), fheader[zipfile._FH_UNCOMPRESSED_SIZE], fname))
        calc_crc = zipfile.crc32(data) & 0xffffffff
        if calc_crc != fheader[zipfile._FH_CRC]:
            raise Exception('CRC mismatch! %d != %d, in %s' % (calc_crc, fheader[zipfile._FH_CRC], fname))
        
        # Write the file
        write_data(fname, data)

    f.close()

if __name__ == '__main__':
    main(sys.argv[1])
