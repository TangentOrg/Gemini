/* Copyright (C) 2000 NuSphere Corporation
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* This File has been modified by: Mike Furgal, NuSphere Corp. on 12/6/01 */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#ifdef PACKAGE
#undef PACKAGE
#endif
#ifdef VERSION
#undef VERSION
#endif
#include "dsmpub.h"

#ifdef OS2
#undef OS2
#endif
/* class for the the gemini handler */

enum enum_key_string_options{KEY_CREATE,KEY_DELETE,KEY_CHECK};
typedef struct st_gemini_share {
  ha_rows  *rec_per_key;
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} GEM_SHARE;

typedef struct gemBlobDesc
{
  dsmBlobId_t  blobId;
  dsmBuffer_t *pBlob;
} gemBlobDesc_t;

class ha_gemini: public handler
{
  /* define file as an int for now until we have a real file struct */
  int file;
  uint int_option_flag;
  dsmObject_t tableNumber;
  dsmIndex_t  *pindexNumbers;  // dsm object numbers for the indexes on this table 
  dsmRecid_t lastRowid;
  uint  last_dup_key;
  bool fixed_length_row, key_read, using_ignore;
  byte *rec_buff;
  dsmKey_t *pbracketBase;
  dsmKey_t *pbracketLimit;
  dsmKey_t *pfoundKey;
  dsmMask_t    tableStatus;   // Crashed/repair status
  gemBlobDesc_t *pBlobDescs;

  int index_open(char *tableName,bool tempTable);
  int  pack_row(byte **prow, int *ppackedLength, const byte *record, 
                bool update);
  int  unpack_row(char *record, char *prow);
  int  check_row(char *prow);
  int findRow(THD *thd, dsmMask_t findMode, byte *buf);
  int fetch_row(void *gemini_context, const byte *buf);
  int handleIndexEntries(const byte * record, dsmRecid_t recid,
                                  enum_key_string_options option);

  int handleIndexEntry(const byte * record, dsmRecid_t recid,
				enum_key_string_options option,uint keynr);

  int createKeyString(const byte * record, KEY *pkeyinfo,
                               unsigned char *pkeyBuf, int bufSize,
                               int  *pkeyStringLen, short geminiIndexNumber,
                               bool *thereIsAnull);
  int fullCheck(THD *thd,byte *buf);

  int pack_key( uint keynr, dsmKey_t  *pkey, 
                           const byte *key_ptr, uint key_length);

  void unpack_key(char *record, dsmKey_t *key, uint index);

  int key_cmp(uint keynr, const byte * old_row,
                         const byte * new_row, bool updateStats);

  int saveKeyStats(THD *thd);
  void get_index_stats(THD *thd);

  int gemini_backup(THD* thd, HA_CHECK_OPT* check_opt, uint options,
                    char *backup_dir);

  dsmMask_t  lockMode;  /* Shared or exclusive      */
  int        tableLock; /* 1 if locked by table lock statement */
  short cursorId;  /* cursorId of active index cursor if any   */

  THR_LOCK_DATA lock;
  GEM_SHARE     *share;

