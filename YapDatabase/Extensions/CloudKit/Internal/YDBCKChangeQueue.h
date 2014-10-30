#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>


@interface YDBCKChangeQueue : NSObject

/**
 * Every YapDatabaseCloudKit instance has a single masterQueue,
 * which tracks the CloutKit related changeSets per commit.
 * 
 * This information is used to create and track the NSOperation's that are pushing data to the cloud,
 * as well as the corresponding information that we need to save to persistent storage.
**/
- (instancetype)initMasterQueue;
- (instancetype)initMasterQueueWithChangeSets:(NSMutableArray *)changeSets;

#pragma mark PendingQueue Lifecycle

/**
 * Invoke this method from 'prepareForReadWriteTransaction' in order to fetch a 'pendingQueue' object.
 *
 * This pendingQueue object will then be used to keep track of all the changes
 * that need to be written to the changesTable.
**/
- (YDBCKChangeQueue *)newPendingQueue;

/**
 * This should be done AFTER the pendingQueue has been written to disk,
 * at the end of the flushPendingChangesToExtensionTables method.
**/
- (void)mergePendingQueue:(YDBCKChangeQueue *)pendingQueue;

#pragma mark Properties

/**
 * Determining queue type.
 * Primarily used for sanity checks.
**/
@property (nonatomic, readonly) BOOL isMasterQueue;
@property (nonatomic, readonly) BOOL isPendingQueue;

/**
 * Each commit that makes one or more changes to a CKRecord (insert/modify/delete)
 * will result in one or more YDBCKChangeSet(s).
 * There is one YDBCKChangeSet per databaseIdentifier.
 * So a single commit may possibly generate multiple changeSets.
 *
 * Thus a changeSet encompasses all the relavent CloudKit related changes per database, per commit.
 * 
 * inFlightChangeSets   : the changeSets that have been handled over to CloudKit via CKModifyRecordsOperation's.
 * pendingChangeSetsXXX : the changeSets that are pending (not yet in-flight).
**/
@property (nonatomic, strong, readonly) NSArray *inFlightChangeSets;
@property (nonatomic, strong, readonly) NSArray *pendingChangeSetsFromPreviousCommits;
@property (nonatomic, strong, readonly) NSArray *pendingChangeSetsFromCurrentCommit;

#pragma mark Merge Handling

/**
 * This method enumerates pendingChangeSetsFromPreviousCommits, from oldest commit to newest commit,
 * and merges the changedKeys & values into the given record.
 * Thus, if the value for a particular key has been changed multiple times,
 * then the given record will end up with the most recent value for that key.
 * 
 * The given record is expected to be a sanitized record.
 * 
 * Returns YES if there were any pending records in the pendingChangeSetsFromPreviousCommits.
**/
- (BOOL)mergeChangesForRowid:(NSNumber *)rowidNumber intoRecord:(CKRecord *)record;

#pragma mark Transaction Handling

/**
 * This method updates the current changeSet of the pendingQueue
 * so that the required CloudKit related information can be restored from disk in the event the app is quit.
**/
- (void)updatePendingQueue:(YDBCKChangeQueue *)pendingQueue
         withInsertedRowid:(NSNumber *)rowidNumber
                    record:(CKRecord *)record
        databaseIdentifier:(NSString *)databaseIdentifier;

/**
 * This method properly updates the pendingQueue,
 * including the current changeSet and any previous changeSets (for previous commits) if needed,
 * so that the required CloudKit related information can be restored from disk in the event the app is quit.
**/
- (void)updatePendingQueue:(YDBCKChangeQueue *)pendingQueue
         withModifiedRowid:(NSNumber *)rowidNumber
                    record:(CKRecord *)record
        databaseIdentifier:(NSString *)databaseIdentifier;

/**
 * This method properly updates the pendingQueue,
 * including any previous changeSets (for previous commits) if needed,
 * so that the required CloudKit related information can be restored from disk in the event the app is quit.
**/
- (void)updatePendingQueue:(YDBCKChangeQueue *)pendingQueue
         withDetachedRowid:(NSNumber *)rowidNumber;

/**
 * This method properly updates the pendingQueue,
 * including the current changeSet and any previous changeSets (for previous commits) if needed,
 * so that the required CloudKit related information can be restored from disk in the event the app is quit.
**/
- (void)updatePendingQueue:(YDBCKChangeQueue *)pendingQueue
          withDeletedRowid:(NSNumber *)rowidNumber
                  recordID:(CKRecordID *)ckRecordID
        databaseIdentifier:(NSString *)databaseIdentifier;

/**
 * This method properly updates the pendingQueue,
 * and updates any previous queued changeSets that include modifications for this item.
**/
- (void)updatePendingQueue:(YDBCKChangeQueue *)pendingQueue
           withMergedRowid:(NSNumber *)rowidNumber
                    record:(CKRecord *)mergedRecord
        databaseIdentifier:(NSString *)databaseIdentifier;

/**
 * This method properly updates the pendingQueue,
 * and updates any previously queued changeSets that include modifications for this item.
**/
- (void)updatePendingQueue:(YDBCKChangeQueue *)pendingQueue
    withRemoteDeletedRowid:(NSNumber *)rowidNumber
                  recordID:(CKRecordID *)recordID
        databaseIdentifier:(NSString *)databaseIdentifier;

@end

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * This class represents a row in the queue table.
 * Every row contains the following columns:
 * 
 * - uuid : The unique primary key
 * - prev : The previous row, representing the commit prior to this one (reverse linked-list style)
 *
 * - databaseIdentifier : The databaseIdentifier for all deleted CKRecordIDs & modified CKRecords
 * 
 * - deletedRecordIDs   : A blob of the CKRecordIDs that are to be marked as deleted.
 * - modifiedRecords    : A blob represending the rowid & modified info (either CKRecord or just changedKeys array).
**/
@interface YDBCKChangeSet : NSObject

- (id)initWithUUID:(NSString *)uuid
              prev:(NSString *)prev
databaseIdentifier:(NSString *)databaseIdentifier
  deletedRecordIDs:(NSData *)serializedRecordIDs
   modifiedRecords:(NSData *)serializedModifiedRecords;

@property (nonatomic, strong, readonly) NSString *uuid;
@property (nonatomic, strong, readonly) NSString *prev;

@property (nonatomic, strong, readonly) NSString *databaseIdentifier;

@property (nonatomic, readonly) NSArray *recordIDsToDelete; // Array of CKRecordID's for CKModifyRecordsOperation
@property (nonatomic, readonly) NSArray *recordsToSave;     // Array of CKRecord's for CKModifyRecordsOperation

@property (nonatomic, readonly) BOOL hasChangesToDeletedRecordIDs;
@property (nonatomic, readonly) BOOL hasChangesToModifiedRecords;

- (NSData *)serializeDeletedRecordIDs; // Blob to go in 'deletedRecordIDs' column of database row
- (NSData *)serializeModifiedRecords;  // Blob to go in 'modifiedRecords' column of database row

- (void)enumerateMissingRecordsWithBlock:(CKRecord* (^)(int64_t rowid, NSArray *changedKeys))block;

- (NSDictionary *)recordIDToRowidMapping;

@end