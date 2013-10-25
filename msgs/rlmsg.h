#define rlFTL001 5327 /* ** rlaixtn: Insufficient disk space to extend the  */
#define rlFTL002 5328 /* rlaidel: read past BOF of the after-image file. */
#define rlFTL003 5329 /* Cannot roll back a Ready-To-Commit task of user %d */
#define rlFTL008 5330 /* ai write out of sequence */
#define rlFTL011 5331 /* Unexpected extent switch note */
#define rlFTL012 5332 /* Invalid note length %i */
#define rlFTL015 5333 /* Unrecognized undo code %d, flags %d. */
#define rlFTL017 5334 /* ** Insufficient disk space to extend the Recovery  */
#define rlFTL018 5335 /* rlaiwt: can not find %l */
#define rlFTL019 5336 /* rlclins has invalid arg clstaddr==nextaddr = %l. */
#define rlFTL020 5337 /* buffer not dirty */
#define rlFTL021 5338 /* Backup after-image extent and mark it as empty. */
#define rlFTL022 5339 /* rlmemchk rlpbkout: note=%l rlctl=%l. */
#define rlFTL023 5340 /* rlmemchk mb_lasttask: note=%l mstrblk=%l. */
#define rlFTL024 5341 /* rlmemchk mb_aictr: note=%l mstrblk=%l. */
#define rlFTL025 5342 /* rlmemchk aiwrtloc: note=%l rlctl=%l. */
#define rlFTL026 5343 /* rlmemchk tx_count: note=%d ptran=%d. */
#define rlFTL027 5344 /* rlmemchk: transaction table mismatch. */
#define rlFTL028 5345 /* rlrdprv: There are no more notes to be read. */
#define rlFTL029 5346 /* rlrdprv missing part of a note. */
#define rlFTL030 5347 /* prevbiblk (rlrw.c) bad prev rlcounter. */
#define rlFTL031 5348 /* rlbird called with invalid address=%l. */
#define rlFTL032 5349 /*  rlrctrd: tx_count = %d note = %d */
#define rlFTL033 5350 /* rlrctrd: ready-to-commit tx table mismatch */
#define rlFTL035 5351 /* The Recovery Log file has the wrong cluster size. */
#define rlFTL036 5352 /* Bad counter in the .bi file. */
#define rlFTL037 5353 /* Two clusters in the .bi file have the same rlcount */
#define rlFTL038 5354 /* Infinite loop in the .bi file. */
#define rlFTL039 5355 /* Unable to find all orphan clusters. */
#define rlFTL040 5356 /* Unable to read memory note in roll forward */
#define rlFTL041 5357 /* Physical redo, BKUPDCTR=%l, note updctr=%l. */
#define rlFTL042 5358 /* rlaiswitch: No next ai area found to switch to. */
#define rlFTL043 5359 /* Live tx mismatch sequence %i note %i tx table %i. */
#define rlFTL044 5360 /* rdbifwd: bad rlcounter when reading forward. */
#define rlFTL045 5361 /* rllktxe: Attempting to acquire txe while holding b */
#define rlFTL046 5362 /* ** Your database cannot be repaired. You must rest */
#define rlFTL051 5363 /* rlbimv: note %i len %i too big */
#define rlFTL052 5364 /* rlputmod: wrong dpend %l */
#define rlFTL053 5365 /* rlputmod: dpend %l already written */
#define rlFTL054 5366 /* rlgetfree: no buffer to write */
#define rlFTL055 5367 /* rlgetfree loop */
#define rlFTL056 5368 /*  bi write queue disordered: dpend %l */
#define rlFTL057 5369 /* rlbiflsh failed on %i last write %i */
#define rlFTL060 5370 /* rlbinext: attempt to find a block %l past end of c */
#define rlFTL061 5371 /* rlbimv: movelen %i, bloff %i */
#define rlFTL062 5372 /* rlrdprv: note len %i too big */
#define rlFTL063 5373 /* rlbiout: Invalid rl block pointer */
#define rlFTL064 5374 /* rlbiout: Invalid buffer state %d */
#define rlFTL065 5375 /* rlaiextrd: Transaction table mismatch ptran: %i no */
#define rlFTL067 5376 /* rlultxe: txe not locked. */
#define rlFTL068 5377 /* rlbiwrt: Invalid rl block pointer. */
#define rlFTL069 5378 /* Expected %i tx entries but got %i. */
#define rlFTL070 5379 /* tl write location mismatch %l %l. */
#define rlFTL071 5380 /* tl write offset mismatch %l %l. */
#define rlFTL073 5381 /* rllktxe: lock error: requested %i holding %i. */
#define rlFTL074 5382 /* rllktxe:Wakened but lock not granted. */
#define rlFTL076 5383 /* Can't hold any latches while napping for group com */
#define rlFTL077 5384 /* tl block header smashed blknum %l smashed code %l. */
#define rlFTL078 5385 /* Redo did not start with a checkpoint note. */
#define rlFTL079 5386 /* Check point notes out of sequence %l %l. */
#define rlFTL080 5387 /* Crash recovery aborted - Database is damaged. */
#define rlFTL081 5388 /* rlrdnxt: note prefix and suffix lengths don't matc */
#define rlMSG001 5389 /* ** The database was last changed %s. */
#define rlMSG002 5390 /* ** The after-image file expected %s. */
#define rlMSG003 5391 /* ** Those dates don't match, so you have the wrong  */
#define rlMSG005 5392 /* After-image disabled. */
#define rlMSG014 5393 /* Before-image cluster size set to %l kb. */
#define rlMSG015 5394 /* ** This session is running with the non-raw (-r) p */
#define rlMSG016 5395 /* ** This session is being run with the no-integrity */
#define rlMSG020 5396 /* ** The database was last used %s. */
#define rlMSG021 5397 /* ** The Recovery Log file expected %s. */
#define rlMSG022 5398 /* ** Those dates don't match, so you have the wrong  */
#define rlMSG023 5399 /* This database has not been fully restored and may  */
#define rlMSG024 5400 /* ** The FORCE option was given, database recovery w */
#define rlMSG025 5401 /* ** The last session was run with the no integrity  */
#define rlMSG026 5402 /* ** The last session was run with the non-raw (-r)  */
#define rlMSG027 5403 /* ** Your database may have been damaged. */
#define rlMSG029 5404 /* ** Your database was damaged. Dump its data and re */
#define rlMSG030 5405 /* ** An earlier -r session crashed, the database may */
#define rlMSG045 5406 /* ** The file %s is not the correct after-image file */
#define rlMSG048 5407 /* Invalid RL file, Recovery Log file is zero length. */
#define rlMSG052 5408 /* Can't switch to after-image extent %s it is full. */
#define rlMSG053 5409 /* Can't switch to after-image extent %s it is full. */
#define rlMSG054 5410 /* Backup ai extent and mark it as empty. */
#define rlMSG055 5411 /* Switched to ai extent %s. */
#define rlMSG056 5412 /* This is after-image file number %l since the last  */
#define rlMSG057 5413 /* Can't extend ai extent %s */
#define rlMSG058 5414 /* Can't roll forward an empty after-image extent fil */
#define rlMSG059 5415 /* Before-image block size set to %l kb (%l bytes). */
#define rlMSG060 5416 /* Invalid block size in AI file found %d, expected % */
#define rlMSG087 5417 /*  dpend: %l sdpend: %l dbkey: %D */
#define rlMSG088 5418 /*  dpend: %l sdpend %l dbkey: %D */
#define rlMSG089 5419 /*  rlwritten: %l, rldepend: %l */
#define rlMSG090 5420 /* Undo trans %l invalid transaction id. */
#define rlMSG091 5421 /* Undo trans %l with invalid bi address %l. */
#define rlMSG092 5422 /* After-imaging disabled by the FORCE parameter. */
#define rlMSG094 5423 /* AI file cannot be used with this database. */
#define rlMSG100 5424 /* Invalid after image version number found in ai fil */
#define rlMSG101 5425 /* This is a version 0 Gemini ai file. */
#define rlMSG102 5426 /* The ai file must be truncated. */
#define rlMSG103 5427 /* See documentation for aimage truncate */
#define rlMSG104 5428 /* This is not a Gemini ai file */
#define rlMSG106 5429 /* rlInit: Could not get memory for rl control */
#define rlMSG112 5430 /* Invalid blocksize requested for %s, attempting to  */
#define rlMSG113 5431 /* ** Insufficient disk space to extend the Recovery  */
#define rlMSG115 5432 /* Begin Physical Redo Phase at %l . */
#define rlMSG117 5433 /* Begin Logical Undo Phase, %i  incomplete transacti */
#define rlMSG118 5434 /* Logical Undo Phase Complete. */
#define rlMSG120 5435 /* Physical Undo Phase Completed at %i . */
#define rlMSG121 5436 /* Database Server shutting down as a result of after */
#define rlMSG123 5437 /* RL file size is greater than the RL file threshold */
#define rlMSG124 5438 /* You must truncate the RL file or increase the RL t */
#define rlMSG134 5439 /* Database must have a TL area to enable 2pc. */
#define rlMSG135 5440 /* Physical Redo Phase Completed at blk %l off %l upd */
#define rlMSG136 5441 /* Begin Logical Undo Phase, %i incomplete transactio */
#define rlMSG137 5442 /* Begin Physical Undo %i transactions at block %l of */
#define rlMSG138 5443 /* Block update counter is too low for undo. */
#define rlMSG139 5444 /* Block DBKEY %D block counter %l note counter %l */
#define rlMSG140 5445 /* Expected ai file number %l but file specified is % */
#define rlMSG141 5446 /* Cluster aging has detected that the clock has gone */
#define rlMSG145 5447 /* Incomplete ai file detected, expected %d, got %d.  */