 public:
  ha_gemini(TABLE *table): handler(table), file(0),
    int_option_flag(HA_READ_NEXT | HA_READ_PREV |
		    HA_REC_NOT_IN_SEQ |
		    HA_KEYPOS_TO_RNDPOS | HA_READ_ORDER | HA_LASTKEY_ORDER |
		    HA_LONGLONG_KEYS | HA_NULL_KEY | HA_HAVE_KEY_READ_ONLY |
		    HA_BLOB_KEY | 
		    /* HA_NO_TEMP_TABLES |*/ HA_NO_FULLTEXT_KEY |
		    /*HA_NOT_EXACT_COUNT | */ 
		    /*HA_KEY_READ_WRONG_STR |*/ HA_DROP_BEFORE_CREATE),
                    pbracketBase(0),pbracketLimit(0),pfoundKey(0),
                    lockMode(0),tableLock(0),
                    cursorId(DSM_NULLCURSID)
  {
  }
  ~ha_gemini() {}
  const char *table_type() const { return "Gemini"; }
  const char **bas_ext() const;
  ulong option_flag() const { return int_option_flag; }
  uint max_record_length() const { return DSM_MAXRECSZ; }
  uint max_keys()          const { return MAX_KEY-1; }
  uint max_key_parts()     const { return MAX_REF_PARTS; }
  uint max_key_length()    const { return DSM_MAXKEYSZ / 2; }
  uint extra_rec_buf_length()	 { return 32; } /* Extra for vsts */
  bool fast_key_read()	   { return 1;}
  bool has_transactions()  { return 1;}

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  double scan_time();
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint index, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_next(byte * buf);
  int index_next_same(byte * buf, const byte *key, uint keylen);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int rnd_init(bool scan=1);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int analyze(THD* thd, HA_CHECK_OPT* check_opt);
  int check(THD* thd, HA_CHECK_OPT* check_opt);
  int repair(THD* thd,  HA_CHECK_OPT* check_opt);
  int restore(THD* thd, HA_CHECK_OPT* check_opt);
  int backup(THD* thd, HA_CHECK_OPT* check_opt);
  int olbackup(THD* thd, HA_CHECK_OPT* check_opt, char *backup_dir);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int external_lock(THD *thd, int lock_type);
  void unlock_row();
  virtual longlong get_auto_increment();
  void position(byte *record);
  ha_rows records_in_range(int inx,
			   const byte *start_key,uint start_key_len,
			   enum ha_rkey_function start_search_flag,
			   const byte *end_key,uint end_key_len,
			   enum ha_rkey_function end_search_flag);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, register TABLE *form,
	     HA_CREATE_INFO *create_info);
  int delete_table(const char *name);
  int rename_table(const char* from, const char* to);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  int init_lock_query(THD *thd);
  int lock_query(THD *thd, HA_LOCK_INFO *lockinfo);
};

#define GEMOPT_FLUSH_LOG          0x00000001
#define GEMOPT_UNBUFFERED_IO      0x00000002

#define GEMINI_RECOVERY_FULL      0x00000001
#define GEMINI_RECOVERY_NONE      0x00000002
#define GEMINI_RECOVERY_FORCE     0x00000004

#define GEM_OPTID_SPIN_RETRIES      1
#define GEM_OPTID_LOCK_WAIT_TIMEOUT      2

#define GEM_SYM_FILE  "bin/mysqld.sym"
#define GEM_MSGS_FILE "gemmsgs.db"

/* field/key defines */
#define DSM_INT        14
#define DSM_FLOAT      24
#define DSM_DOUBLE     25
#define DSM_CHAR       26
#define DSM_TINYBLOB   28
#define DSM_BLOB       30
#define DSM_MEDIUMBLOB 32
#define DSM_LONGBLOB   35
#define DSM_DECIMAL    251

extern bool gemini_skip;
extern SHOW_COMP_OPTION have_gemini;
extern long gemini_options;
extern long gemini_buffer_cache;
extern long gemini_io_threads;
extern long gemini_log_cluster_size;
extern long gemini_locktablesize;
extern long gemini_lock_wait_timeout;
extern long gemini_spin_retries;
extern long gemini_connection_limit;
extern char *gemini_basedir;
extern TYPELIB gemini_recovery_typelib;
extern ulong gemini_recovery_options;

bool gemini_init(void);
bool gemini_end(void);
bool gemini_flush_logs(void);
int gemini_commit(THD *thd);
int gemini_rollback(THD *thd);
int gemini_recovery_logging(THD *thd, bool on);
void gemini_disconnect(THD *thd);
int gemini_rollback_to_savepoint(THD *thd);
int gemini_parse_table_name(const char *fullname, char *dbname, 
			    char *tabname);
