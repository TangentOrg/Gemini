#define mtFTL004 5294 /* Illegal semaphore address */
#define mtFTL005 5295 /* bad latch wake usr %i latch %i waiting for %i. */
#define mtFTL006 5296 /* mtlatch: already holding %i. */
#define mtFTL007 5297 /* mtlatch %i, holding %X. */
#define mtFTL008 5298 /* invalid latch type %i. */
#define mtFTL009 5299 /* mtunlatch %d: not owner. */
#define mtFTL010 5300 /* latch %i depth %i. */
#define mtFTL011 5301 /* mtunlatch: invalid latch type */
#define mtFTL012 5302 /* mtrelatch: lost latch depth counter %i. */
#define mtFTL013 5303 /* mtlkmux %i, holding %X. */
#define mtFTL014 5304 /* muxfree %i not owner. */
#define mtFTL015 5305 /* mtLockWait: waitCode 0. */
#define mtFTL016 5306 /* lkwait holding shm locks %X. */
#define mtMSG003 5307 /* The database is being shutdown. */
#define mtMSG004 5308 /* Warning: another user is using this database in up */
#define mtMSG005 5309 /* Releasing regular latch. latchId: %d */
#define mtMSG007 5310 /* mtLatWake self. */
#define mtMSG008 5311 /* mtLockWait: umLockWait returns %i. */
#define mtMSG009 5312 /* mtusrwake: self. */
#define mtMSG010 5313 /* mtusrwake: wrong queue, usr %i, queue %i, waiting  */
#define mtMSG011 5314 /* mtusrwake: user %i already waked, queue %i wake %i */
#define mtMSG012 5315 /* mtusrwake: user %i still in queue %i. */
#define mtMSG013 5316 /* Releasing multiplexed latch. latchId: %d */
#define mtMSG019 5317 /* There is a schema lock being held by %s on %s, exi */
