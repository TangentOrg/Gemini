#define bkFTL001 5001 /* Attempt to read block %l which does not exist. */
#define bkFTL003 5002 /* No blocks on free chain number %i. */
#define bkFTL004 5003 /* Database block %D has incorrect recid: %D. */
#define bkFTL005 5004 /* bkfradd: block is already on chain %t. */
#define bkFTL006 5005 /* Your database version doesn't match the Gemini ver */
#define bkFTL008 5006 /* error reading file %s, ret = %d" */
#define bkFTL009 5007 /* The %s.lk file is invalid. */
#define bkFTL016 5008 /* bkread: missing bkflsx call */
#define bkFTL017 5009 /* Possible file truncation, %l too big for database. */
#define bkFTL018 5010 /* Invalid use of the Gemini demo database. */
#define bkFTL019 5011 /* error writing, file = %s, ret = %d */
#define bkFTL020 5012 /* Unable to write extent header, file = %s, ret = %d */
#define bkFTL021 5013 /* buffer stack underflow dbkey %D. */
#define bkFTL022 5014 /* bkwrite: bktbl dbk %D not equal to bkbuf dbk %D. */
#define bkFTL023 5015 /* Unable to read extent header, file = %s, ret = %d. */
#define bkFTL024 5016 /* bkset: Extent %s is below size %D. */
#define bkFTL025 5017 /* ** bkgetb: out of storage. */
#define bkFTL027 5018 /* bkpopbs: buffer for dbkey %D not found. */
#define bkFTL028 5019 /* bkaddr called with negative blkaddr: %l */
#define bkFTL029 5020 /* bkwrite for .bi block with negative addr: %l */
#define bkFTL030 5021 /* Block %D use count underflow %i. */
#define bkFTL032 5022 /* bkOpenFilesBuffered: Invalid file table */
#define bkFTL033 5023 /* Not enough database buffers (gemini_buffer_cache). */
#define bkFTL035 5024 /* bkVerifyExtentHeader: Invalid file table */
#define bkFTL039 5025 /* Length requested for a backup note is too large, r */
#define bkFTL041 5026 /* Error getting first extent for area %d, error = %d */
#define bkFTL042 5027 /* Can't allocate storage for area descriptor. */
#define bkFTL043 5028 /* bkburls: invalid block state %i. */
#define bkFTL044 5029 /* bkrlsbuf: cannot release buffer lock, use count %l */
#define bkFTL045 5030 /* Could not determine size of file %s. */
#define bkFTL046 5031 /* bkfilc during resyncing */
#define bkFTL047 5032 /* bkfndblk: dbkey %D not locked. */
#define bkFTL048 5033 /* bkupgrade: dbkey %D not intent locked. */
#define bkFTL052 5034 /* wrong RL blk, read %D from %D */
#define bkFTL053 5035 /* wrong RL blk, write %D into %D */
#define bkFTL054 5036 /* block %D not in hash chain. */
#define bkFTL055 5037 /* bksteal: Attempt to read block %D above high water */
#define bkFTL056 5038 /* wrong dbkey in block. Found %D, should be %D */
#define bkFTL057 5039 /* Extent create failed reading control area extent h */
#define bkFTL058 5040 /* Error allocating block buffer memory. */
#define bkFTL059 5041 /* bkloc: invalid state %d %d. */
#define bkFTL060 5042 /* Couldn't read extent header for file %s. */
#define bkFTL061 5043 /* bkmod: block %D without EXCLUSIVE lock. */
#define bkFTL063 5044 /* Invalid file length for %l for file %s. */
#define bkFTL065 5045 /*  invalid file type %i. */
#define bkFTL066 5046 /* ai header corrupt, blk %X, reason %i, addr %X. */
#define bkFTL067 5047 /* bkwrite: write to disk failed errno %i. */
#define bkFTL073 5048 /* extend buffer %l smaller than blocksize %l. */
#define bkFTL075 5049 /* File %s too small %l, blocksize %l extend failed." */
#define bkFTL076 5050 /* Couldn't write extent header for file %s. */
#define bkFTL077 5051 /* Error writing area block for file %s. */
#define bkFTL078 5052 /* Error writing object block for file %s. */
#define bkFTL079 5053 /* Error writing ai header block for file %s. */
#define bkFTL080 5054 /* Unable to read extent header for file %s. */
#define bkFTL081 5055 /* File %s exists but does not have expected time sta */
#define bkFTL082 5056 /* Can't allocate storage to add extent. */
#define bkFTL083 5057 /* Can't allocate space for new extent structures. */
#define bkFTL084 5058 /* Unable to extend database within area %s. */
#define bkFTL085 5059 /* Unable to truncate extent header, file = %s, ret = */
#define bkMSG002 5060 /* ** Cannot find or open file %s, errno = %i. */
#define bkMSG003 5061 /* ** Database has the wrong version number. (db: %d, */
#define bkMSG005 5062 /* ** The database %s is in use in single-user mode. */
#define bkMSG006 5063 /* ** The database %s is in use in multi-user mode. */
#define bkMSG007 5064 /* The file %s exists.  This indicates one of: */
#define bkMSG008 5065 /*   - The database is in use by another user. */
#define bkMSG009 5066 /*   - The last session terminated abnormally. */
#define bkMSG010 5067 /* If you are CERTAIN the database is not in use by a */
#define bkMSG011 5068 /* simply erase %s and try again. */
#define bkMSG013 5069 /* Database is damaged, see documentation. */
#define bkMSG014 5070 /* %s is a copy of %s. Database cannot be opened. */
#define bkMSG017 5071 /* Probable backup/restore error. */
#define bkMSG019 5072 /* ** The database has the wrong version for database */
#define bkMSG029 5073 /* This is the last time you can use this database. */
#define bkMSG030 5074 /* Use the dictionary to dump any data you want to sa */
#define bkMSG031 5075 /* You may use Gemini on this database %d more time(s */
#define bkMSG032 5076 /* Unable to read master block, file = %s, errno = %d */
#define bkMSG034 5077 /* File %s is on a remote system */
#define bkMSG038 5078 /* Database integrity CANNOT be guaranteed for this s */
#define bkMSG039 5079 /* bkxtn: write error, file %s errno: %i. */
#define bkMSG040 5080 /* Trimed file %s for blocksize %l bytes old:%l new:% */
#define bkMSG041 5081 /* Invalid blocksize %d in database %s. */
#define bkMSG042 5082 /* ** file %s is not compatible with blocksize %d byt */
#define bkMSG043 5083 /* bkioWrite:Bad address passed address = %X */
#define bkMSG044 5084 /* bkioWrite: lseek error %d on file %n at %l, file % */
#define bkMSG045 5085 /* bkioRead: lseek error %d on file %n at %l. */
#define bkMSG049 5086 /* %s: Bad file descriptor was used during %s, fd  %d */
#define bkMSG050 5087 /* %s: Invalid argument %s, fd %d, len %l, offset %l, */
#define bkMSG051 5088 /* %s:Disk quota exceeded during %s, fd %d, len %l, o */
#define bkMSG052 5089 /* %s:Maximum file size exceeded during %s, fd %d, le */
#define bkMSG053 5090 /* %s:Insufficient disk space during %s, fd %d, len % */
#define bkMSG054 5091 /* %s:Unknown O/S error during %s, errno %d, fd %d, l */
#define bkMSG056 5092 /* Unable to read extent header, ret = %d file = %s. */
#define bkMSG058 5093 /* Your database version doesn't match the Gemini ver */
#define bkMSG059 5094 /* You have exceeded the maximum allowed demo databas */
#define bkMSG060 5095 /* bkioWrite: I/O table not initialized, index %d. */
#define bkMSG061 5096 /* bkioWrite: Unknown I/O mode specified %d, file %s. */
#define bkMSG062 5097 /* bkioRead: I/O table not initialized index %d. */
#define bkMSG063 5098 /* bkioRead: Unknown I/O mode specified %d, file %s. */
#define bkMSG064 5099 /* I/O table not initialized. */
#define bkMSG065 5100 /* Invalid I/O mode specified requested %d. */
#define bkMSG066 5101 /* Unable to write extent header, ret = %d file = %s. */
#define bkMSG068 5102 /* loadFileType: stget allocation failure */
#define bkMSG069 5103 /* bkLoadFileTable: utmalloc allocation failure */
#define bkMSG071 5104 /* Unable to read extent header errno = %d file = %s. */
#define bkMSG073 5105 /* read wrong dbkey at offset %l in file %s %r  found */
#define bkMSG074 5106 /* Invalid block %l for file %s, max is %l */
#define bkMSG093 5107 /* stat() failed on both %s and %s, errno = %d. */
#define bkMSG095 5108 /* Invalid FD on seek, ret = %d file = %s. */
#define bkMSG096 5109 /* Unable to read file = %s, errno = %d. */
#define bkMSG099 5110 /* Error writing file %s, errno = %d. */
#define bkMSG100 5111 /* Error opening file %s, errno = %d. */
#define bkMSG101 5112 /* Error reading %s out of %s, errno = %d. */
#define bkMSG102 5113 /* %s: HOSTNAME is %s, expected %s. */
#define bkMSG103 5114 /* %s: PID is %l, expected %l. */
#define bkMSG104 5115 /* Broker disappeared, updating %s.lk file. */
#define bkMSG105 5116 /* %s is missing, shutting down... */
#define bkMSG106 5117 /* %s is not a valid .lk file for this server. */
#define bkMSG107 5118 /* Corrupt block detected when reading from database. */
#define bkMSG108 5119 /* Corrupt block detected when attempting to write a  */
#define bkMSG109 5120 /* Corrupt block detected when attempting to modify a */
#define bkMSG110 5121 /* Corrupt block detected when attempting to release  */
#define bkMSG115 5122 /* Unable to get file descriptor for %s */
#define bkMSG116 5123 /* Unable to store shared memory and semaphore identi */
#define bkMSG131 5124 /* ** Unable to lock the .lk file %s, errno = %i */
#define bkMSG132 5125 /* Found lock file %s unlocked. */
#define bkMSG137 5126 /* bkOpenControlArea error. */
#define bkMSG138 5127 /* Database must contain a Primary Recovery Area. */
#define bkMSG139 5128 /* Database must contain at least one Primary Recover */
#define bkMSG141 5129 /* bkForEachArea: buffer too small - need %l. */
#define bkMSG142 5130 /* bkForEachExtent: buffer too small - need %l. */
#define bkMSG144 5131 /* An error occurred during area truncation. rtc %l. */
#define bkMSG145 5132 /* Record %d in area %l not found. */
#define bkMSG146 5133 /* Truncating area %s. */
#define bkMSG147 5134 /* Attempted to exceed 2GB limit with file %s. */
#define bkMSG148 5135 /* Extent %s has the wrong creation date %s, */
#define bkMSG149 5136 /* Extent %s has a different last opened date %s, */
#define bkMSG150 5137 /* Creation date mismatch. */
#define bkMSG151 5138 /* Last open date mismatch. */
#define bkMSG152 5139 /* Control Area has a creation date of %s. */
#define bkMSG153 5140 /* Control Area has a last open date of %s. */
#define bkMSG154 5616 /* Unable to write object block to %s. %d */
#define bkMSG155 5617 /* File %s is too small, fixing */
#define bkMSG156 5618 /* File %s is smaller than the blocksize of %d */
#define bkMSG157 5619 /* Extent %s removed */
#define bkMSG158 5620 /* User requesting %i private read only buffers was a */
#define bkMSG159 5621 /* User requesting %i private read only buffers but n */
#define bkMSG160 5622 /* userid of upgrade is %d */
#define bkMSG161 5623 /* Invalid buffer lock down grade %D %l */
#define bkMSG162 5624 /* Index Anchor not found for index %s */
#define bkMSG163 5625 /* Dumping index anchor table */
#define bkMSG164 5626 /* slot %d, name %s, root %d */