int gemini_is_vst(const char *pname);
int gemini_set_option_long(int optid, long optval);
int gemini_online_backup(THD *thd);
int gemini_online_backup_end(THD *thd);

const int gemini_blocksize = DSM_BLKSIZE;
const int gemini_recbits = DSM_DEFAULT_RECBITS;

#ifdef  __cplusplus
extern "C" {
#endif
/* index and key prototypes for the MySQL integration */
int gemKeyInit(unsigned char *pKeyBuf, int *pKeyStringLen, short index);
int gemFieldToIdxComponent(unsigned char *pField, unsigned long fldLength,
                           int fieldType, int fieldIsNull,
                           int fieldIsUnsigned, unsigned char *pCompBuffer,
                           int bufferMaxSize, int *pCompLength);
int gemIdxComponentToField( unsigned char    *pComponent, int fieldType,
		            unsigned char    *pField, int fieldSize,
                            int decimalDigits, int *pfieldIsNull);  
 
int gemKeyAddHigh(unsigned char *pkeyBuf, int *pkeyStringLen);
int gemKeyHigh(unsigned char *pkeyBuf, int *pkeyStringLen, short index);
int gemKeyLow(unsigned char *pkeyBuf, int *pkeyStringLen, short index);

void gemtrace(void);
#ifdef  __cplusplus
}
#endif



/* Below is the code for dynamic linking.  All generic changes should
   be above this line */
/* defines for dynamic linking */
#define dsmAPW                      (*gemfp.fp_dsmAPW)
#define dsmAreaClose                (*gemfp.fp_dsmAreaClose)
#define dsmAreaCreate               (*gemfp.fp_dsmAreaCreate)
#define dsmAreaDelete               (*gemfp.fp_dsmAreaDelete)
#define dsmAreaFlush                (*gemfp.fp_dsmAreaFlush)
#define dsmAreaNew                  (*gemfp.fp_dsmAreaNew)
#define dsmAreaOpen                 (*gemfp.fp_dsmAreaOpen)
#define dsmBlobDelete               (*gemfp.fp_dsmBlobDelete)
#define dsmBlobEnd                  (*gemfp.fp_dsmBlobEnd)
#define dsmBlobGet                  (*gemfp.fp_dsmBlobGet)
#define dsmBlobPut                  (*gemfp.fp_dsmBlobPut)
#define dsmBlobStart                (*gemfp.fp_dsmBlobStart)
#define dsmBlobUpdate               (*gemfp.fp_dsmBlobUpdate)
#define dsmContextCopy              (*gemfp.fp_dsmContextCopy)
#define dsmContextCreate            (*gemfp.fp_dsmContextCreate)
#define dsmContextGetLong           (*gemfp.fp_dsmContextGetLong)
#define dsmContextSetLong           (*gemfp.fp_dsmContextSetLong)
#define dsmContextSetString         (*gemfp.fp_dsmContextSetString)
#define dsmCursorCreate             (*gemfp.fp_dsmCursorCreate)
#define dsmCursorDelete             (*gemfp.fp_dsmCursorDelete)
#define dsmCursorFind               (*gemfp.fp_dsmCursorFind)
#define dsmDatabaseProcessEvents    (*gemfp.fp_dsmDatabaseProcessEvents)
#define dsmExtentCreate             (*gemfp.fp_dsmExtentCreate)
#define dsmExtentDelete             (*gemfp.fp_dsmExtentDelete)
#define dsmExtentUnlink             (*gemfp.fp_dsmExtentUnlink)
#define dsmIndexRowsInRange         (*gemfp.fp_dsmIndexRowsInRange)
#define dsmIndexStatsGet            (*gemfp.fp_dsmIndexStatsGet)
#define dsmIndexStatsPut            (*gemfp.fp_dsmIndexStatsPut)
#define dsmKeyCreate                (*gemfp.fp_dsmKeyCreate)
#define dsmKeyDelete                (*gemfp.fp_dsmKeyDelete)
#define dsmLockQueryGetLock         (*gemfp.fp_dsmLockQueryGetLock)
#define dsmLockQueryInit            (*gemfp.fp_dsmLockQueryInit)
#define dsmMsgdCallBack             (*gemfp.fp_dsmMsgdCallBack)
#define dsmOLBackupEnd              (*gemfp.fp_dsmOLBackupEnd)
#define dsmOLBackupInit             (*gemfp.fp_dsmOLBackupInit)
#define dsmOLBackupRL               (*gemfp.fp_dsmOLBackupRL)
#define dsmOLBackupSetFlag          (*gemfp.fp_dsmOLBackupSetFlag)
#define dsmObjectCreate             (*gemfp.fp_dsmObjectCreate)
#define dsmObjectDeleteAssociate    (*gemfp.fp_dsmObjectDeleteAssociate)
#define dsmObjectInfo               (*gemfp.fp_dsmObjectInfo)
#define dsmObjectLock               (*gemfp.fp_dsmObjectLock)
#define dsmObjectNameToNum          (*gemfp.fp_dsmObjectNameToNum)
#define dsmObjectRename             (*gemfp.fp_dsmObjectRename)
#define dsmObjectUnlock             (*gemfp.fp_dsmObjectUnlock)
#define dsmRLwriter                 (*gemfp.fp_dsmRLwriter)
#define dsmRecordCreate             (*gemfp.fp_dsmRecordCreate)
#define dsmRecordDelete             (*gemfp.fp_dsmRecordDelete)
#define dsmRecordGet                (*gemfp.fp_dsmRecordGet)
#define dsmRecordUpdate             (*gemfp.fp_dsmRecordUpdate)
#define dsmRowCount                 (*gemfp.fp_dsmRowCount)
#define dsmShutdown                 (*gemfp.fp_dsmShutdown)
#define dsmShutdownSet              (*gemfp.fp_dsmShutdownSet)
#define dsmTableAutoIncrement       (*gemfp.fp_dsmTableAutoIncrement)
#define dsmTableAutoIncrementSet    (*gemfp.fp_dsmTableAutoIncrementSet)
#define dsmTableReset               (*gemfp.fp_dsmTableReset)
#define dsmTableScan                (*gemfp.fp_dsmTableScan)
#define dsmTableStatus              (*gemfp.fp_dsmTableStatus)
#define dsmTransaction              (*gemfp.fp_dsmTransaction)
#define dsmUserConnect              (*gemfp.fp_dsmUserConnect)
#define dsmUserDisconnect           (*gemfp.fp_dsmUserDisconnect)
#define dsmWatchdog                 (*gemfp.fp_dsmWatchdog)

#define gemFieldToIdxComponent      (*gemfp.fp_gemFieldToIdxComponent)
#define gemIdxComponentToField      (*gemfp.fp_gemIdxComponentToField)
#define gemKeyAddHigh               (*gemfp.fp_gemKeyAddHigh)
#define gemKeyHigh                  (*gemfp.fp_gemKeyHigh)
#define gemKeyInit                  (*gemfp.fp_gemKeyInit)
#define gemKeyLow                   (*gemfp.fp_gemKeyLow)

/* Function Prototypes for dynamic linking */
struct gem_fp
{
dsmStatus_t  (*fp_dsmAPW)( dsmContext_t *pcontext);
dsmStatus_t  (*fp_dsmAreaClose)(dsmContext_t *pcontext, dsmArea_t area);
dsmStatus_t  (*fp_dsmAreaCreate)( 
              dsmContext_t 	*pcontext, 	/* IN database context */ 
              dsmBlocksize_t	blockSize, 	/* IN are block size in bytes */
	      dsmAreaType_t	areaType, 	/* IN type of area */
	      dsmArea_t		areaNumber,	/* IN area number */
              dsmRecbits_t	areaRecbits,    /* IN # of record bits */
              dsmText_t        *areaName);      /* IN name of new area */

dsmStatus_t  (*fp_dsmAreaDelete)(
              dsmContext_t 	*pcontext,	/* IN database context */ 
              dsmArea_t		area);		/* IN area to delete */

dsmStatus_t (*fp_dsmAreaFlush)(dsmContext_t *pcontext, dsmArea_t area, int flags);

dsmStatus_t  (*fp_dsmAreaNew)(
              dsmContext_t 	*pcontext,     /* IN database context */
	      dsmBlocksize_t	blockSize,     /* IN are block size in bytes */
	      dsmAreaType_t	areaType,      /* IN type of area */
	      dsmArea_t		*pareaNumber,  /* OUT area number */
              dsmRecbits_t	areaRecbits,   /* IN # of record bits */
              dsmText_t        *areaName);     /* IN name of new area */

dsmStatus_t (*fp_dsmAreaOpen)(dsmContext_t *pcontext, dsmArea_t area, int refresh);

dsmStatus_t   (*fp_dsmBlobDelete)( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t   (*fp_dsmBlobEnd)( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob);        /* IN blob descriptor */

dsmStatus_t  (*fp_dsmBlobGet)( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t  (*fp_dsmBlobPut)( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t   (*fp_dsmBlobStart)( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob);        /* IN blob descriptor */

dsmStatus_t   (*fp_dsmBlobUpdate)( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t  (*fp_dsmContextCopy)( 
        dsmContext_t  *psource,   /* IN  database context */
        dsmContext_t **pptarget,  /* OUT database context */
        dsmMask_t      copyMode); /* IN  what to copy     */

dsmStatus_t  (*fp_dsmContextCreate)(dsmContext_t **pcontext); /* OUT database context */

dsmStatus_t  (*fp_dsmContextGetLong)( 
    dsmContext_t        *pcontext,  /* IN/OUT database context  */
    dsmTagId_t          tagId,      /* IN Tag identifier        */
    dsmTagLong_t        *ptagValue);/* IN tag value             */

dsmStatus_t  (*fp_dsmContextSetLong)( 
    dsmContext_t        *pcontext,  /* IN/OUT database context  */
    dsmTagId_t          tagId,      /* IN Tag identifier        */
    dsmTagLong_t        tagValue);  /* IN tag value             */

dsmStatus_t  (*fp_dsmContextSetString)( 
    dsmContext_t        *pcontext,    /* IN/OUT database context */
    dsmTagId_t          tagId,        /* IN Tag identifier       */
    dsmLength_t         tagLength,    /* IN Length of string     */
    dsmBuffer_t         *ptagString); /* IN tagValue             */

dsmStatus_t  (*fp_dsmCursorCreate)( 
        dsmContext_t *pcontext, /* IN/OUT database context */
        dsmTable_t    table,    /* IN table associated with cursor */
        dsmIndex_t    index,    /* IN index associated with cursor */
        dsmCursid_t  *pcursid,  /* OUT created cursor id */
        dsmCursid_t  *pcopy);   /* IN cursor id to copy */

dsmStatus_t  (*fp_dsmCursorDelete)( 
        dsmContext_t  *pcontext,        /* IN database context */
        dsmCursid_t   *pcursid,         /* IN  pointer to cursor to delete */
        dsmBoolean_t  deleteAll);       /* IN delete all cursors indicator */

dsmStatus_t  (*fp_dsmCursorFind)( 
        dsmContext_t *pcontext,         /* IN database context */
        dsmCursid_t  *pcursid,          /* IN/OUT cursor */
        dsmKey_t     *pkeyBase,         /* IN base key of bracket for find */
        dsmKey_t     *pkeylimit,        /* IN limit key of bracket for find */
        dsmMask_t     findType,         /* IN DSMPARTIAL DSMUNIQUE ...      */
        dsmMask_t     findMode,         /* IN find mode bit mask */
        dsmMask_t     lockMode,         /* IN lock mode bit mask */
        dsmObject_t  *pObjectNumber,    /* OUT object number the index is on */
        dsmRecid_t   *precid,           /* OUT recid found */
        dsmKey_t     *pKey);            /* OUT [optional] key found */

dsmStatus_t  (*fp_dsmDatabaseProcessEvents)(
        dsmContext_t  *pcontext);       /* IN database context */

dsmStatus_t  (*fp_dsmExtentCreate)( 
                dsmContext_t	*pcontext,	/* IN database context */
               	dsmArea_t 	area,		/* IN area for the extent */
	        dsmExtent_t	extentId,	/* IN extent id */
	        dsmSize_t 	initialSize,	/* IN initial size in blocks */
	        dsmMask_t	extentMode,	/* IN extent mode bit mask */
	        dsmText_t	*pname);	/* IN extent path name */

dsmStatus_t  (*fp_dsmExtentDelete)(
                dsmContext_t	*pcontext,	/* IN database context */ 
                dsmArea_t	area);		/* IN area to delete */

dsmStatus_t   (*fp_dsmExtentUnlink)(dsmContext_t *pcontext);

dsmStatus_t   (*fp_dsmIndexRowsInRange)(
        dsmContext_t  *pcontext,           /* IN database context */
        dsmKey_t      *pbase,              /* IN bracket base     */
        dsmKey_t      *plimit,             /* IN bracket limit    */
        dsmObject_t    tableNum,
        float         *pctInrange);        /* Out percent of index in bracket */

dsmStatus_t   (*fp_dsmIndexStatsGet)(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN index component number */
        LONG64        *prowsPerKey);       /* IN rows per key value */

dsmStatus_t   (*fp_dsmIndexStatsPut)(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN index component # */
        LONG64        rowsPerKey);         /* IN rows per key value */

dsmStatus_t  (*fp_dsmKeyCreate)( 
        dsmContext_t  *pcontext,   /* IN database context */
	dsmKey_t      *pkey,       /* IN the key to be inserted in the index*/
	dsmTable_t   table,        /* IN the table the index is on.         */
	dsmRecid_t   recordId,     /* IN the recid of the record the 
			              index entry is for         */
	dsmText_t    *pname);	   /* IN refname for Rec in use msg    */

dsmStatus_t  (*fp_dsmKeyDelete)(   
        dsmContext_t *pContext,
	dsmKey_t *pkey,            /* IN the key to be inserted in the index*/
	COUNT   table,             /* IN the table the index is on.         */
	dsmRecid_t recordId,       /* IN the recid of the record the 
			              index entry is for           	*/
	dsmMask_t    lockmode,     /* IN lockmode            */
	dsmText_t    *pname);	   /* refname for Rec in use msg           */

dsmStatus_t  (*fp_dsmLockQueryGetLock)(
        dsmContext_t *pcontext,     /* IN database context */
        dsmTable_t    tableNum,     /* IN table number */
        GEM_LOCKINFO *plockinfo);   /* OUT lock information */

dsmStatus_t  (*fp_dsmLockQueryInit)(
        dsmContext_t *pcontext,     /* IN database context */
        int           getLock);     /* IN lock/unlock lock table */

dsmStatus_t (*fp_dsmMsgdCallBack)(
    dsmContext_t         *pcontext,     /* IN database context              */
    ...);                               /* IN variable length arg list      */ 

dsmStatus_t  (*fp_dsmOLBackupEnd)(
        dsmContext_t *pcontext);    /* IN database context */

dsmStatus_t  (*fp_dsmOLBackupInit)(
        dsmContext_t *pcontext);    /* IN database context */

dsmStatus_t  (*fp_dsmOLBackupRL)(
        dsmContext_t *pcontext,     /* IN database context */
        dsmText_t    *ptarget);     /* IN backup target path and filename */

dsmStatus_t  (*fp_dsmOLBackupSetFlag)(
        dsmContext_t *pcontext,      /* IN database context */
        dsmText_t    *ptarget,       /* IN path to gemini db */
        dsmBoolean_t  backupStatus); /* IN turn backup bit on/off */

dsmStatus_t   (*fp_dsmObjectCreate)( 
            dsmContext_t       *pContext,   /* IN database context */
	    dsmArea_t 	       area,        /* IN area */
	    dsmObject_t	       *pobject,    /* IN/OUT for object id */
	    dsmObjectType_t    objectType,  /* IN object type */
	    dsmObjectAttr_t    objectAttr,  /* IN object attributes */
            dsmObject_t        associate,   /* IN object number of associate
                                               object */
            dsmObjectType_t    associateType, /* IN type of associate object
                                                 usually an index           */
            dsmText_t          *pname,      /* IN name of the object */
	    dsmDbkey_t         *pblock,     /* IN/OUT dbkey of object block */
	    dsmDbkey_t         *proot);     /* OUT dbkey of object root */

dsmStatus_t  (*fp_dsmObjectDeleteAssociate)(
            dsmContext_t    *pcontext,        /* IN database context */
	    dsmObject_t      tableNum,        /* IN number of table */
            dsmArea_t       *indexArea);      /* OUT area for the indexes */

dsmStatus_t   (*fp_dsmObjectInfo)( 
    dsmContext_t       *pContext,          /* IN database context */
    dsmObject_t         objectNumber,      /* IN object number    */
    dsmObjectType_t     objectType,        /* IN object type      */
    dsmObject_t         associate,         /* IN Associate object number */
    dsmArea_t           *parea,            /* OUT area object is in */
    dsmObjectAttr_t     *pobjectAttr,      /* OUT Object attributes    */
    dsmObjectType_t     *passociateType,   /* OUT Associate object type  */
    dsmDbkey_t          *pblock,           /* OUT dbkey of object block  */
    dsmDbkey_t          *proot);           /* OUT dbkey of index root block */

dsmStatus_t   (*fp_dsmObjectLock)( 
            dsmContext_t       *pContext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmRecid_t         recid,      /* IN object type */
	    dsmMask_t          lockMode,   /* IN delete mode bit mask */
            COUNT              waitFlag,   /* IN client:1(wait), server:0 */
	    dsmText_t          *pname);    /* IN reference name for messages */

dsmStatus_t (*fp_dsmObjectNameToNum)(dsmContext_t *pcontext,   /* IN database context */
		   dsmBoolean_t  tempTable,  /* IN 1 - if temporary object */
		   dsmText_t  *pname,          /* IN the name */
		   dsmObject_t         *pnum);  /* OUT number */

dsmStatus_t  (*fp_dsmObjectRename)( 
            dsmContext_t   *pContext,     /* IN database context */
            dsmObject_t     tableNumber,  /* IN table number */
            dsmText_t      *pNewName,     /* IN new name of the object */
            dsmText_t      *pIdxExtName,  /* IN new name of the index extent */
            dsmText_t      *pNewExtName,  /* IN new name of the extent */
            dsmArea_t      *pIndexArea,   /* OUT index area */
            dsmArea_t      *pTableArea);  /* OUT table area */

dsmStatus_t   (*fp_dsmObjectUnlock)( 
            dsmContext_t       *pContext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmRecid_t         recid,      /* IN object type */
	    dsmMask_t          lockMode);  /* IN delete mode bit mask */

dsmStatus_t  (*fp_dsmRLwriter)( dsmContext_t *pcontext);

dsmStatus_t  (*fp_dsmRecordCreate )( 
        dsmContext_t  *pContext,     /* IN database context */
        dsmRecord_t   *pRecord);     /* IN/OUT record descriptor */

dsmStatus_t  (*fp_dsmRecordDelete )( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN record to delete */
        dsmText_t    *pname);       /* IN msg reference name */

dsmStatus_t  (*fp_dsmRecordGet)( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN/OUT rec buffer to store record */
        dsmMask_t     getMode);     /* IN get mode bit mask */

dsmStatus_t  (*fp_dsmRecordUpdate)( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN new record to store */
        dsmText_t    *pName);       /* IN msg reference name */

dsmStatus_t  (*fp_dsmRowCount)(
        dsmContext_t    *pcontext,
        dsmTable_t       tableNumber,
        ULONG64          *prows);

dsmStatus_t  (*fp_dsmShutdownMarkUser)(
        dsmContext_t     *pcontext,    /* IN database context */
        psc_user_number_t userNumber); /* IN the usernumber */
dsmStatus_t  (*fp_dsmShutdownUser)(
        dsmContext_t     *pcontext,   /* IN database context */
        psc_user_number_t server);    /* IN the server identifier */
dsmStatus_t  (*fp_dsmShutdown)(
        dsmContext_t  *pcontext,        /* IN database context */
        int            exitFlags,
        int            exitCode);
dsmStatus_t  (*fp_dsmShutdownSet)(
        dsmContext_t  *pcontext,        /* IN database context */
        int            flag);

dsmStatus_t  (*fp_dsmTableAutoIncrement)(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 *pvalue,int update);
dsmStatus_t  (*fp_dsmTableAutoIncrementSet)(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 value);

dsmStatus_t  (*fp_dsmTableReset)(dsmContext_t *pcontext, 
              dsmTable_t tableNumber, 
              int numIndexes,
              dsmIndex_t   *pindexes);

dsmStatus_t  (*fp_dsmTableScan)(
            dsmContext_t    *pcontext,        /* IN database context */
	    dsmRecord_t     *precordDesc,     /* IN/OUT row descriptor */
	    int              direction,       /* IN next, previous...  */
	    dsmMask_t        lockMode,        /* IN nolock share or excl */
            int              repair);         /* IN repair rm chain     */

dsmStatus_t  (*fp_dsmTableStatus)(dsmContext_t *pcontext, 
               dsmTable_t tableNumber,
               dsmMask_t *ptableStatus);

dsmStatus_t  (*fp_dsmTransaction)( 
        dsmContext_t  *pcontext,          /* IN/OUT database context */
        dsmTxnSave_t  *pSavePoint,        /* IN/OUT savepoint           */
        dsmTxnCode_t  txnCode);           /* IN function code       */

dsmStatus_t  (*fp_dsmUserConnect)( 
        dsmContext_t *pContext, /* IN/OUT database context */
        dsmText_t    *prunmode, /* IN string describing use, goes in lg file  */
        int           mode);    /* bit mask with these bits: */

dsmStatus_t  (*fp_dsmUserDisconnect)(
        dsmContext_t *pContext,    /* IN/OUT database context */
        int           exitflags);  /* may have NICEBIT, etc   */ 

dsmStatus_t  (*fp_dsmWatchdog)(
        dsmContext_t  *pcontext);       /* IN database context */


int (*fp_gemKeyInit)(unsigned char *pKeyBuf, int *pKeyStringLen, short index);
int (*fp_gemFieldToIdxComponent)(unsigned char *pField, unsigned long fldLength,
                           int fieldType, int fieldIsNull,
                           int fieldIsUnsigned, unsigned char *pCompBuffer,
                           int bufferMaxSize, int *pCompLength);
int (*fp_gemIdxComponentToField)( unsigned char    *pComponent, int fieldType,
		            unsigned char    *pField, int fieldSize,
                            int decimalDigits, int *pfieldIsNull);  
 
int (*fp_gemKeyAddHigh)(unsigned char *pkeyBuf, int *pkeyStringLen);
int (*fp_gemKeyHigh)(unsigned char *pkeyBuf, int *pkeyStringLen, short index);
int (*fp_gemKeyLow)(unsigned char *pkeyBuf, int *pkeyStringLen, short index);

void (*fp_uttrace)();
};
